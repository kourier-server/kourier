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

#ifndef KOURIER_TCP_SOCKET_PRIVATE_EPOLL_H
#define KOURIER_TCP_SOCKET_PRIVATE_EPOLL_H

#include "TcpSocket.h"
#include "EpollEventSource.h"
#include "TcpSocketDataSource.h"
#include "TcpSocketDataSink.h"
#include "Timer.h"


namespace Kourier
{

static constexpr int64_t connectTimeoutInMSecs = 60000;
static constexpr int64_t disconnectTimeoutInMSecs = 10000;

class TcpSocketPrivate : public EpollEventSource
{
KOURIER_OBJECT(Kourier::TcpSocketPrivate);
public:
    TcpSocketPrivate();
    ~TcpSocketPrivate() override;
    void setSocketDescriptor(int64_t socketDescriptor);
    void bind(std::string_view address, uint16_t port = 0);
    virtual void connect(std::string_view host, uint16_t port);
    virtual void disconnectFromPeer();
    virtual void abort();
    inline std::string_view errorMessage() const {return m_errorMessage;}
    inline std::string_view localAddress() const {return m_localAddress;}
    inline uint16_t localPort() const {return m_localPort;}
    inline std::string_view peerName() const {return m_peerName;}
    inline std::string_view peerAddress() const {return m_peerAddress;}
    inline uint16_t peerPort() const {return m_peerPort;}
    inline std::string_view proxyAddress() const {return m_proxyAddress;}
    inline uint16_t proxyPort() const {return m_proxyPort;}
    inline TcpSocket::State state() const {return m_state;}
    int64_t fileDescriptor() const override {return m_socketDescriptor;}
    inline TcpSocketDataSource &tcpSocketDataSource() {return m_tcpSocketDataSource;}
    inline TcpSocketDataSink &tcpSocketDataSink() {return m_tcpSocketDataSink;}
    void setReadEnabled(bool enabled);
    void setWriteEnabled(bool enabled);
    int getSocketOption(TcpSocket::SocketOption option) const;
    void setSocketOption(TcpSocket::SocketOption option, int value);

private:
    void connectToHost();
    static void hostFoundCallback(const std::vector<std::string> &addresses, void *pRawTcpSocketPrivate);
    void onHostFound(const std::vector<std::string> &addresses);
    bool fetchConnectionParameters();
    void setError(std::string_view errorMessage);
    void onEvent(uint32_t epollEvents) override;
    virtual void onConnecting() {}
    virtual void onConnected();
    void onConnectTimeout();
    void onDisconnectTimeout() {onDisconnectTimeoutImpl();}

protected:
    virtual void onDisconnectTimeoutImpl();

private:
    TcpSocket *q_ptr;
    Q_DECLARE_PUBLIC(TcpSocket)
    friend class TlsSocket;
    friend class TlsSocketPrivate;

protected:
    std::string m_peerName;
    std::string m_bindAddress;
    std::string m_peerAddress;
    std::string m_localAddress;
    std::string m_proxyAddress;
    std::string m_errorMessage;
    std::vector<std::string> m_hostAddresses;
    int64_t m_socketDescriptor = -1;
    TcpSocketDataSource m_tcpSocketDataSource;
    TcpSocketDataSink m_tcpSocketDataSink;
    Timer m_connectTimer;
    Timer m_disconnectTimer;
    uint64_t m_contextId = 1;
    uint16_t m_bindPort = 0;
    uint16_t m_peerPort = 0;
    uint16_t m_localPort = 0;
    uint16_t m_proxyPort = 0;
    TcpSocket::State m_state = TcpSocket::State::Unconnected;
    bool m_hasToAddSocketToReadyEventSourceListAfterReading = false;
    bool m_hasAlreadyScheduledWriteEvent = false;
    bool m_isLookingUpHost = false;
};

}

#endif // KOURIER_TCP_SOCKET_PRIVATE_EPOLL_H
