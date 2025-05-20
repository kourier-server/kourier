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

#include "TlsConfiguration.h"
#include <QSharedData>


namespace Kourier
{

/*!
\class Kourier::TlsConfiguration
\brief The TlsConfiguration class represents a configuration for TLS encryption.
*/

/*!
 \enum TlsConfiguration::Cipher
 \brief Ciphers TlsConfiguration supports
 \var TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
 \brief TLS 1.2 cipher.
 \var TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256
 \brief TLS 1.2 cipher.
 \var TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
 \brief TLS 1.2 cipher.
 \var TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256
 \brief TLS 1.2 cipher.
 \var TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256
 \brief TLS 1.2 cipher.
 \var TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256
 \brief TLS 1.2 cipher.
 \var TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384
 \brief TLS 1.2 cipher.
 \var TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384
 \brief TLS 1.2 cipher.
 \var TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
 \brief TLS 1.2 cipher.
 \var TlsConfiguration::Cipher::TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384
 \brief TLS 1.2 cipher.
 \var TlsConfiguration::Cipher::TLS_AES_128_GCM_SHA256
 \brief TLS 1.3 cipher.
 \var TlsConfiguration::Cipher::TLS_AES_256_GCM_SHA384
 \brief TLS 1.3 cipher.
 \var TlsConfiguration::Cipher::TLS_CHACHA20_POLY1305_SHA256
 \brief TLS 1.3 cipher.
*/

/*!
 \enum TlsConfiguration::Curve
 \brief Curves TlsConfiguration supports.
 \var TlsConfiguration::Curve::X25519
 \brief A 256-bit Montgomery curve.
 \var TlsConfiguration::Curve::prime256v1
 \brief A 256-bit prime field Weierstrass curve.
 \var TlsConfiguration::Curve::secp384r1
 \brief A 384-bit prime field Weierstrass curve.
 \var TlsConfiguration::Curve::secp521r1
 \brief A 521-bit prime field Weierstrass curve.
*/

/*!
 \enum TlsConfiguration::TlsVersion
 \brief TLS version.
 \var TlsConfiguration::TlsVersion::TLS_1_2
 \brief TLS version 1.2.
 \var TlsConfiguration::TlsVersion::TLS_1_2_or_newer
 \brief TLS version 1.2 or newer.
 \var TlsConfiguration::TlsVersion::TLS_1_3
 \brief TLS version 1.3
 \var TlsConfiguration::TlsVersion::TLS_1_3_or_newer
 \brief TLS version 1.3 or newer.
*/

/*!
 \enum TlsConfiguration::PeerVerifyMode
 \brief Peer verification mode for TLS connections.
 \var TlsConfiguration::PeerVerifyMode TlsConfiguration::On
 \brief Verify the peer when establishing the TLS connection.
 \var TlsConfiguration::PeerVerifyMode TlsConfiguration::Off
 \brief Do not verify the peer when establishing the TLS connection.
 \var TlsConfiguration::PeerVerifyMode TlsConfiguration::Auto
 \brief Verify servers but not clients when establishing TLS connections.
*/

/*!
\fn TlsConfiguration::TlsConfiguration()
Creates an empty TLS configuration.
*/

/*!
\fn TlsConfiguration::TlsConfiguration(const TlsConfiguration &other)
Creates a TlsConfiguration object and copies configuration from other to this object.
*/

/*!
\fn TlsConfiguration::operator=(const TlsConfiguration &other)
Copies configuration from \a other to this object.
*/

/*!
\fn TlsConfiguration::~TlsConfiguration()
Destroys the TlsConfiguration object.
*/

/*!
\fn TlsConfiguration::setCertificateKeyPair(std::string_view certificate, std::string_view key, std::string_view keyPassword = "")
Sets the certificate and private key files.
TlsConfiguration loads the first private key found in the \a key file and the first certificate in the \a certificate file.
If the \a certificate file contains more than one certificate, TlsConfiguration adds all the other certificates to the chain of certificates.
Certificates and private keys should be in the PEM format.
You can use the \a keyPassword parameter to inform the password for encrypted private keys.
*/

/*!
\fn TlsConfiguration::setTlsVersion(TlsVersion tlsVersion)
Sets the \link TlsConfiguration::TlsVersion TLS version\endlink to use.
*/

/*!
\fn TlsConfiguration::setCiphers(const std::set<Cipher> &ciphers)
Sets the \link TlsConfiguration::Cipher ciphers\endlink to use.
*/

/*!
\fn TlsConfiguration::setCiphers(std::set<Cipher> &&ciphers)
Sets the \link TlsConfiguration::Cipher ciphers\endlink to use.
*/

/*!
\fn TlsConfiguration::supportedCiphers()
Returns the ciphers supported by TlsConfiguration.
*/

/*!
\fn TlsConfiguration::setCurves(std::set<Curve> curves)
Sets the \link TlsConfiguration::Curve curves\endlink to use.
*/

/*!
\fn TlsConfiguration::supportedCurves()
Returns the curves supported by TlsConfiguration.
*/

/*!
\fn TlsConfiguration::addCaCertificate(std::string_view certificate)
Adds the \a certificate file to the set of files from which to load CA certificates. The certificate files should contain CA
certificates in the PEM format.
*/

/*!
\fn TlsConfiguration::setCaCertificates(const std::set<std::string> &certificates)
Sets the set of file paths containing CA \a certificates. CA certificates should be in the PEM format.
*/

/*!
\fn TlsConfiguration::setPeerVerifyDepth(int depth)
Sets the maximum \a depth for the certificate chain verification that TlsConfiguration can use.
*/

/*!
\fn TlsConfiguration::setPeerVerifyMode(PeerVerifyMode mode)
Sets the \link TlsConfiguration::PeerVerifyMode peer verification mode\endlink.
*/

/*!
\fn TlsConfiguration::setUseSystemCertificates(bool useSystemCertificates)
If \a useSystemCertificates is true, TlsConfiguration sets OpenSSL to load CA certificates from default locations.
*/

/*!
\fn TlsConfiguration::certificate()
Returns the file path of the local certificate given in [setCertificateKeyPair](@ref Kourier::TlsConfiguration::setCertificateKeyPair),
which should be sent to the connected peer during the TLS handshake for
verification purposes.
*/

/*!
\fn TlsConfiguration::privateKey()
Returns the file path of the private key given in [setCertificateKeyPair](@ref Kourier::TlsConfiguration::setCertificateKeyPair),
and belonging to the local certificate.
*/

/*!
\fn TlsConfiguration::privateKeyPassword()
Returns the encrypted private key password given in [setCertificateKeyPair](@ref Kourier::TlsConfiguration::setCertificateKeyPair).
*/

/*!
\fn TlsConfiguration::useSystemCertificates()
Returns true if this TlsConfiguration makes OpenSSL load CA certificates from default locations.
*/

/*!
\fn TlsConfiguration::addedCertificates()
Returns the file paths of the files from which CA certificates are loaded.
*/

/*!
\fn TlsConfiguration::ciphers()
Returns the set of [ciphers](@ref Kourier::TlsConfiguration::Cipher) that have been set for this TlsConfiguration instance.
*/

/*!
\fn TlsConfiguration::curves()
Returns the set of [curves](@ref Kourier::TlsConfiguration::Curve) that have been set for this TlsConfiguration instance.
*/

/*!
\fn TlsConfiguration::tlsVersion()
Returns the [TLS version](@ref Kourier::TlsConfiguration::TlsVersion) this TlsConfiguration uses.
*/

/*!
\fn TlsConfiguration::peerVerifyDepth()
Returns the peer verify depth.
*/

/*!
\fn TlsConfiguration::peerVerifyMode()
Returns the \link TlsConfiguration::PeerVerifyMode peer verify mode\endlink.
*/

struct TlsConfigurationData : public QSharedData
{
    std::string m_certificate;
    std::string m_privateKey;
    std::string m_privateKeyPassword;
    TlsConfiguration::TlsVersion m_tlsVersion = TlsConfiguration::TlsVersion::TLS_1_2_or_newer;
    std::set<TlsConfiguration::Cipher> m_ciphers;
    std::set<TlsConfiguration::Curve> m_curves;
    std::set<std::string> m_addedCertificates;
    int m_peerVerifyDepth = 0;
    TlsConfiguration::PeerVerifyMode m_peerVerifyMode = TlsConfiguration::PeerVerifyMode::Auto;
    bool m_useSystemCertificates = true;
    friend inline bool operator==(const TlsConfigurationData &obj1, const TlsConfigurationData &obj2)
    {
        return obj1.m_certificate == obj2.m_certificate
               && obj1.m_privateKey == obj2.m_privateKey
               && obj1.m_privateKeyPassword == obj2.m_privateKeyPassword
               && obj1.m_tlsVersion == obj2.m_tlsVersion
               && obj1.m_ciphers == obj2.m_ciphers
               && obj1.m_curves == obj2.m_curves
               && obj1.m_addedCertificates == obj2.m_addedCertificates
               && obj1.m_peerVerifyDepth == obj2.m_peerVerifyDepth
               && obj1.m_peerVerifyMode == obj2.m_peerVerifyMode
               && obj1.m_useSystemCertificates == obj2.m_useSystemCertificates;
    }
};

TlsConfiguration::TlsConfiguration() : m_d(new TlsConfigurationData) {}

TlsConfiguration::TlsConfiguration(const TlsConfiguration &other) :
    m_d(other.m_d)
{
}

TlsConfiguration &TlsConfiguration::operator=(const TlsConfiguration &other)
{
    if (this != &other)
        m_d = other.m_d;
    return *this;
}

TlsConfiguration::~TlsConfiguration()
{
}

void TlsConfiguration::setCertificateKeyPair(std::string_view certificate,
                                             std::string_view key,
                                             std::string_view keyPassword)
{
    m_d->m_certificate = certificate;
    m_d->m_privateKey = key;
    m_d->m_privateKeyPassword = keyPassword;
}

void TlsConfiguration::setTlsVersion(TlsVersion tlsVersion)
{
    m_d->m_tlsVersion = tlsVersion;
}

void TlsConfiguration::setCiphers(const std::set<Cipher> &ciphers)
{
    if (!ciphers.empty())
        m_d->m_ciphers = ciphers;
}

std::set<TlsConfiguration::Cipher> TlsConfiguration::supportedCiphers()
{
    return {Cipher::TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
            Cipher::TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
            Cipher::TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
            Cipher::TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
            Cipher::TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
            Cipher::TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
            Cipher::TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
            Cipher::TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
            Cipher::TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
            Cipher::TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
            Cipher::TLS_AES_128_GCM_SHA256,
            Cipher::TLS_AES_256_GCM_SHA384,
            Cipher::TLS_CHACHA20_POLY1305_SHA256};
}

void TlsConfiguration::setCurves(std::set<Curve> curves)
{
    if (!curves.empty())
        m_d->m_curves = curves;
}

std::set<TlsConfiguration::Curve> TlsConfiguration::supportedCurves()
{
    return {Curve::X25519,
            Curve::prime256v1,
            Curve::secp384r1,
            Curve::secp521r1};
}

void TlsConfiguration::addCaCertificate(std::string_view certificate)
{
    m_d->m_addedCertificates.insert(std::string(certificate));
}

void TlsConfiguration::setCaCertificates(const std::set<std::string> &certificates)
{
    if (!certificates.empty())
    {
        m_d->m_useSystemCertificates = false;
        m_d->m_addedCertificates = certificates;
    }
}

void TlsConfiguration::setPeerVerifyDepth(int depth)
{
    m_d->m_peerVerifyDepth = depth;
}

void TlsConfiguration::setPeerVerifyMode(PeerVerifyMode mode)
{
    m_d->m_peerVerifyMode = mode;
}

void TlsConfiguration::setUseSystemCertificates(bool useSystemCertificates)
{
    m_d->m_useSystemCertificates = useSystemCertificates;
}

const std::string &TlsConfiguration::certificate() const
{
    return m_d->m_certificate;
}

const std::string &TlsConfiguration::privateKey() const
{
    return m_d->m_privateKey;
}

const std::string &TlsConfiguration::privateKeyPassword() const
{
    return m_d->m_privateKeyPassword;
}

bool TlsConfiguration::useSystemCertificates() const
{
    return m_d->m_useSystemCertificates;
}

const std::set<std::string> &TlsConfiguration::addedCertificates() const
{
    return m_d->m_addedCertificates;
}

const std::set<TlsConfiguration::Cipher> &TlsConfiguration::ciphers() const
{
    return m_d->m_ciphers;
}

const std::set<TlsConfiguration::Curve> &TlsConfiguration::curves() const
{
    return m_d->m_curves;
}

TlsConfiguration::TlsVersion TlsConfiguration::tlsVersion() const
{
    return m_d->m_tlsVersion;
}

int TlsConfiguration::peerVerifyDepth() const
{
    return m_d->m_peerVerifyDepth;
}

TlsConfiguration::PeerVerifyMode TlsConfiguration::peerVerifyMode() const
{
    return m_d->m_peerVerifyMode;
}

bool operator==(const TlsConfiguration &obj1, const TlsConfiguration &obj2)
{
    return *(obj1.m_d.constData()) == *(obj2.m_d.constData());
}

}
