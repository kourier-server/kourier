//
// Copyright (C) 2023 Glauco Pacheco <glauco@kourier.io>
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

#ifndef KOURIER_TCP_SOCKET_H
#define KOURIER_TCP_SOCKET_H

#include "IOChannel.h"


namespace Kourier
{

class TcpSocketPrivate;

class KOURIER_EXPORT TcpSocket : public IOChannel
{
KOURIER_OBJECT(Kourier::TcpSocket)
public:
    TcpSocket();
    TcpSocket(int64_t socketDescriptor);
    ~TcpSocket() override;
    size_t read(char *pBuffer, size_t maxSize) override;
    size_t write(std::string_view data) {return !data.empty() ? write(data.data(), data.size()) : 0;}
    size_t write(const char *pData, size_t maxSize) override;
    std::string_view readAll() override;
    size_t skip(size_t maxSize) override;
    void setBindAddressAndPort(std::string_view address, uint16_t port = 0);
    void connect(std::string_view host, uint16_t port);
    void disconnectFromPeer();
    void abort();
    std::string_view errorMessage() const;
    std::string_view localAddress() const;
    uint16_t localPort() const;
    std::string_view peerName() const;
    std::string_view peerAddress() const;
    uint16_t peerPort() const;
    std::string_view proxyAddress() const;
    uint16_t proxyPort() const;
    inline size_t readBufferCapacity() const {return IOChannel::readBufferCapacity();}
    inline bool setReadBufferCapacity(size_t capacity) {return IOChannel::setReadBufferCapacity(capacity);}
    enum class State {Unconnected, Connecting, Connected, Disconnecting};
    State state() const;
    bool resetBuffers() {return IOChannel::reset();}
    enum class SocketOption {LowDelay, KeepAlive, SendBufferSize, ReceiveBufferSize};
    int getSocketOption(SocketOption option) const;
    void setSocketOption(SocketOption option, int value);
    Signal connected();
    Signal disconnected();
    Signal error();

protected:
    TcpSocket(TcpSocketPrivate *pTcpSocketPrivate);

private:
    // From IOChannel
    size_t readDataFromChannel() override;
    size_t writeDataToChannel() override;
    DataSource &dataSource() override;
    DataSink &dataSink() override;
    void onReadNotificationChanged() override;
    void onWriteNotificationChanged() override;

private:
    TcpSocketPrivate *d_ptr;
    Q_DECLARE_PRIVATE(TcpSocket)
    Q_DISABLE_COPY_MOVE(TcpSocket)
    friend class TlsSocket;
};

}

#endif // KOURIER_TCP_SOCKET_H
