//
// Copyright (C) 2024 Glauco Pacheco <glauco@kourier.io>
// SPDX-License-Identifier: AGPL-3.0-only
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, version 3 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "TlsContext.h"
#include "NoDestroy.h"
#include <forward_list>
#include <cstdio>
#include <openssl/err.h>
#include <openssl/x509_vfy.h>


namespace Kourier
{

static int pemPasswordCallback(char *buf, int size, int rwflag, void *u)
{
    if (!u)
        return -1;
    auto *pTlsConfiguration = (TlsConfiguration*)u;
    const auto &privateKeyPassword = pTlsConfiguration->privateKeyPassword();
    if (privateKeyPassword.empty())
        return -1;
    const auto length = std::min<int>(size, privateKeyPassword.size());
    std::memcpy(buf, privateKeyPassword.c_str(), length);
    return length;
}

static int notResumableSessionCallback(SSL*, int)
{
    return 1;
}

class PassphraseCallbackRestorer
{
public:
    PassphraseCallbackRestorer(SSL_CTX *pSslCtx) :
        m_pSslCtx(pSslCtx)
    {
        if (m_pSslCtx != nullptr)
        {
            m_pCallback = SSL_CTX_get_default_passwd_cb(m_pSslCtx);
            m_pUserData = SSL_CTX_get_default_passwd_cb_userdata(m_pSslCtx);
        }
    }
    ~PassphraseCallbackRestorer()
    {
        if (m_pSslCtx != nullptr)
        {
            SSL_CTX_set_default_passwd_cb(m_pSslCtx, m_pCallback);
            SSL_CTX_set_default_passwd_cb_userdata(m_pSslCtx, m_pUserData);
        }
    }
private:
    SSL_CTX * const m_pSslCtx = nullptr;
    int (*m_pCallback)(char *, int, int, void*) = nullptr;
    void *m_pUserData = nullptr;
};

class DefaultCaCertsCache
{
public:
    DefaultCaCertsCache()
    {
        std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> tlsContext(SSL_CTX_new(TLS_method()), &SSL_CTX_free);
        if (!tlsContext)
            throw RuntimeError("Failed to create OpenSSL context.", RuntimeError::ErrorType::TLS);
        if (SSL_CTX_set_default_verify_paths(tlsContext.get()) != 1)
            throw RuntimeError("Failed to load system certificates.", RuntimeError::ErrorType::TLS);
        //
        // Retrieve added CA Certificates
        //
        auto *pCACertStore = SSL_CTX_get_cert_store(tlsContext.get());
        if (pCACertStore == nullptr)
            throw RuntimeError("Failed to load system certificates.", RuntimeError::ErrorType::TLS);
        auto *pCACerts = X509_STORE_get1_all_certs(pCACertStore);
        if (pCACerts == nullptr)
            throw RuntimeError("Failed to load system certificates.", RuntimeError::ErrorType::TLS);
        const auto caCertsInStore = sk_X509_num(pCACerts);
        m_defaultCaCerts.reserve(caCertsInStore);
        for (auto i = 0; i < caCertsInStore; ++i)
            m_defaultCaCerts.push_back(sk_X509_value(pCACerts, i));
        sk_X509_free(pCACerts);
    }
    ~DefaultCaCertsCache()
    {
        for (auto *pCert : m_defaultCaCerts)
            X509_free(pCert);
    }
    inline const std::vector<X509*> &get() {return m_defaultCaCerts;}

private:
    std::vector<X509*> m_defaultCaCerts;
};

TlsContext::TlsContext(Role role, const TlsConfiguration &tlsConfiguration)
{
    m_pTlsContextData.reset(new TlsContextData(tlsConfiguration, role));
}

TlsContext TlsContext::fromTlsConfiguration(const TlsConfiguration &tlsConfiguration, Role role)
{
    //
    // Search cache first
    //
    static constinit NoDestroy<std::atomic_bool> allThreadsFinished(false);
    static constexpr auto pCleanerFcn = []() -> void {allThreadsFinished().wait(false);};
    static constinit NoDestroy<std::atomic_bool> hasAlreadyRegisteredCleanupFcn(false);
    static constinit NoDestroy<std::atomic_size_t> threadCount(0);
    bool expected = false;
    if (hasAlreadyRegisteredCleanupFcn().compare_exchange_strong(expected, true))
        OPENSSL_atexit(pCleanerFcn);
    struct TlsContextThreadData
    {
        TlsContextThreadData()
        {
            m_pContextCache = new std::forward_list<TlsContext>;
            ++threadCount();
            allThreadsFinished() = false;
        }
        ~TlsContextThreadData()
        {
            delete m_pContextCache;
            OPENSSL_thread_stop();
            if((--threadCount()) == 0)
            {
                allThreadsFinished() = true;
                allThreadsFinished().notify_one();
            }
        }
        std::forward_list<TlsContext> *m_pContextCache;
    };
    static thread_local NoDestroy<TlsContextThreadData*> tlsContextThreadData(new TlsContextThreadData);
    static thread_local NoDestroyPtrDeleter<TlsContextThreadData*> tlsContextThreadDataDeleter(tlsContextThreadData);
    auto *pTlsContextCacheThreadData = tlsContextThreadData();
    if (pTlsContextCacheThreadData != nullptr)
    {
        for (auto &it : *(pTlsContextCacheThreadData->m_pContextCache))
        {
            if (it.role() == role && it.tlsConfiguration() == tlsConfiguration)
                return it;
        }
    }
    //
    // Create context
    //
    TlsContext tlsContext(role, tlsConfiguration);
    PassphraseCallbackRestorer passphraseCallbackRestorer(tlsContext.context());
    SSL_CTX_set_default_passwd_cb(tlsContext.context(), &pemPasswordCallback);
    SSL_CTX_set_default_passwd_cb_userdata(tlsContext.context(), (void*)&tlsConfiguration);
    //
    // Load CA Certificates
    //
    auto *pCaCertStore = SSL_CTX_get_cert_store(tlsContext.context());
    if (pCaCertStore == nullptr) [[unlikely]]
        throw RuntimeError(std::string("Failed to fetch cert store from context."), RuntimeError::ErrorType::TLS);
    if (tlsConfiguration.useSystemCertificates())
    {
        static thread_local NoDestroy<DefaultCaCertsCache*> pDefaultCaCertsCache(new DefaultCaCertsCache);
        static thread_local NoDestroyPtrDeleter<DefaultCaCertsCache*> defaultCaCertsCacheDeleter(pDefaultCaCertsCache);
        auto *pCaCerts = pDefaultCaCertsCache();
        if (pCaCerts != nullptr)
        {
            const auto &caCerts = pCaCerts->get();
            for (const auto &caCert : caCerts)
            {
                if (X509_STORE_add_cert(pCaCertStore, caCert) != 1) [[unlikely]]
                    throw RuntimeError(std::string("Failed to add CA certificate to certificate store."), RuntimeError::ErrorType::TLS);
            }
        }
        else if (SSL_CTX_set_default_verify_paths(tlsContext.context()) != 1)
            throw RuntimeError("Failed to load system certificates.", RuntimeError::ErrorType::TLS);
    }
    for (const auto &caCert : tlsConfiguration.addedCertificates())
    {
        std::unique_ptr<FILE, decltype(&std::fclose)> caCertFileStream(std::fopen(caCert.c_str(), "r"), &std::fclose);
        if (!caCertFileStream)
        {
            throw RuntimeError(std::string("Failed to open CA certificate ")
                                   .append(caCert.c_str())
                                   .append("."), RuntimeError::ErrorType::TLS);
        }
        std::unique_ptr<X509, decltype(&X509_free)> pCaCertificate(nullptr, &X509_free);
        pCaCertificate.reset(PEM_read_X509(caCertFileStream.get(), nullptr, &pemPasswordCallback, nullptr));
        if (!pCaCertificate) [[unlikely]]
        {
            throw RuntimeError(std::string("Failed to load CA certificate ")
                                   .append(caCert.c_str())
                                   .append("."), RuntimeError::ErrorType::TLS);
        }
        if (X509_STORE_add_cert(pCaCertStore, pCaCertificate.get()) != 1) [[unlikely]]
        {
            throw RuntimeError(std::string("Failed to add CA certificate ")
                                   .append(caCert.c_str())
                                   .append(" to certificate store."), RuntimeError::ErrorType::TLS);
        }
    }
    X509_STORE_set_flags(pCaCertStore, X509_V_FLAG_PARTIAL_CHAIN);
    //
    // Load private key
    //
    if (!tlsConfiguration.privateKey().empty())
    {
        if (SSL_CTX_use_PrivateKey_file(tlsContext.context(), tlsConfiguration.privateKey().c_str(), SSL_FILETYPE_PEM) != 1) [[unlikely]]
        {
            throw RuntimeError(std::string("Failed to load private key from ")
                                   .append(tlsConfiguration.privateKey().c_str())
                                   .append("."), RuntimeError::ErrorType::TLS);
        }
    }
    //
    // Load local certificate
    //
    if (!tlsConfiguration.certificate().empty())
    {
        if (SSL_CTX_use_certificate_chain_file(tlsContext.context(), tlsConfiguration.certificate().c_str()) != 1) [[unlikely]]
        {
            throw RuntimeError(std::string("Failed to load certificate chain from ")
                                   .append(tlsConfiguration.certificate().c_str())
                                   .append("."), RuntimeError::ErrorType::TLS);
        }
    }
    //
    // Validate and set local certificate and private key
    //
    if (!tlsConfiguration.privateKey().empty() && !tlsConfiguration.certificate().empty())
    {
        if (SSL_CTX_check_private_key(tlsContext.context()) != 1) [[unlikely]]
        {
            throw RuntimeError(std::string("Failed to validate private key ")
                                   .append(tlsConfiguration.privateKey().c_str())
                                   .append("."), RuntimeError::ErrorType::TLS);
        }
        if (SSL_CTX_build_cert_chain(tlsContext.context(), SSL_BUILD_CHAIN_FLAG_IGNORE_ERROR | SSL_BUILD_CHAIN_FLAG_UNTRUSTED | SSL_BUILD_CHAIN_FLAG_NO_ROOT) != 1) [[unlikely]]
        {
            throw RuntimeError(std::string("Failed to validate certificate chain ")
                                   .append(tlsConfiguration.certificate().c_str())
                                   .append("."), RuntimeError::ErrorType::TLS);
        }
    }
    //
    // Setting cipher list
    //
    std::string tls12CiphersNames;
    std::string tls13CiphersNames;
    const auto &setCiphers = tlsConfiguration.ciphers();
    if (!setCiphers.empty())
    {
        for (auto &cipher : setCiphers)
        {
            switch (cipher)
            {
                case TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256:
                    if (!tls12CiphersNames.empty())
                        tls12CiphersNames.append(":");
                    tls12CiphersNames.append("ECDHE-ECDSA-AES128-GCM-SHA256");
                    break;
                case TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256:
                    if (!tls12CiphersNames.empty())
                        tls12CiphersNames.append(":");
                    tls12CiphersNames.append("ECDHE-ECDSA-CHACHA20-POLY1305");
                    break;
                case TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:
                    if (!tls12CiphersNames.empty())
                        tls12CiphersNames.append(":");
                    tls12CiphersNames.append("ECDHE-RSA-AES128-GCM-SHA256");
                    break;
                case TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256:
                    if (!tls12CiphersNames.empty())
                        tls12CiphersNames.append(":");
                    tls12CiphersNames.append("ECDHE-RSA-CHACHA20-POLY1305");
                    break;
                case TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256:
                    if (!tls12CiphersNames.empty())
                        tls12CiphersNames.append(":");
                    tls12CiphersNames.append("ECDHE-ECDSA-AES128-SHA256");
                    break;
                case TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256:
                    if (!tls12CiphersNames.empty())
                        tls12CiphersNames.append(":");
                    tls12CiphersNames.append("ECDHE-RSA-AES128-SHA256");
                    break;
                case TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384:
                    if (!tls12CiphersNames.empty())
                        tls12CiphersNames.append(":");
                    tls12CiphersNames.append("ECDHE-ECDSA-AES256-GCM-SHA384");
                    break;
                case TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384:
                    if (!tls12CiphersNames.empty())
                        tls12CiphersNames.append(":");
                    tls12CiphersNames.append("ECDHE-ECDSA-AES256-SHA384");
                    break;
                case TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384:
                    if (!tls12CiphersNames.empty())
                        tls12CiphersNames.append(":");
                    tls12CiphersNames.append("ECDHE-RSA-AES256-GCM-SHA384");
                    break;
                case TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384:
                    if (!tls12CiphersNames.empty())
                        tls12CiphersNames.append(":");
                    tls12CiphersNames.append("ECDHE-RSA-AES256-SHA384");
                    break;
                case TlsConfiguration::Cipher::TLS_AES_128_GCM_SHA256:
                    if (!tls13CiphersNames.empty())
                        tls13CiphersNames.append(":");
                    tls13CiphersNames.append("TLS_AES_128_GCM_SHA256");
                    break;
                case TlsConfiguration::Cipher::TLS_AES_256_GCM_SHA384:
                    if (!tls13CiphersNames.empty())
                        tls13CiphersNames.append(":");
                    tls13CiphersNames.append("TLS_AES_256_GCM_SHA384");
                    break;
                case TlsConfiguration::Cipher::TLS_CHACHA20_POLY1305_SHA256:
                    if (!tls13CiphersNames.empty())
                        tls13CiphersNames.append(":");
                    tls13CiphersNames.append("TLS_CHACHA20_POLY1305_SHA256");
                    break;
            }
        }
    }
    if (tls12CiphersNames.empty())
    {
        if (tls13CiphersNames.empty())
            tls13CiphersNames = "TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256";
        tls12CiphersNames = "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-CHACHA20-POLY1305:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-SHA384";
    }
    if (SSL_CTX_set_cipher_list(tlsContext.context(), tls12CiphersNames.c_str()) != 1) [[unlikely]]
        throw RuntimeError(std::string("Failed to set TLS 1.2 ciphers ").append(tls12CiphersNames).append("."), RuntimeError::ErrorType::TLS);
    if (SSL_CTX_set_ciphersuites(tlsContext.context(), !tls13CiphersNames.empty() ? tls13CiphersNames.c_str() : "") != 1) [[unlikely]]
        throw RuntimeError(std::string("Failed to set TLS 1.3 ciphers ").append(tls13CiphersNames).append("."), RuntimeError::ErrorType::TLS);
    //
    // Setting Elliptic Curves
    //
    std::string curvesNames;
    const auto &setCurves = tlsConfiguration.curves();
    if (!setCurves.empty())
    {
        for (auto &curve : setCurves)
        {
            if (!curvesNames.empty())
                curvesNames.append(":");
            switch (curve)
            {
                case TlsConfiguration::Curve::X25519:
                    curvesNames.append("X25519");
                    break;
                case TlsConfiguration::Curve::prime256v1:
                    curvesNames.append("P-256");
                    break;
                case TlsConfiguration::Curve::secp384r1:
                    curvesNames.append("P-384");
                    break;
                case TlsConfiguration::Curve::secp521r1:
                    curvesNames.append("P-521");
                    break;
            }
        }
    }
    else
        curvesNames = "X25519:P-256:P-384:P-521";
    if (SSL_CTX_set1_groups_list(tlsContext.context(), curvesNames.c_str()) != 1) [[unlikely]]
        throw RuntimeError(std::string("Failed to set curves ").append(curvesNames).append("."), RuntimeError::ErrorType::TLS);
    //
    // Configuring TLS protocol
    //
    switch (tlsConfiguration.tlsVersion())
    {
        case TlsConfiguration::TlsVersion::TLS_1_2:
            if (SSL_CTX_set_min_proto_version(tlsContext.context(), TLS1_2_VERSION) != 1)
                throw RuntimeError("Failed to set minimum TLS protocol version to 1.2.", RuntimeError::ErrorType::TLS);
            if (SSL_CTX_set_max_proto_version(tlsContext.context(), TLS1_2_VERSION) != 1)
                throw RuntimeError("Failed to set maximum TLS protocol version to 1.2.", RuntimeError::ErrorType::TLS);
            break;
        case TlsConfiguration::TlsVersion::TLS_1_3:
            if (SSL_CTX_set_min_proto_version(tlsContext.context(), TLS1_3_VERSION) != 1)
                throw RuntimeError("Failed to set minimum TLS protocol version to 1.3.", RuntimeError::ErrorType::TLS);
            if (SSL_CTX_set_max_proto_version(tlsContext.context(), TLS1_3_VERSION) != 1)
                throw RuntimeError("Failed to set maximum TLS protocol version to 1.3.", RuntimeError::ErrorType::TLS);
            break;
        case TlsConfiguration::TlsVersion::TLS_1_3_or_newer:
            if (SSL_CTX_set_min_proto_version(tlsContext.context(), TLS1_3_VERSION) != 1)
                throw RuntimeError("Failed to set minimum TLS protocol version to 1.3.", RuntimeError::ErrorType::TLS);
            if (SSL_CTX_set_max_proto_version(tlsContext.context(), 0) != 1)
                throw RuntimeError("Failed to set maximum TLS protocol version to 1.3 or newer.", RuntimeError::ErrorType::TLS);
            break;
        case TlsConfiguration::TlsVersion::TLS_1_2_or_newer:
            if (SSL_CTX_set_min_proto_version(tlsContext.context(), TLS1_2_VERSION) != 1)
                throw RuntimeError("Failed to set minimum TLS protocol version to 1.2.", RuntimeError::ErrorType::TLS);
            if (SSL_CTX_set_max_proto_version(tlsContext.context(), 0) != 1)
                throw RuntimeError("Failed to set maximum TLS protocol version to 1.2 or newer.", RuntimeError::ErrorType::TLS);
            break;
    }
    SSL_CTX_set_session_cache_mode(tlsContext.context(), SSL_SESS_CACHE_OFF | SSL_SESS_CACHE_NO_INTERNAL);
    if (SSL_CTX_set_num_tickets(tlsContext.context(), 0) != 1) [[unlikely]]
        throw RuntimeError("Failed to disable sending session tickets on connections using TLS 1.3.", RuntimeError::ErrorType::TLS);
    SSL_CTX_set_options(tlsContext.context(), SSL_OP_NO_COMPRESSION | SSL_OP_NO_RENEGOTIATION | SSL_OP_NO_TICKET | SSL_OP_CIPHER_SERVER_PREFERENCE);
    SSL_CTX_set_num_tickets(tlsContext.context(), 0);
    SSL_CTX_set_mode(tlsContext.context(), SSL_MODE_AUTO_RETRY | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER | SSL_MODE_RELEASE_BUFFERS);
    SSL_CTX_set_read_ahead(tlsContext.context(), 0);
    SSL_CTX_set_dh_auto(tlsContext.context(), 1);
    SSL_CTX_set_not_resumable_session_callback(tlsContext.context(), &notResumableSessionCallback);
    //
    // Configuring Peer Verify Mode
    //
    switch (role)
    {
        case TlsContext::Role::Client:
            switch (tlsConfiguration.peerVerifyMode())
            {
                case TlsConfiguration::Auto:
                case TlsConfiguration::On:
                    SSL_CTX_set_verify(tlsContext.context(), SSL_VERIFY_PEER, nullptr);
                    break;
                case TlsConfiguration::Off:
                    SSL_CTX_set_verify(tlsContext.context(), SSL_VERIFY_NONE, nullptr);
                    break;
            }
            break;
        case TlsContext::Role::Server:
            switch (tlsConfiguration.peerVerifyMode())
            {
                case TlsConfiguration::On:
                {
                    SSL_CTX_set_verify(tlsContext.context(), SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
                    STACK_OF(X509_NAME) *pStackOfCACerts = nullptr;

                    for (const auto &caCert : tlsConfiguration.addedCertificates())
                    {
                        if (pStackOfCACerts == nullptr)
                        {
                            pStackOfCACerts = SSL_load_client_CA_file(caCert.c_str());
                            if (pStackOfCACerts == nullptr)
                            {
                                throw RuntimeError(std::string("Failed to load client CA file ")
                                                       .append(caCert.c_str())
                                                       .append("."), RuntimeError::ErrorType::TLS);
                            }
                        }
                        else if (SSL_add_file_cert_subjects_to_stack(pStackOfCACerts, caCert.c_str()) != 1)
                        {
                            if (pStackOfCACerts != nullptr)
                                sk_X509_NAME_pop_free(pStackOfCACerts, X509_NAME_free);
                            throw RuntimeError(std::string("Failed to load client CA file ")
                                                   .append(caCert.c_str())
                                                   .append("."), RuntimeError::ErrorType::TLS);
                        }
                    }
                    if (pStackOfCACerts != nullptr)
                        SSL_CTX_set_client_CA_list(tlsContext.context(), pStackOfCACerts);
                    break;
                }
                case TlsConfiguration::Off:
                case TlsConfiguration::Auto:
                    SSL_CTX_set_verify(tlsContext.context(), SSL_VERIFY_NONE, nullptr);
                    break;
            }
            break;
    }
    //
    // Configuring Peer Verify Depth
    //
    const int peerVerifyDepth = std::min<int>(65535, std::max<int>(0, tlsConfiguration.peerVerifyDepth()));
    SSL_CTX_set_verify_depth(tlsContext.context(), peerVerifyDepth ? peerVerifyDepth : 65536);
    //
    // Store context in cache
    //
    if (pTlsContextCacheThreadData != nullptr)
        pTlsContextCacheThreadData->m_pContextCache->push_front(tlsContext);
    return tlsContext;
}

std::pair<bool, std::string> TlsContext::validateTlsConfiguration(const TlsConfiguration &tlsConfiguration, Role role)
{
    auto response = std::make_pair(true, std::string{});
    try
    {
        auto tlsContext = TlsContext::fromTlsConfiguration(tlsConfiguration, role);
    }
    catch (const RuntimeError &runtimeError)
    {
        response.first = false;
        response.second = runtimeError.error();
        if (response.second.empty())
            response.second = "Failed to create TLS context from TlsConfiguration. Unknown TLS error.";
    }
    return response;
}

}
