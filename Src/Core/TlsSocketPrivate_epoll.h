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

#ifndef KOURIER_TLS_SOCKET_PRIVATE_EPOLL_H
#define KOURIER_TLS_SOCKET_PRIVATE_EPOLL_H

#include "TlsSocket.h"
#include "TcpSocketPrivate_epoll.h"
#include "TlsContext.h"
#include "TlsSocketDataSink.h"
#include "TlsSocketDataSource.h"
#include "RingBufferBIO.h"
#include <openssl/ssl.h>


namespace Kourier
{

static constexpr int64_t handshakeTimeoutInMSecs = 60000;

class TlsSocketPrivate : public TcpSocketPrivate
{
KOURIER_OBJECT(Kourier::TlsSocketPrivate);
public:
    TlsSocketPrivate(RingBuffer &unencryptedIncomingDataBuffer,
                     RingBuffer &unencryptedOutgoingDataBuffer,
                     const TlsConfiguration &tlsConfiguration,
                     TlsContext::Role role);
    ~TlsSocketPrivate() override;
    void connect(std::string_view host, uint16_t port) override;
    void disconnectFromPeer() override;
    void abort() override;
    const TlsConfiguration &tlsConfiguration() const {return m_tlsContext.tlsConfiguration();}

private:
    void onDisconnectTimeoutImpl() override;
    void doHandshake();
    void setupTls();
    void abortTls();
    void onConnecting() override;
    void onConnected() override;
    void onEvent(uint32_t epollEvents) override;
    void onHandshakeTimeout();

private:
    Q_DECLARE_PUBLIC(TlsSocket)
    TlsContext m_tlsContext;
    SSL *m_pSSL = nullptr;
    Timer m_handshakeTimer;
    RingBufferBIO m_encryptedIncomingDataBufferBIO;
    RingBufferBIO m_encryptedOutgoingDataBufferBIO;
    RingBuffer &m_encryptedIncomingDataBuffer;
    RingBuffer &m_unencryptedIncomingDataBuffer;
    RingBuffer &m_unencryptedOutgoingDataBuffer;
    RingBuffer &m_encryptedOutgoingDataBuffer;
    TlsSocketDataSink m_tlsDataSink;
    TlsSocketDataSource m_tlsDataSource;
    std::string m_tlsErrorMessage;
    bool m_hasCompletedHandshake = false;
};

}

#endif // KOURIER_TLS_SOCKET_PRIVATE_EPOLL_H
