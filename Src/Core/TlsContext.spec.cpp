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
#include "TlsConfiguration.h"
#include <Tests/Resources/TlsTestCertificates.h>
#include <Spectator.h>
#include <string>
#include <set>


using Kourier::TlsContext;
using Kourier::TlsConfiguration;
using Kourier::TestResources::TlsTestCertificateInfo;
using Kourier::TestResources::TlsTestCertificates;

struct SslCtxConfiguration
{
    std::string certificateContents;
    std::string privateKeyContents;
    TlsConfiguration::TlsVersion tlsVersion;
    std::set<TlsConfiguration::Cipher> ciphers;
    std::set<TlsConfiguration::Curve> curves;
    std::set<std::string> addedCACertificates;
    std::set<std::string> caClientList;
    int peerVerifyDepth = 0;
    bool useSystemCertificates = true;
    int verifyMode = -1;
};

static void getSslCtxConfiguration(SslCtxConfiguration &sslCtxConfiguration, SSL_CTX *pCTX);


SCENARIO("TlsContext is setup according to given certificate pair configuration")
{
    GIVEN("a tls configuration containing a certificate pair")
    {
        const auto role = GENERATE(AS(TlsContext::Role),
                                   TlsContext::Role::Client,
                                   TlsContext::Role::Server);
        const auto certificateType = GENERATE(AS(TlsTestCertificates::CertificateType),
                                              TlsTestCertificates::CertificateType::RSA_2048,
                                              TlsTestCertificates::CertificateType::RSA_2048_CHAIN,
                                              TlsTestCertificates::CertificateType::RSA_2048_EncryptedPrivateKey,
                                              TlsTestCertificates::CertificateType::RSA_2048_CHAIN_EncryptedPrivateKey,
                                              TlsTestCertificates::CertificateType::ECDSA,
                                              TlsTestCertificates::CertificateType::ECDSA_CHAIN,
                                              TlsTestCertificates::CertificateType::ECDSA_EncryptedPrivateKey,
                                              TlsTestCertificates::CertificateType::ECDSA_CHAIN_EncryptedPrivateKey);
        std::string certificateFile;
        std::string privateKeyFile;
        std::string caCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
        std::string certificateContents;
        std::string privateKeyContents;
        std::string privateKeyPassword;
        std::string caCertificateContents;
        TlsTestCertificates::getContentsFromCertificateType(certificateType, certificateContents, privateKeyContents, privateKeyPassword, caCertificateContents);
        TlsConfiguration tlsConfiguration;
        tlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile, privateKeyPassword);
        tlsConfiguration.addCaCertificate(caCertificateFile);

        WHEN("tls context is fetched for given configuration and role")
        {
            TlsContext tlsContext;
            try
            {
                tlsContext = TlsContext::fromTlsConfiguration(tlsConfiguration, role);
            }
            catch (Kourier::RuntimeError &error)
            {
                FAIL(error.error().data());
            }
            REQUIRE(tlsContext.context() != nullptr);

            THEN("fetched tls context was setup according to given tls configuration")
            {
                SslCtxConfiguration sslCtxConfiguration;
                getSslCtxConfiguration(sslCtxConfiguration, tlsContext.context());
                REQUIRE(sslCtxConfiguration.certificateContents == certificateContents);
                REQUIRE(sslCtxConfiguration.privateKeyContents == privateKeyContents);

                AND_WHEN("another context is created from given configuration")
                {
                    THEN("previous context is given from cache")
                    {
                        REQUIRE(TlsContext::fromTlsConfiguration(tlsConfiguration, role).context() == tlsContext.context());
                    }
                }
            }
        }
    }
}


SCENARIO("TlsContext is setup according to given CA certificates tls configuration")
{
    GIVEN("a tls configuration containing CA certificates")
    {
        const auto role = GENERATE(AS(TlsContext::Role),
                                   TlsContext::Role::Client,
                                   TlsContext::Role::Server);
        const auto peerVerifyMode = GENERATE(AS(TlsConfiguration::PeerVerifyMode),
                                             TlsConfiguration::PeerVerifyMode::On,
                                             TlsConfiguration::PeerVerifyMode::Off,
                                             TlsConfiguration::PeerVerifyMode::Auto);
        const auto caCertificateTypes = GENERATE(AS(std::set<TlsTestCertificates::CertificateType>),
                                                 {TlsTestCertificates::CertificateType::RSA_2048},
                                                 {TlsTestCertificates::CertificateType::RSA_2048_CHAIN},
                                                 {TlsTestCertificates::CertificateType::RSA_2048_EncryptedPrivateKey},
                                                 {TlsTestCertificates::CertificateType::RSA_2048_CHAIN_EncryptedPrivateKey},
                                                 {TlsTestCertificates::CertificateType::ECDSA},
                                                 {TlsTestCertificates::CertificateType::ECDSA_CHAIN},
                                                 {TlsTestCertificates::CertificateType::ECDSA_EncryptedPrivateKey},
                                                 {TlsTestCertificates::CertificateType::ECDSA_CHAIN_EncryptedPrivateKey},
                                                 {TlsTestCertificates::CertificateType::RSA_2048,
                                                  TlsTestCertificates::CertificateType::RSA_2048_CHAIN},
                                                 {TlsTestCertificates::CertificateType::RSA_2048_CHAIN_EncryptedPrivateKey,
                                                  TlsTestCertificates::CertificateType::ECDSA},
                                                 {TlsTestCertificates::CertificateType::RSA_2048,
                                                  TlsTestCertificates::CertificateType::RSA_2048_CHAIN,
                                                  TlsTestCertificates::CertificateType::RSA_2048_EncryptedPrivateKey,
                                                  TlsTestCertificates::CertificateType::RSA_2048_CHAIN_EncryptedPrivateKey,
                                                  TlsTestCertificates::CertificateType::ECDSA,
                                                  TlsTestCertificates::CertificateType::ECDSA_CHAIN,
                                                  TlsTestCertificates::CertificateType::ECDSA_EncryptedPrivateKey,
                                                  TlsTestCertificates::CertificateType::ECDSA_CHAIN_EncryptedPrivateKey});
        const auto useSystemCertificates = GENERATE(AS(bool), true, false);
        const auto addInsteadOfSettingCACerts = GENERATE(AS(bool), true, false);
        std::set<std::string> caCertsFilePath;
        std::set<std::string> caCertsContents;
        for (auto caCertType : caCertificateTypes)
        {
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(caCertType, certificateFile, privateKeyFile, caCertificateFile);
            caCertsFilePath.insert(caCertificateFile);
            std::string certificateContents;
            std::string privateKeyContents;
            std::string privateKeyPassword;
            std::string caCertificateContents;
            TlsTestCertificates::getContentsFromCertificateType(caCertType, certificateContents, privateKeyContents, privateKeyPassword, caCertificateContents);
            caCertsContents.insert(caCertificateContents);
        }
        std::set<std::string> clientCaList;
        if (role == TlsContext::Role::Server && peerVerifyMode == TlsConfiguration::PeerVerifyMode::On)
        {
            for (const auto &caCert : caCertsContents)
            {
                auto *pBIO = BIO_new(BIO_s_mem());
                BIO_write(pBIO, caCert.data(), caCert.size());
                auto *pCert = PEM_read_bio_X509(pBIO, nullptr, nullptr, nullptr);
                REQUIRE(pCert != nullptr);
                auto *pString = X509_NAME_oneline(X509_get_issuer_name(pCert), NULL, 0);
                REQUIRE(pString != nullptr);
                clientCaList.insert(std::string(pString));
                free(pString);
                X509_free(pCert);
                BIO_free(pBIO);
            }
        }
        TlsConfiguration tlsConfiguration;
        if (addInsteadOfSettingCACerts)
        {
            for (const auto &caCert : caCertsFilePath)
                tlsConfiguration.addCaCertificate(caCert);
        }
        else
            tlsConfiguration.setCaCertificates(caCertsFilePath);
        tlsConfiguration.setUseSystemCertificates(useSystemCertificates);
        tlsConfiguration.setPeerVerifyMode(peerVerifyMode);
        if (useSystemCertificates)
        {
            static const auto defaultCaCerts = []() -> std::set<std::string>
            {
                auto *pCtx = SSL_CTX_new(TLS_method());
                REQUIRE(pCtx != nullptr);
                REQUIRE(1 == SSL_CTX_set_default_verify_paths(pCtx));
                auto *pCACertStore = SSL_CTX_get_cert_store(pCtx);
                REQUIRE(pCACertStore != nullptr);
                auto *pCACerts = X509_STORE_get1_all_certs(pCACertStore);
                REQUIRE(pCACerts != nullptr);
                const auto caCertsInStore = sk_X509_num(pCACerts);
                REQUIRE(caCertsInStore > 0);
                std::set<std::string> caCerts;
                for (auto i = 0; i < caCertsInStore; ++i)
                {
                    auto *pCACertInChain = sk_X509_value(pCACerts, i);
                    auto *pBIO = BIO_new(BIO_s_mem());
                    REQUIRE(pBIO != nullptr);
                    REQUIRE(1 == PEM_write_bio_X509(pBIO, pCACertInChain));
                    const auto caCertContentsSize = BIO_ctrl_pending(pBIO);
                    REQUIRE(caCertContentsSize > 0);
                    std::string caCertInChainContents(caCertContentsSize, ' ');
                    BIO_read(pBIO, caCertInChainContents.data(), caCertInChainContents.size());
                    BIO_free(pBIO);
                    caCerts.insert(caCertInChainContents);
                }
                sk_X509_pop_free(pCACerts, X509_free);
                SSL_CTX_free(pCtx);
                return caCerts;
            }();
            for (auto &defaultCACert : defaultCaCerts)
                caCertsContents.insert(defaultCACert);
        }

        WHEN("tls context is fetched for given configuration and role")
        {
            TlsContext tlsContext;
            try
            {
                tlsContext = TlsContext::fromTlsConfiguration(tlsConfiguration, role);
            }
            catch (Kourier::RuntimeError &error)
            {
                FAIL(error.error().data());
            }
            REQUIRE(tlsContext.context() != nullptr);

            THEN("fetched tls context was setup according to given tls configuration")
            {
                SslCtxConfiguration sslCtxConfiguration;
                getSslCtxConfiguration(sslCtxConfiguration, tlsContext.context());
                REQUIRE(sslCtxConfiguration.addedCACertificates == caCertsContents);
                REQUIRE(sslCtxConfiguration.caClientList == clientCaList);

                AND_WHEN("another context is created from given configuration")
                {
                    THEN("previous context is given from cache")
                    {
                        REQUIRE(TlsContext::fromTlsConfiguration(tlsConfiguration, role).context() == tlsContext.context());
                    }
                }
            }
        }
    }
}


SCENARIO("TlsContext is setup according to given tls version")
{
    GIVEN("a tls configuration containing a tls version")
    {
        const auto role = GENERATE(AS(TlsContext::Role),
                                   TlsContext::Role::Client,
                                   TlsContext::Role::Server);
        const auto tlsVersion = GENERATE(AS(TlsConfiguration::TlsVersion),
                                         TlsConfiguration::TlsVersion::TLS_1_2,
                                         TlsConfiguration::TlsVersion::TLS_1_2_or_newer,
                                         TlsConfiguration::TlsVersion::TLS_1_3,
                                         TlsConfiguration::TlsVersion::TLS_1_3_or_newer);
        TlsConfiguration tlsConfiguration;
        tlsConfiguration.setTlsVersion(tlsVersion);

        WHEN("tls context is fetched for given configuration and role")
        {
            TlsContext tlsContext;
            try
            {
                tlsContext = TlsContext::fromTlsConfiguration(tlsConfiguration, role);
            }
            catch (Kourier::RuntimeError &error)
            {
                FAIL(error.error().data());
            }
            REQUIRE(tlsContext.context() != nullptr);

            THEN("fetched tls context was setup according to given tls configuration")
            {
                SslCtxConfiguration sslCtxConfiguration;
                getSslCtxConfiguration(sslCtxConfiguration, tlsContext.context());
                REQUIRE(sslCtxConfiguration.tlsVersion == tlsVersion);

                AND_WHEN("another context is created from given configuration")
                {
                    THEN("previous context is given from cache")
                    {
                        REQUIRE(TlsContext::fromTlsConfiguration(tlsConfiguration, role).context() == tlsContext.context());
                    }
                }
            }
        }
    }
}


SCENARIO("TlsContext is setup according to given ciphers")
{
    GIVEN("a tls configuration containing a set ciphers")
    {
        const auto role = GENERATE(AS(TlsContext::Role),
                                   TlsContext::Role::Client,
                                   TlsContext::Role::Server);
        const auto ciphers = GENERATE(AS(std::set<TlsConfiguration::Cipher>),
                                      {},
                                      {TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256},
                                      {TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256},
                                      {TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256},
                                      {TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256},
                                      {TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256},
                                      {TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256},
                                      {TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384},
                                      {TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384},
                                      {TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384},
                                      {TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384},
                                      {TlsConfiguration::Cipher::TLS_AES_128_GCM_SHA256},
                                      {TlsConfiguration::Cipher::TLS_AES_256_GCM_SHA384},
                                      {TlsConfiguration::Cipher::TLS_CHACHA20_POLY1305_SHA256},
                                      {TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
                                       TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
                                       TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
                                       TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256},
                                      {TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
                                       TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
                                       TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
                                       TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
                                       TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
                                       TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
                                       TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
                                       TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
                                       TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
                                       TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
                                       TlsConfiguration::Cipher::TLS_AES_128_GCM_SHA256,
                                       TlsConfiguration::Cipher::TLS_AES_256_GCM_SHA384,
                                       TlsConfiguration::Cipher::TLS_CHACHA20_POLY1305_SHA256});
        TlsConfiguration tlsConfiguration;
        tlsConfiguration.setCiphers(ciphers);

        WHEN("tls context is fetched for given configuration and role")
        {
            TlsContext tlsContext;
            try
            {
                tlsContext = TlsContext::fromTlsConfiguration(tlsConfiguration, role);
            }
            catch (Kourier::RuntimeError &error)
            {
                FAIL(error.error().data());
            }
            REQUIRE(tlsContext.context() != nullptr);

            THEN("fetched tls context was setup according to given tls configuration")
            {
                SslCtxConfiguration sslCtxConfiguration;
                getSslCtxConfiguration(sslCtxConfiguration, tlsContext.context());
                static const std::set<TlsConfiguration::Cipher> allCiphers({TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
                                                                            TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
                                                                            TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
                                                                            TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
                                                                            TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
                                                                            TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
                                                                            TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
                                                                            TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
                                                                            TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
                                                                            TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
                                                                            TlsConfiguration::Cipher::TLS_AES_128_GCM_SHA256,
                                                                            TlsConfiguration::Cipher::TLS_AES_256_GCM_SHA384,
                                                                            TlsConfiguration::Cipher::TLS_CHACHA20_POLY1305_SHA256});
                const std::set<TlsConfiguration::Cipher> expectedCiphers = [ciphers]()
                {
                    if (ciphers.empty())
                        return allCiphers;
                    bool hasTls12CipherInList = false;
                    for (auto &cipher : ciphers)
                    {
                        switch (cipher)
                        {
                            case TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256:
                                hasTls12CipherInList = true;
                                break;
                            case TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256:
                                hasTls12CipherInList = true;
                                break;
                            case TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:
                                hasTls12CipherInList = true;
                                break;
                            case TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256:
                                hasTls12CipherInList = true;
                                break;
                            case TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256:
                                hasTls12CipherInList = true;
                                break;
                            case TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256:
                                hasTls12CipherInList = true;
                                break;
                            case TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384:
                                hasTls12CipherInList = true;
                                break;
                            case TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384:
                                hasTls12CipherInList = true;
                                break;
                            case TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384:
                                hasTls12CipherInList = true;
                                break;
                            case TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384:
                                hasTls12CipherInList = true;
                                break;
                            case TlsConfiguration::Cipher::TLS_AES_128_GCM_SHA256:
                                break;
                            case TlsConfiguration::Cipher::TLS_AES_256_GCM_SHA384:
                                break;
                            case TlsConfiguration::Cipher::TLS_CHACHA20_POLY1305_SHA256:
                                break;
                        }
                        if (hasTls12CipherInList)
                            break;
                    }
                    if (hasTls12CipherInList)
                        return ciphers;
                    else
                    {
                        std::set<TlsConfiguration::Cipher> ciphersWithTls12Ciphers(ciphers);
                        ciphersWithTls12Ciphers.insert({TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
                                                        TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
                                                        TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
                                                        TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
                                                        TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
                                                        TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
                                                        TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
                                                        TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
                                                        TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
                                                        TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384});
                        return ciphersWithTls12Ciphers;
                    }
                }();
                REQUIRE(sslCtxConfiguration.ciphers == expectedCiphers);

                AND_WHEN("another context is created from given configuration")
                {
                    THEN("previous context is given from cache")
                    {
                        REQUIRE(TlsContext::fromTlsConfiguration(tlsConfiguration, role).context() == tlsContext.context());
                    }
                }
            }
        }
    }
}


SCENARIO("TlsContext is setup according to peer verify depth")
{
    GIVEN("a tls configuration containing a peer verify depth")
    {
        const auto role = GENERATE(AS(TlsContext::Role),
                                   TlsContext::Role::Client,
                                   TlsContext::Role::Server);
        const auto peerVerifyDepth = GENERATE(AS(int), 1024, 256);
        TlsConfiguration tlsConfiguration;
        tlsConfiguration.setPeerVerifyDepth(peerVerifyDepth);

        WHEN("tls context is fetched for given configuration and role")
        {
            TlsContext tlsContext;
            try
            {
                tlsContext = TlsContext::fromTlsConfiguration(tlsConfiguration, role);
            }
            catch (Kourier::RuntimeError &error)
            {
                FAIL(error.error().data());
            }
            REQUIRE(tlsContext.context() != nullptr);

            THEN("fetched tls context was setup according to given tls configuration")
            {
                SslCtxConfiguration sslCtxConfiguration;
                getSslCtxConfiguration(sslCtxConfiguration, tlsContext.context());
                REQUIRE(sslCtxConfiguration.peerVerifyDepth == peerVerifyDepth);

                AND_WHEN("another context is created from given configuration")
                {
                    THEN("previous context is given from cache")
                    {
                        REQUIRE(TlsContext::fromTlsConfiguration(tlsConfiguration, role).context() == tlsContext.context());
                    }
                }
            }
        }
    }
}


SCENARIO("TlsContext is setup according to peer verify mode")
{
    GIVEN("a tls configuration containing a peer verify mode")
    {
        const auto role = GENERATE(AS(TlsContext::Role),
                                   TlsContext::Role::Client,
                                   TlsContext::Role::Server);
        const auto peerVerifyMode = GENERATE(AS(TlsConfiguration::PeerVerifyMode),
                                             TlsConfiguration::PeerVerifyMode::On,
                                             TlsConfiguration::PeerVerifyMode::Off,
                                             TlsConfiguration::PeerVerifyMode::Auto);
        TlsConfiguration tlsConfiguration;
        tlsConfiguration.setPeerVerifyMode(peerVerifyMode);

        WHEN("tls context is fetched for given configuration and role")
        {
            TlsContext tlsContext;
            try
            {
                tlsContext = TlsContext::fromTlsConfiguration(tlsConfiguration, role);
            }
            catch (Kourier::RuntimeError &error)
            {
                FAIL(error.error().data());
            }
            REQUIRE(tlsContext.context() != nullptr);

            THEN("fetched tls context was setup according to given tls configuration")
            {
                SslCtxConfiguration sslCtxConfiguration;
                getSslCtxConfiguration(sslCtxConfiguration, tlsContext.context());
                int verifyFlag = -1;
                switch (role)
                {
                    case TlsContext::Role::Client:
                    {
                        switch (peerVerifyMode)
                        {
                            case Kourier::TlsConfiguration::Auto:
                            case Kourier::TlsConfiguration::On:
                                verifyFlag = SSL_VERIFY_PEER;
                                break;
                            case Kourier::TlsConfiguration::Off:
                                verifyFlag = SSL_VERIFY_NONE;
                                break;
                            default:
                            {
                                FAIL("This code is supposed to be unreachable.");
                            }
                        }
                        break;
                    }
                    case TlsContext::Role::Server:
                        switch (peerVerifyMode)
                        {
                            case Kourier::TlsConfiguration::On:
                            {
                                verifyFlag = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
                                break;
                            }
                            case Kourier::TlsConfiguration::Auto:
                            case Kourier::TlsConfiguration::Off:
                                verifyFlag = SSL_VERIFY_NONE;
                                break;
                            default:
                            {
                                FAIL("This code is supposed to be unreachable.");
                            }
                        }
                        break;
                    default:
                    {
                        FAIL("This code is supposed to be unreachable.");
                    }
                }
                REQUIRE(sslCtxConfiguration.verifyMode == verifyFlag);

                AND_WHEN("another context is created from given configuration")
                {
                    THEN("previous context is given from cache")
                    {
                        REQUIRE(TlsContext::fromTlsConfiguration(tlsConfiguration, role).context() == tlsContext.context());
                    }
                }
            }
        }
    }
}


static void getSslCtxConfiguration(SslCtxConfiguration &sslCtxConfiguration, SSL_CTX *pCTX)
{
    REQUIRE(pCTX != nullptr);
    //
    // Retrieve private key
    //
    auto *pKey = SSL_CTX_get0_privatekey(pCTX);
    auto *pBIO = BIO_new(BIO_s_mem());
    if (pKey != nullptr)
    {
        REQUIRE(pBIO != nullptr);
        if (EVP_PKEY_get_base_id(pKey) != EVP_PKEY_EC)
        {
            REQUIRE(1 == PEM_write_bio_PrivateKey(pBIO, pKey, nullptr, nullptr, 0, nullptr, nullptr));
        }
        else
        {
            REQUIRE(1 == PEM_write_bio_PrivateKey_traditional(pBIO, pKey, nullptr, nullptr, 0, nullptr, nullptr));
        }
        const auto privateKeyContentsSize = BIO_ctrl_pending(pBIO);
        REQUIRE(privateKeyContentsSize > 0);
        sslCtxConfiguration.privateKeyContents.resize(privateKeyContentsSize);
        BIO_read(pBIO, sslCtxConfiguration.privateKeyContents.data(), sslCtxConfiguration.privateKeyContents.size());
    }
    if (pBIO)
    {
        BIO_free(pBIO);
        pBIO = nullptr;
    }
    //
    // Retrieve certificate chain
    //
    auto *pCertificate = SSL_CTX_get0_certificate(pCTX);
    if (pCertificate != nullptr)
    {
        pBIO = BIO_new(BIO_s_mem());
        REQUIRE(pBIO != nullptr);
        REQUIRE(PEM_write_bio_X509(pBIO, pCertificate) == 1);
        const auto certContentsSize = BIO_ctrl_pending(pBIO);
        REQUIRE(certContentsSize > 0);
        std::string certInChainContents(certContentsSize, ' ');
        BIO_read(pBIO, certInChainContents.data(), certInChainContents.size());
        BIO_free(pBIO);
        sslCtxConfiguration.certificateContents = certInChainContents;
        STACK_OF(X509) *pCertChainStack = nullptr;
        REQUIRE(SSL_CTX_get0_chain_certs(pCTX, &pCertChainStack) == 1);
        if (pCertChainStack != nullptr)
        {
            const auto certsInChain = sk_X509_num(pCertChainStack);
            for (auto i = 0; i < certsInChain; ++i)
            {
                auto *pCertInChain = sk_X509_value(pCertChainStack, i);
                pBIO = BIO_new(BIO_s_mem());
                REQUIRE(pBIO != nullptr);
                REQUIRE(PEM_write_bio_X509(pBIO, pCertInChain) == 1);
                const auto certContentsSize = BIO_ctrl_pending(pBIO);
                REQUIRE(certContentsSize > 0);
                std::string certInChainContents(certContentsSize, ' ');
                BIO_read(pBIO, certInChainContents.data(), certInChainContents.size());
                BIO_free(pBIO);
                if (!sslCtxConfiguration.certificateContents.empty() && sslCtxConfiguration.certificateContents.back() != '\n')
                    sslCtxConfiguration.certificateContents.append("\n");
                sslCtxConfiguration.certificateContents.append(certInChainContents);
            }
        }
    }
    //
    // Retrieve added CA Certificates
    //
    auto *pCACertStore = SSL_CTX_get_cert_store(pCTX);
    REQUIRE(pCACertStore != nullptr);
    auto *pCACerts = X509_STORE_get1_all_certs(pCACertStore);
    REQUIRE(pCACerts != nullptr);
    const auto caCertsInStore = sk_X509_num(pCACerts);
    REQUIRE(caCertsInStore > 0);
    for (auto i = 0; i < caCertsInStore; ++i)
    {
        auto *pCACertInChain = sk_X509_value(pCACerts, i);
        pBIO = BIO_new(BIO_s_mem());
        REQUIRE(pBIO != nullptr);
        REQUIRE(1 == PEM_write_bio_X509(pBIO, pCACertInChain));
        const auto caCertContentsSize = BIO_ctrl_pending(pBIO);
        REQUIRE(caCertContentsSize > 0);
        std::string caCertInChainContents(caCertContentsSize, ' ');
        BIO_read(pBIO, caCertInChainContents.data(), caCertInChainContents.size());
        BIO_free(pBIO);
        sslCtxConfiguration.addedCACertificates.insert(caCertInChainContents);
    }
    sk_X509_pop_free(pCACerts, X509_free);
    //
    // Retrieve Ciphers
    //
    auto *pCiphers = SSL_CTX_get_ciphers(pCTX);
    REQUIRE(pCiphers != nullptr);
    const auto cipherCount = sk_SSL_CIPHER_num(pCiphers);
    for (auto i = 0; i < cipherCount; ++i)
    {
        auto *pCipher = sk_SSL_CIPHER_value(pCiphers, i);
        REQUIRE(pCipher != nullptr);
        std::string_view cipherName(SSL_CIPHER_get_name(pCipher));
        if (cipherName == "ECDHE-ECDSA-AES128-GCM-SHA256")
            sslCtxConfiguration.ciphers.insert(TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256);
        else if (cipherName == "ECDHE-ECDSA-CHACHA20-POLY1305")
            sslCtxConfiguration.ciphers.insert(TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256);
        else if (cipherName == "ECDHE-RSA-AES128-GCM-SHA256")
            sslCtxConfiguration.ciphers.insert(TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256);
        else if (cipherName == "ECDHE-RSA-CHACHA20-POLY1305")
            sslCtxConfiguration.ciphers.insert(TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256);
        else if (cipherName == "ECDHE-ECDSA-AES128-SHA256")
            sslCtxConfiguration.ciphers.insert(TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256);
        else if (cipherName == "ECDHE-RSA-AES128-SHA256")
            sslCtxConfiguration.ciphers.insert(TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256);
        else if (cipherName == "ECDHE-ECDSA-AES256-GCM-SHA384")
            sslCtxConfiguration.ciphers.insert(TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384);
        else if (cipherName == "ECDHE-ECDSA-AES256-SHA384")
            sslCtxConfiguration.ciphers.insert(TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384);
        else if (cipherName == "ECDHE-RSA-AES256-GCM-SHA384")
            sslCtxConfiguration.ciphers.insert(TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384);
        else if (cipherName == "ECDHE-RSA-AES256-SHA384")
            sslCtxConfiguration.ciphers.insert(TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384);
        else if (cipherName == "TLS_AES_128_GCM_SHA256")
            sslCtxConfiguration.ciphers.insert(TlsConfiguration::Cipher::TLS_AES_128_GCM_SHA256);
        else if (cipherName == "TLS_AES_256_GCM_SHA384")
            sslCtxConfiguration.ciphers.insert(TlsConfiguration::Cipher::TLS_AES_256_GCM_SHA384);
        else if (cipherName == "TLS_CHACHA20_POLY1305_SHA256")
            sslCtxConfiguration.ciphers.insert(TlsConfiguration::Cipher::TLS_CHACHA20_POLY1305_SHA256);
        else
        {
            FAIL(std::string("Cipher name ").append(cipherName).append(" is not supported.").data());
        }
    }
    //
    // Retrieve curves
    //

    // std::unique_ptr<SSL, decltype(&SSL_free)> pSSL(nullptr, &SSL_free);
    // pSSL.reset(SSL_new(pCTX));
    // REQUIRE(pSSL.get() != nullptr);
    // const auto sharedGroupCount = SSL_get_shared_group(pSSL.get(), -1);
    // REQUIRE(sharedGroupCount > 0);
    // std::vector<int> groups(sharedGroupCount);
    // for (auto i = 0; i < sharedGroupCount; ++i)
    // {
    //     const auto groupId = SSL_get_shared_group(pSSL.get(), i);
    //     switch (groupId)
    //     {
    //         case NID_X9_62_prime256v1:
    //             sslCtxConfiguration.curves.insert(TlsConfiguration::Curve::prime256v1);
    //             break;
    //         case NID_secp384r1:
    //             sslCtxConfiguration.curves.insert(TlsConfiguration::Curve::secp384r1);
    //             break;
    //         case NID_secp521r1:
    //             sslCtxConfiguration.curves.insert(TlsConfiguration::Curve::secp521r1);
    //             break;
    //         case NID_X25519:
    //             sslCtxConfiguration.curves.insert(TlsConfiguration::Curve::X25519);
    //             break;
    //         default:
    //         {
    //             FAIL("Failed to retrieve curves. Unsupported shared group.");
    //         }
    //     }
    // }
    //
    // Retrieve TLS version
    //
    const auto minTlsVersion = SSL_CTX_get_min_proto_version(pCTX);
    const auto maxTlsVersion = SSL_CTX_get_max_proto_version(pCTX);
    if (minTlsVersion == TLS1_2_VERSION && maxTlsVersion == TLS1_2_VERSION)
        sslCtxConfiguration.tlsVersion = TlsConfiguration::TlsVersion::TLS_1_2;
    else if (minTlsVersion == TLS1_3_VERSION && maxTlsVersion == TLS1_3_VERSION)
        sslCtxConfiguration.tlsVersion = TlsConfiguration::TlsVersion::TLS_1_3;
    else if (minTlsVersion == TLS1_3_VERSION && maxTlsVersion == 0)
        sslCtxConfiguration.tlsVersion = TlsConfiguration::TlsVersion::TLS_1_3_or_newer;
    else if (minTlsVersion == TLS1_2_VERSION && maxTlsVersion == 0)
        sslCtxConfiguration.tlsVersion = TlsConfiguration::TlsVersion::TLS_1_2_or_newer;
    else
    {
        FAIL("This code is supposed to be unreachable");
    }
    sslCtxConfiguration.verifyMode = SSL_CTX_get_verify_mode(pCTX);
    sslCtxConfiguration.peerVerifyDepth = SSL_CTX_get_verify_depth(pCTX);
    //
    // Retrieve Client CA List
    //
    auto *pClientCAList = SSL_CTX_get_client_CA_list(pCTX);
    if (pClientCAList != nullptr)
    {
        const auto certsInClientCAList = sk_X509_NAME_num(pClientCAList);
        for (auto i = 0; i < certsInClientCAList; ++i)
        {
            auto *pString = X509_NAME_oneline(sk_X509_NAME_value(pClientCAList, i), nullptr, 0);
            REQUIRE(pString != nullptr);
            sslCtxConfiguration.caClientList.insert(std::string(pString));
            free(pString);
        }
    }
}
