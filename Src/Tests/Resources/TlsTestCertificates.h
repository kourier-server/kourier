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

#ifndef KOURIER_TLS_TEST_CERTIFICATES_H
#define KOURIER_TLS_TEST_CERTIFICATES_H

#include <string>


namespace Kourier::TestResources
{

struct TlsTestCertificateInfo
{
    std::string caCertificate;
    std::string certificateChain;
    std::string privateKey;
    std::string encryptedPrivateKey;
    std::string encryptedPrivateKeyPassword;
};

class TlsTestCertificates
{
public:
    enum CertificateType {RSA_2048,
                           RSA_2048_EncryptedPrivateKey,
                           RSA_2048_CHAIN,
                           RSA_2048_CHAIN_EncryptedPrivateKey,
                           ECDSA,
                           ECDSA_EncryptedPrivateKey,
                           ECDSA_CHAIN,
                           ECDSA_CHAIN_EncryptedPrivateKey};
    static TlsTestCertificateInfo getTestCertificate(CertificateType type);
    static void getFilesFromCertificateType(CertificateType certType, std::string &certificateFile, std::string &privateKeyFile, std::string &caCertificateFile);
    static void getContentsFromCertificateType(CertificateType certType, std::string &certificateContents, std::string &privateKeyContents, std::string &privateKeyPassword, std::string &caCertificateContents);;

private:
    static std::string createTempFile(const std::string &contents);
};

}

#endif // KOURIER_TLS_TEST_CERTIFICATES_H
