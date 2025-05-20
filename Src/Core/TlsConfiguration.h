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

#ifndef KOURIER_TLS_CONFIGURATION_H
#define KOURIER_TLS_CONFIGURATION_H

#include "SDK.h"
#include <QSharedDataPointer>
#include <string>
#include <set>


namespace Kourier
{

struct TlsConfigurationData;
class KOURIER_EXPORT TlsConfiguration
{
public:
    enum PeerVerifyMode {On, Off, Auto};
    TlsConfiguration();
    TlsConfiguration(const TlsConfiguration &other);
    TlsConfiguration &operator=(const TlsConfiguration &other);
    ~TlsConfiguration();
    void setCertificateKeyPair(std::string_view certificate, std::string_view key, std::string_view keyPassword = "");
    enum class TlsVersion {TLS_1_2, TLS_1_2_or_newer, TLS_1_3, TLS_1_3_or_newer};
    void setTlsVersion(TlsVersion tlsVersion);
    enum class Cipher
    {
        TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
        TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
        TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
        TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
        TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
        TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
        TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
        TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
        TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
        TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
        TLS_AES_128_GCM_SHA256,
        TLS_AES_256_GCM_SHA384,
        TLS_CHACHA20_POLY1305_SHA256
    };
    void setCiphers(const std::set<Cipher> &ciphers);
    static std::set<Cipher> supportedCiphers();
    enum class Curve
    {
        X25519,
        prime256v1,
        secp384r1,
        secp521r1
    };
    void setCurves(std::set<Curve> curves);
    static std::set<Curve> supportedCurves();
    void addCaCertificate(std::string_view certificate);
    void setCaCertificates(const std::set<std::string> &certificates);
    void setPeerVerifyDepth(int depth);
    void setPeerVerifyMode(PeerVerifyMode mode);
    void setUseSystemCertificates(bool useSystemCertificates);
    const std::string &certificate() const;
    const std::string &privateKey() const;
    const std::string &privateKeyPassword() const;
    bool useSystemCertificates() const;
    const std::set<std::string> &addedCertificates() const;
    const std::set<Cipher> &ciphers() const;
    const std::set<Curve> &curves() const;
    TlsVersion tlsVersion() const;
    int peerVerifyDepth() const;
    PeerVerifyMode peerVerifyMode() const;
    friend bool operator==(const TlsConfiguration &obj1, const TlsConfiguration &obj2);

private:
    QSharedDataPointer<TlsConfigurationData> m_d;
};

bool operator==(const TlsConfiguration &obj1, const TlsConfiguration &obj2);

}

#endif // KOURIER_TLS_CONFIGURATION_H
