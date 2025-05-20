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

#ifndef KOURIER_TLS_SOCKET_H
#define KOURIER_TLS_SOCKET_H

#include "TcpSocket.h"
#include "TlsConfiguration.h"


namespace Kourier
{

class TlsSocketPrivate;

class KOURIER_EXPORT TlsSocket : public TcpSocket
{
KOURIER_OBJECT(Kourier::TlsSocket)
public:
    TlsSocket(const TlsConfiguration &tlsConfiguration);
    TlsSocket(int64_t socketDescriptor, const TlsConfiguration &tlsConfiguration);
    ~TlsSocket() override;
    bool isEncrypted() const;
    size_t dataToWrite() const override;
    size_t read(char *pBuffer, size_t maxSize) override;
    std::string_view readAll() override;
    size_t skip(size_t maxSize) override;
    const TlsConfiguration &tlsConfiguration() const;
    Signal encrypted();

private:
    // From IOChannel
    size_t readDataFromChannel() override;
    size_t writeDataToChannel() override;

private:
    Q_DECLARE_PRIVATE(TlsSocket)
    Q_DISABLE_COPY_MOVE(TlsSocket)
};

}

#endif // KOURIER_TLS_SOCKET_H
