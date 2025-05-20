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

#include "TlsSocket.h"
#include "TlsSocketPrivate_epoll.h"
#include "UnixUtils.h"
#include <openssl/err.h>
#include <sys/ioctl.h>
#include <sys/socket.h>


namespace Kourier
{

TlsSocketPrivate::TlsSocketPrivate(RingBuffer &unencryptedIncomingDataBuffer,
                                   RingBuffer &unencryptedOutgoingDataBuffer,
                                   const TlsConfiguration &tlsConfiguration,
                                   TlsContext::Role role) :
    m_encryptedIncomingDataBuffer(m_encryptedIncomingDataBufferBIO.ringBuffer()),
    m_unencryptedIncomingDataBuffer(unencryptedIncomingDataBuffer),
    m_unencryptedOutgoingDataBuffer(unencryptedOutgoingDataBuffer),
    m_encryptedOutgoingDataBuffer(m_encryptedOutgoingDataBufferBIO.ringBuffer()),
    m_tlsDataSink(m_pSSL),
    m_tlsDataSource(m_pSSL, m_encryptedIncomingDataBufferBIO.ringBuffer())
{
    m_handshakeTimer.setSingleShot(true);
    Object::connect(&m_handshakeTimer, &Timer::timeout, this, &TlsSocketPrivate::onHandshakeTimeout);
    try
    {
        m_tlsContext = TlsContext::fromTlsConfiguration(tlsConfiguration, role);
    }
    catch (const RuntimeError &runtimeError)
    {
        m_tlsErrorMessage = runtimeError.error();
        if (m_tlsErrorMessage.empty())
            m_tlsErrorMessage = "Failed to create TLS context from TlsConfiguration. Unknown TLS error.";
    }
}

TlsSocketPrivate::~TlsSocketPrivate()
{
    assert(m_state == TcpSocket::State::Unconnected);
    abortTls();
}

void TlsSocketPrivate::connect(std::string_view host, uint16_t port)
{
    Q_Q(TlsSocket);
    if (m_tlsContext.context() == nullptr)
        throw RuntimeError(m_tlsErrorMessage, RuntimeError::ErrorType::User);
    else if (m_tlsContext.role() != TlsContext::Role::Client)
    {
        try
        {
            m_tlsContext = TlsContext::fromTlsConfiguration(m_tlsContext.tlsConfiguration(), TlsContext::Role::Client);
        }
        catch (const RuntimeError &runtimeError)
        {
            m_tlsContext = TlsContext();
            m_tlsErrorMessage = runtimeError.error();
            throw RuntimeError(m_tlsErrorMessage, RuntimeError::ErrorType::User);
        }
    }
    TcpSocketPrivate::connect(host, port);
}

void TlsSocketPrivate::disconnectFromPeer()
{
    Q_Q(TlsSocket);
    switch (m_state)
    {
        case TcpSocket::State::Unconnected:
        case TcpSocket::State::Disconnecting:
            break;
        case TcpSocket::State::Connecting:
            abort();
            return;
        case TcpSocket::State::Connected:
            if (!m_hasCompletedHandshake)
            {
                abort();
                return;
            }
            m_state = TcpSocket::State::Disconnecting;
            m_disconnectTimer.start(disconnectTimeoutInMSecs);
            if (m_unencryptedOutgoingDataBuffer.isEmpty())
            {
                if ((SSL_get_shutdown(m_pSSL) & SSL_SENT_SHUTDOWN) != SSL_SENT_SHUTDOWN)
                {
                    SSL_shutdown(m_pSSL);
                    eventNotifier()->postEvent(this, EPOLLOUT);
                }
                if (m_encryptedOutgoingDataBuffer.isEmpty())
                {
                    q->setReadChannelNotificationEnabled(false);
                    q->setWriteChannelNotificationEnabled(false);
                    setEventTypes(eventTypes() & ~(EPOLLIN | EPOLLOUT));
                    if (::shutdown(m_socketDescriptor, SHUT_WR) != 0)
                    {
                        if (q->dataAvailable() > 0)
                        {
                            setEnabled(false);
                            UnixUtils::safeClose(m_socketDescriptor);
                            m_socketDescriptor = -1;
                            m_unencryptedOutgoingDataBuffer.clear();
                            m_encryptedOutgoingDataBuffer.clear();
                            abortTls();
                            m_state = TcpSocket::State::Unconnected;
                        }
                        else
                            abort();
                        q->disconnected();
                        return;
                    }
                }
            }
            break;
    }
}

void TlsSocketPrivate::abort()
{
    abortTls();
    TcpSocketPrivate::abort();
}

void TlsSocketPrivate::onDisconnectTimeoutImpl()
{
    Q_Q(TlsSocket);
    if (m_state == TcpSocket::State::Disconnecting)
    {
        if (q->dataAvailable() > 0)
        {
            setEnabled(false);
            UnixUtils::safeClose(m_socketDescriptor);
            m_socketDescriptor = -1;
            m_unencryptedOutgoingDataBuffer.clear();
            m_encryptedOutgoingDataBuffer.clear();
            abortTls();
            m_state = TcpSocket::State::Unconnected;
        }
        else
            abort();
        q->disconnected();
    }
}

void TlsSocketPrivate::doHandshake()
{
    Q_Q(TlsSocket);
    if (m_hasCompletedHandshake)
        return;
    if (!m_handshakeTimer.isActive())
        m_handshakeTimer.start(handshakeTimeoutInMSecs);
    switch (const auto ret = SSL_do_handshake(m_pSSL))
    {
        case 0:
            throw RuntimeError("TLS handshake failed.", RuntimeError::ErrorType::TLS);
        case 1:
        {
            m_handshakeTimer.stop();
            q->writeDataToChannel();
            m_hasCompletedHandshake = true;
            eventNotifier()->postEvent(this, EPOLLOUT);
            q->encrypted();
            return;
        }
        default:
            switch (SSL_get_error(m_pSSL, ret))
            {
                case SSL_ERROR_WANT_WRITE:
                case SSL_ERROR_WANT_READ:
                    q->writeDataToChannel();
                    return;
                default:
                    throw RuntimeError("TLS handshake failed.", RuntimeError::ErrorType::TLS);
            }
    }
}

void TlsSocketPrivate::setupTls()
{
    abortTls();
    if (m_tlsContext.context() == nullptr)
        throw RuntimeError(m_tlsErrorMessage, RuntimeError::ErrorType::User);
    m_pSSL = SSL_new(m_tlsContext.context());
    if (m_pSSL == nullptr)
        throw RuntimeError("Failed to create SSL object.", RuntimeError::ErrorType::TLS);
    switch (m_tlsContext.role())
    {
        case TlsContext::Role::Client:
            if (!m_peerName.empty())
            {
                SSL_set_tlsext_host_name(m_pSSL, m_peerName.c_str());
                SSL_set1_host(m_pSSL, m_peerName.c_str());
            }
            else if (!m_peerAddress.empty())
            {
                SSL_set_tlsext_host_name(m_pSSL, m_peerAddress.c_str());
                SSL_set1_host(m_pSSL, m_peerAddress.c_str());
            }
            else if (!m_hostAddresses.empty())
            {
                SSL_set_tlsext_host_name(m_pSSL, m_hostAddresses[0].c_str());
                SSL_set1_host(m_pSSL, m_hostAddresses[0].c_str());
            }
            SSL_set_connect_state(m_pSSL);
            break;
        case TlsContext::Role::Server:
            SSL_set_accept_state(m_pSSL);
            break;
    }
    BIO_up_ref(m_encryptedIncomingDataBufferBIO.bio());
    BIO_up_ref(m_encryptedOutgoingDataBufferBIO.bio());
    SSL_set_bio(m_pSSL, m_encryptedIncomingDataBufferBIO.bio(), m_encryptedOutgoingDataBufferBIO.bio());
}

void TlsSocketPrivate::abortTls()
{
    if (m_pSSL != nullptr)
        SSL_free(m_pSSL);
    m_pSSL = nullptr;
    m_hasCompletedHandshake = false;
    m_handshakeTimer.stop();
    m_encryptedIncomingDataBufferBIO.ringBuffer().clear();
    m_encryptedOutgoingDataBufferBIO.ringBuffer().clear();
}

void TlsSocketPrivate::onConnecting()
{
    setupTls();
}

void TlsSocketPrivate::onConnected()
{
    Q_Q(TlsSocket);
    if (m_pSSL == nullptr)
        setupTls();
    const auto contextId = m_contextId;
    q->connected();
    if (contextId == m_contextId)
        doHandshake();
}

void TlsSocketPrivate::onEvent(uint32_t epollEvents)
{
    Q_Q(TlsSocket);
    try
    {
        size_t receivedDataSize = 0;
        size_t sentDataSize = 0;
        bool hasDisconnected = false;
        if ((epollEvents & EPOLLIN) && (m_state == TcpSocket::State::Connected))
        {
            receivedDataSize = q->readDataFromChannel();
            if (!m_hasCompletedHandshake)
            {
                const auto currentContextId = m_contextId;
                doHandshake();
                if (currentContextId != m_contextId)
                    return;
            }
        }
        if (epollEvents & EPOLLOUT)
        {
            if (m_state == TcpSocket::State::Connected)
                sentDataSize = q->writeDataToChannel();
            else if (m_state == TcpSocket::State::Disconnecting)
            {
                sentDataSize = q->writeDataToChannel();
                if (m_unencryptedOutgoingDataBuffer.isEmpty())
                {
                    if ((SSL_get_shutdown(m_pSSL) & SSL_SENT_SHUTDOWN) != SSL_SENT_SHUTDOWN)
                    {
                        SSL_shutdown(m_pSSL);
                        eventNotifier()->postEvent(this, EPOLLOUT);
                    }
                    if (m_encryptedOutgoingDataBuffer.isEmpty())
                    {
                        q->setReadChannelNotificationEnabled(false);
                        q->setWriteChannelNotificationEnabled(false);
                        setEventTypes(eventTypes() & ~(EPOLLIN | EPOLLOUT));
                        if (::shutdown(m_socketDescriptor, SHUT_WR) != 0)
                        {
                            m_disconnectTimer.stop();
                            hasDisconnected = true;
                        }
                    }
                }
            }
            else if (m_state == TcpSocket::State::Connecting)
            {
                m_connectTimer.stop();
                int errorCode = -1;
                socklen_t optlen = sizeof(errorCode);
                if (::getsockopt(m_socketDescriptor, SOL_SOCKET, SO_ERROR, &errorCode, &optlen) == 0
                    && errorCode == 0
                    && fetchConnectionParameters())
                {
                    m_state = TcpSocket::State::Connected;
                    q->setReadChannelNotificationEnabled(true);
                    q->setWriteChannelNotificationEnabled(false);
                    setEnabled(true);
                    const auto currentContextId = m_contextId;
                    onConnected();
                    if (currentContextId != m_contextId)
                        return;
                }
                else
                {
                    connectToHost();
                    return;
                }
            }
        }
        if ((epollEvents & EPOLLRDHUP)
            || (epollEvents & EPOLLERR)
            || (epollEvents & EPOLLHUP)
            || (epollEvents & EPOLLPRI))
        {
            m_disconnectTimer.stop();
            hasDisconnected = true;
        }
        const auto contextId = m_contextId;
        if (receivedDataSize > 0)
            q->receivedData();
        if (contextId == m_contextId && sentDataSize > 0)
            q->sentData(sentDataSize);
        if (contextId == m_contextId && hasDisconnected)
        {
            while (contextId == m_contextId
                   && (m_tcpSocketDataSource.dataAvailable() + m_tlsDataSource.dataAvailable()) > 0
                   && q->readDataFromChannel() > 0)
            {
                q->receivedData();
            }
            m_hasToAddSocketToReadyEventSourceListAfterReading = false;
            eventNotifier()->removePostedEvents(this);
            const bool hasToEmitDisconnected = (m_state == TcpSocket::State::Connected || m_state == TcpSocket::State::Disconnecting);
            if (contextId == m_contextId)
            {
                if (q->dataAvailable() > 0)
                {
                    setEnabled(false);
                    UnixUtils::safeClose(m_socketDescriptor);
                    m_socketDescriptor = -1;
                    m_unencryptedOutgoingDataBuffer.clear();
                    m_encryptedOutgoingDataBuffer.clear();
                    abortTls();
                    m_state = TcpSocket::State::Unconnected;
                }
                else
                    abort();
                if (hasToEmitDisconnected)
                    q->disconnected();
            }
        }
    }
    catch (const RuntimeError &runtimeError)
    {
        setError(runtimeError.error());
        return;
    }
}

void TlsSocketPrivate::onHandshakeTimeout()
{
    const bool isIPv6 = (QHostAddress(QString::fromStdString(m_peerAddress)).protocol() == QAbstractSocket::IPv6Protocol);
    if (m_peerName.empty())
        setError(std::string("Failed to connect to ")
                     .append(isIPv6 ? "[" : "")
                     .append(m_peerAddress)
                     .append(isIPv6 ? "]" : "")
                     .append(":")
                     .append(std::to_string(m_peerPort))
                     .append(". TLS handshake timed out."));
    else
        setError(std::string("Failed to connect to ")
                     .append(m_peerName)
                     .append(" at ")
                     .append(isIPv6 ? "[" : "")
                     .append(m_peerAddress)
                     .append(isIPv6 ? "]" : "")
                     .append(":")
                     .append(std::to_string(m_peerPort))
                     .append(". TLS handshake timed out."));
}

//
// TlsSocket methods
//

TlsSocket::TlsSocket(const TlsConfiguration &tlsConfiguration) :
    TcpSocket(new TlsSocketPrivate(m_readBuffer, m_writeBuffer, tlsConfiguration, TlsContext::Role::Client))
{
    d_ptr->q_ptr = this;
}

TlsSocket::TlsSocket(int64_t socketDescriptor, const TlsConfiguration &tlsConfiguration) :
    TcpSocket(new TlsSocketPrivate(m_readBuffer, m_writeBuffer, tlsConfiguration, TlsContext::Role::Server))
{
    d_ptr->q_ptr = this;
    if (((TlsSocketPrivate*)d_ptr)->m_tlsErrorMessage.empty())
    {
        try
        {
            d_ptr->setSocketDescriptor(socketDescriptor);
        }
        catch (const RuntimeError &runtimeError)
        {
            abort();
            d_ptr->m_errorMessage = runtimeError.error();
        }
    }
    else
    {
        if (socketDescriptor >= 0)
            UnixUtils::safeClose(socketDescriptor);
    }
}

TlsSocket::~TlsSocket()
{
    Q_D(TlsSocket);
    d->abortTls();
}

bool TlsSocket::isEncrypted() const
{
    Q_D(const TlsSocket);
    return (d->m_state == TcpSocket::State::Connected && d->m_hasCompletedHandshake);
}

size_t TlsSocket::dataToWrite() const
{
    Q_D(const TlsSocket);
    return m_writeBuffer.size() + d->m_encryptedOutgoingDataBuffer.size();
}

size_t TlsSocket::read(char *pBuffer, size_t maxSize)
{
    Q_D(TlsSocket);
    const bool wasFull = d->m_unencryptedIncomingDataBuffer.isFull();
    const auto bytesRead = d->m_unencryptedIncomingDataBuffer.read(pBuffer, maxSize);
    if (wasFull && bytesRead > 0)
        d->eventNotifier()->postEvent(d, EPOLLIN);
    return bytesRead;
}

std::string_view TlsSocket::readAll()
{
    Q_D(TlsSocket);
    const bool wasFull = d->m_unencryptedIncomingDataBuffer.isFull();
    auto data = d->m_unencryptedIncomingDataBuffer.readAll();
    if (wasFull)
        d->eventNotifier()->postEvent(d, EPOLLIN);
    return data;
}

size_t TlsSocket::skip(size_t maxSize)
{
    Q_D(TlsSocket);
    const bool wasFull = d->m_unencryptedIncomingDataBuffer.isFull();
    const auto poppedBytes = d->m_unencryptedIncomingDataBuffer.popFront(maxSize);
    if (wasFull && poppedBytes > 0)
        d->eventNotifier()->postEvent(d, EPOLLIN);
    return poppedBytes;
}

const TlsConfiguration &TlsSocket::tlsConfiguration() const
{
    Q_D(const TlsSocket);
    return d->tlsConfiguration();
}

Signal TlsSocket::encrypted() KOURIER_SIGNAL(&TlsSocket::encrypted)

//
// From IOChannel
//
size_t TlsSocket::readDataFromChannel()
{
    Q_D(TlsSocket);
    if (d->m_unencryptedIncomingDataBuffer.isFull())
        return 0;
    const auto tlsDataSinkWasExpectingToRead = d->m_tlsDataSink.needsToRead();
    d->m_encryptedIncomingDataBuffer.write(dataSource());
    if (dataSource().dataAvailable() > 0)
        d->eventNotifier()->postEvent(d, EPOLLIN);
    const auto encryptedOutgoingDataBufferPreviousSize = d->m_encryptedOutgoingDataBuffer.size();
    const auto bytesRead = d->m_unencryptedIncomingDataBuffer.write(d->m_tlsDataSource);
    if (tlsDataSinkWasExpectingToRead || (d->m_encryptedOutgoingDataBuffer.size() > encryptedOutgoingDataBufferPreviousSize))
        d->eventNotifier()->postEvent(d, EPOLLOUT);
    if (d->m_hasCompletedHandshake && ((SSL_get_shutdown(d->m_pSSL) & SSL_RECEIVED_SHUTDOWN) == SSL_RECEIVED_SHUTDOWN))
        disconnectFromPeer();
    return bytesRead;
}

size_t TlsSocket::writeDataToChannel()
{
    Q_D(TlsSocket);
    if (d->m_hasCompletedHandshake)
        d->m_unencryptedOutgoingDataBuffer.read(d->m_tlsDataSink);
    const auto bytesWritten = d->m_encryptedOutgoingDataBuffer.read(dataSink());
    d->m_hasAlreadyScheduledWriteEvent = false;
    return bytesWritten;
}

}
