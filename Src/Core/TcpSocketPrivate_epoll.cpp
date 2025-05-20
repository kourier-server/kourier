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

#include "TcpSocket.h"
#include "TcpSocketPrivate_epoll.h"
#include "EpollEventNotifier.h"
#include "HostAddressFetcher.h"
#include "RuntimeError.h"
#include "UnixUtils.h"
#include "NoDestroy.h"
#include <QAtomicInt>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>


namespace Kourier
{

TcpSocketPrivate::TcpSocketPrivate() :
    EpollEventSource(EPOLLRDHUP | EPOLLPRI | EPOLLET | EPOLLIN | EPOLLOUT),
    m_tcpSocketDataSource(m_socketDescriptor),
    m_tcpSocketDataSink(m_socketDescriptor)
{
    m_connectTimer.setSingleShot(true);
    Object::connect(&m_connectTimer, &Timer::timeout, this, &TcpSocketPrivate::onConnectTimeout);
    m_disconnectTimer.setSingleShot(true);
    Object::connect(&m_disconnectTimer, &Timer::timeout, this, &TcpSocketPrivate::onDisconnectTimeout);
    static constinit NoDestroy<std::atomic_flag> sigPipeDisabler;
    if (!sigPipeDisabler().test_and_set())
    {
        struct sigaction disableSignal;
        std::memset(&disableSignal, 0, sizeof(disableSignal));
        disableSignal.sa_handler = SIG_IGN;
        if (::sigaction(SIGPIPE, &disableSignal, nullptr) != 0)
            qFatal("Failed to disable SIGPIPE signal.");
    }
}

TcpSocketPrivate::~TcpSocketPrivate()
{
    assert(m_state == TcpSocket::State::Unconnected);
}

void TcpSocketPrivate::setSocketDescriptor(int64_t socketDescriptor)
{
    Q_Q(TcpSocket);
    abort();
    if (socketDescriptor >= 0)
    {
        m_socketDescriptor = socketDescriptor;
        int socketType = -1;
        int socketProtocol = -1;
        int socketDomain = -1;
        int errorCode = -1;
        socklen_t optlen = sizeof(errorCode);
        const auto flags = ::fcntl(m_socketDescriptor, F_GETFL, 0);
        if (flags != -1
            && ::fcntl(m_socketDescriptor, F_SETFL, flags | O_NONBLOCK) == 0
            && ::getsockopt(m_socketDescriptor, SOL_SOCKET, SO_TYPE, &socketType, &optlen) == 0
            && socketType == SOCK_STREAM
            && ::getsockopt(m_socketDescriptor, SOL_SOCKET, SO_PROTOCOL, &socketProtocol, &optlen) == 0
            && socketProtocol == IPPROTO_TCP
            && ::getsockopt(m_socketDescriptor, SOL_SOCKET, SO_DOMAIN, &socketDomain, &optlen) == 0
            && ((socketDomain == AF_INET) || (socketDomain == AF_INET6))
            && ::getsockopt(m_socketDescriptor, SOL_SOCKET, SO_ERROR, &errorCode, &optlen) == 0
            && errorCode == 0
            && fetchConnectionParameters())
        {
            setSocketOption(TcpSocket::SocketOption::LowDelay, 1);
            m_state = TcpSocket::State::Connected;
            q->m_isReadNotificationEnabled = true;
            q->m_isWriteNotificationEnabled = false;
            setEnabled(true);
            onConnected();
            return;
        }
    }
    abort();
}

void TcpSocketPrivate::abort()
{
    Q_Q(TcpSocket);
    setEnabled(false);
    eventNotifier()->removePostedEvents(this);
    setEventTypes(EPOLLRDHUP | EPOLLPRI | EPOLLET | EPOLLIN | EPOLLOUT);
    if (m_socketDescriptor >= 0)
        UnixUtils::safeClose(m_socketDescriptor);
    if (m_isLookingUpHost)
    {
        m_isLookingUpHost = false;
        HostAddressFetcher::removeHostLookup(m_peerName, &hostFoundCallback, (void*)this);
    }
    m_peerName.clear();
    m_bindAddress.clear();
    m_peerAddress.clear();
    m_localAddress.clear();
    m_proxyAddress.clear();
    m_errorMessage.clear();
    m_hostAddresses.clear();
    m_socketDescriptor = -1;
    m_connectTimer.stop();
    m_disconnectTimer.stop();
    ++m_contextId;
    m_bindPort = 0;
    m_peerPort = 0;
    m_localPort = 0;
    m_proxyPort = 0;
    m_state = TcpSocket::State::Unconnected;
    m_hasToAddSocketToReadyEventSourceListAfterReading = false;
    q->m_readBuffer.clear();
    q->m_writeBuffer.clear();
    q->m_isReadNotificationEnabled = true;
    setReadEnabled(true);
    q->m_isWriteNotificationEnabled = true;
    setWriteEnabled(true);
}

void TcpSocketPrivate::setReadEnabled(bool enabled)
{
    // if (enabled)
    //     setEventTypes(eventTypes() | EPOLLIN);
    // else
    //     setEventTypes(eventTypes() & ~EPOLLIN);
}

void TcpSocketPrivate::setWriteEnabled(bool enabled)
{
    // if (enabled)
    //     setEventTypes(eventTypes() | EPOLLOUT);
    // else
    //     setEventTypes(eventTypes() & ~EPOLLOUT);
}

int TcpSocketPrivate::getSocketOption(TcpSocket::SocketOption option) const
{
    if (m_socketDescriptor < 0)
        return -1;
    int value = -1;
    socklen_t valueLength = sizeof(value);
    switch (option)
    {
        case TcpSocket::SocketOption::LowDelay:
            if (::getsockopt(m_socketDescriptor, IPPROTO_TCP, TCP_NODELAY, &value, &valueLength) == 0)
                return value;
            break;
        case TcpSocket::SocketOption::KeepAlive:
            if (::getsockopt(m_socketDescriptor, SOL_SOCKET, SO_KEEPALIVE, &value, &valueLength) == 0)
                return value;
            break;
        case TcpSocket::SocketOption::SendBufferSize:
            if (::getsockopt(m_socketDescriptor, SOL_SOCKET, SO_SNDBUF, &value, &valueLength) == 0)
                return value;
            break;
        case TcpSocket::SocketOption::ReceiveBufferSize:
            if (::getsockopt(m_socketDescriptor, SOL_SOCKET, SO_RCVBUF, &value, &valueLength) == 0)
                return value;
            break;
    }
    return -1;
}

void TcpSocketPrivate::setSocketOption(TcpSocket::SocketOption option, int value)
{
    if (m_socketDescriptor < 0)
        return;
    switch (option)
    {
        case TcpSocket::SocketOption::LowDelay:
            value = (value != 0) ? 1 : 0;
            ::setsockopt(m_socketDescriptor, IPPROTO_TCP, TCP_NODELAY, &value, sizeof(value));
            break;
        case TcpSocket::SocketOption::KeepAlive:
            value = (value != 0) ? 1 : 0;
            ::setsockopt(m_socketDescriptor, SOL_SOCKET, SO_KEEPALIVE, &value, sizeof(value));
            break;
        case TcpSocket::SocketOption::SendBufferSize:
            if (value < 0)
                return;
            ::setsockopt(m_socketDescriptor, SOL_SOCKET, SO_SNDBUF, &value, sizeof(value));
            break;
        case TcpSocket::SocketOption::ReceiveBufferSize:
            if (value < 0)
                return;
            ::setsockopt(m_socketDescriptor, SOL_SOCKET, SO_RCVBUF, &value, sizeof(value));
            break;
    }
}

void TcpSocketPrivate::bind(std::string_view address, uint16_t port)
{
    m_bindAddress = address;
    m_bindPort = port;
}

void TcpSocketPrivate::connect(std::string_view host, uint16_t port)
{
    Q_Q(TcpSocket);
    const auto bindAddress = m_bindAddress;
    const auto bindPort = m_bindPort;
    abort();
    m_bindAddress = bindAddress;
    m_bindPort = bindPort;
    if (host.empty())
    {
        setError("Failed to connect to host. Given host is empty.");
        return;
    }
    if (port == 0)
    {
        setError(std::string("Failed to connect to ").append(host).append(". Given port is 0."));
        return;
    }
    QHostAddress address;
    m_state = TcpSocket::State::Connecting;
    m_peerPort = port;
    if (address.setAddress(QString::fromLatin1(host.data(), host.size())))
    {
        m_hostAddresses = std::vector<std::string>{std::string(host)};
        onConnecting();
        connectToHost();
    }
    else
    {
        m_peerName = std::string(host);
        m_isLookingUpHost = true;
        onConnecting();
        HostAddressFetcher::addHostLookup(m_peerName, &hostFoundCallback, (void*)this);
    }
}

void TcpSocketPrivate::connectToHost()
{
    Q_Q(TcpSocket);
    while (!m_hostAddresses.empty())
    {
        if (m_socketDescriptor >= 0)
        {
            setEnabled(false);
            UnixUtils::safeClose(m_socketDescriptor);
        }
        m_socketDescriptor = -1;
        m_errorMessage.clear();
        m_peerAddress = m_hostAddresses.front();
        m_hostAddresses.erase(m_hostAddresses.begin());
        QHostAddress peerAddress(QString::fromStdString(m_peerAddress));
        if (!m_bindAddress.empty())
        {
            QHostAddress bindAddress(QString::fromStdString(m_bindAddress));
            if (bindAddress.protocol() == QAbstractSocket::IPv6Protocol
                && peerAddress.protocol() == QAbstractSocket::IPv4Protocol)
                continue;
            struct sockaddr_storage addr;
            std::memset(&addr, 0, sizeof(addr));
            struct sockaddr_in  *addr4;
            struct sockaddr_in6 *addr6;
            switch (bindAddress.protocol())
            {
                case QAbstractSocket::IPv4Protocol:
                    addr4 = (struct sockaddr_in *) &addr;
                    addr4->sin_family = AF_INET;
                    addr4->sin_addr.s_addr = ::htonl(bindAddress.toIPv4Address());
                    addr4->sin_port = ::htons(m_bindPort);
                    m_socketDescriptor = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
                    break;
                case QAbstractSocket::IPv6Protocol:
                {
                    addr6 = (struct sockaddr_in6 *) &addr;
                    addr6->sin6_family = AF_INET6;
                    auto qtIpv6Addr = bindAddress.toIPv6Address();
                    ::memcpy(&addr6->sin6_addr.s6_addr, &qtIpv6Addr, sizeof(qtIpv6Addr));
                    addr6->sin6_port = ::htons(m_bindPort);
                    m_socketDescriptor = ::socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, 0);
                    break;
                }
                case QAbstractSocket::AnyIPProtocol:
                case QAbstractSocket::UnknownNetworkLayerProtocol:
                    continue;
            }
            if (m_socketDescriptor == -1)
            {
                if (bindAddress.protocol() == QAbstractSocket::IPv4Protocol)
                setError(RuntimeError(std::string("Failed to bind socket to ")
                                              .append(bindAddress.protocol() == QAbstractSocket::IPv6Protocol ? "[" : "")
                                              .append(m_bindAddress)
                                              .append(bindAddress.protocol() == QAbstractSocket::IPv6Protocol ? "]" : "")
                                              .append(":")
                                              .append(std::to_string(m_bindPort))
                                              .append("."), RuntimeError::ErrorType::POSIX).error());
                return;
            }
            const int value = 1;
            ::setsockopt(m_socketDescriptor, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
            if (::bind(m_socketDescriptor, (struct sockaddr *) &addr, sizeof(addr)) != 0)
            {
                setError(RuntimeError(std::string("Failed to bind socket to ")
                                          .append(bindAddress.protocol() == QAbstractSocket::IPv6Protocol ? "[" : "")
                                          .append(m_bindAddress)
                                          .append(bindAddress.protocol() == QAbstractSocket::IPv6Protocol ? "]" : "")
                                          .append(":")
                                          .append(std::to_string(m_bindPort))
                                          .append("."), RuntimeError::ErrorType::POSIX).error());
                return;
            }
        }
        else
        {
            switch (peerAddress.protocol())
            {
                case QAbstractSocket::IPv4Protocol:
                    m_socketDescriptor = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
                    break;
                case QAbstractSocket::IPv6Protocol:
                {
                    m_socketDescriptor = ::socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, 0);
                    if (m_socketDescriptor == -1)
                        m_socketDescriptor = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
                    break;
                }
                case QAbstractSocket::AnyIPProtocol:
                case QAbstractSocket::UnknownNetworkLayerProtocol:
                    continue;
            }
            if (m_socketDescriptor == -1)
            {
                setError(RuntimeError(std::string("Failed to connect to ").append(m_peerAddress).append("."), RuntimeError::ErrorType::POSIX).error());
                return;
            }
        }
        assert(m_socketDescriptor >= 0);
        setSocketOption(TcpSocket::SocketOption::LowDelay, 1);
        struct sockaddr_storage addr;
        std::memset(&addr, 0, sizeof(addr));
        struct sockaddr_in  *addr4;
        struct sockaddr_in6 *addr6;
        switch (peerAddress.protocol())
        {
            case QAbstractSocket::IPv4Protocol:
                addr4 = (struct sockaddr_in *) &addr;
                addr4->sin_family = AF_INET;
                addr4->sin_addr.s_addr = ::htonl(peerAddress.toIPv4Address());
                addr4->sin_port = ::htons(m_peerPort);
                break;
            case QAbstractSocket::IPv6Protocol:
            {
                addr6 = (struct sockaddr_in6 *) &addr;
                addr6->sin6_family = AF_INET6;
                auto qtIpv6Addr = peerAddress.toIPv6Address();
                std::memcpy(&addr6->sin6_addr.s6_addr, &qtIpv6Addr, sizeof(qtIpv6Addr));
                addr6->sin6_port = ::htons(m_peerPort);
                break;
            }
            case QAbstractSocket::AnyIPProtocol:
            case QAbstractSocket::UnknownNetworkLayerProtocol:
                continue;
        }
        int result = 0;
        do
        {
            result = ::connect(m_socketDescriptor, (sockaddr*)&addr, sizeof(addr));
        } while (-1 == result && EINTR == errno);
        if (result == 0 || EINPROGRESS == errno)
        {
            m_connectTimer.start(connectTimeoutInMSecs);
            q->setReadChannelNotificationEnabled(true);
            q->setWriteChannelNotificationEnabled(true);
            setEnabled(true);
            return;
        }
        else
            continue;
    }
    const bool isIPv6 = (QHostAddress(QString::fromStdString(m_peerAddress)).protocol() == QAbstractSocket::IPv6Protocol);
    if (m_peerName.empty())
        setError(std::string("Failed to connect to ")
                     .append(isIPv6 ? "[" : "")
                     .append(m_peerAddress)
                     .append(isIPv6 ? "]" : "")
                     .append(":")
                     .append(std::to_string(m_peerPort))
                     .append("."));
    else
        setError(std::string("Failed to connect to ")
                     .append(m_peerName)
                     .append(" at ")
                     .append(isIPv6 ? "[" : "")
                     .append(m_peerAddress)
                     .append(isIPv6 ? "]" : "")
                     .append(":")
                     .append(std::to_string(m_peerPort))
                     .append("."));
}

void TcpSocketPrivate::disconnectFromPeer()
{
    Q_Q(TcpSocket);
    switch (m_state)
    {
        case TcpSocket::State::Unconnected:
        case TcpSocket::State::Disconnecting:
            break;
        case TcpSocket::State::Connecting:
            abort();
            return;
        case TcpSocket::State::Connected:
            q->setReadChannelNotificationEnabled(false);
            setEventTypes(eventTypes() & ~EPOLLIN);
            m_state = TcpSocket::State::Disconnecting;
            m_disconnectTimer.start(disconnectTimeoutInMSecs);
            if (q->m_writeBuffer.isEmpty())
            {
                q->setWriteChannelNotificationEnabled(false);
                setEventTypes(eventTypes() & ~EPOLLOUT);
                if (::shutdown(m_socketDescriptor, SHUT_WR) != 0)
                {
                    if (q->dataAvailable() > 0)
                    {
                        setEnabled(false);
                        UnixUtils::safeClose(m_socketDescriptor);
                        m_socketDescriptor = -1;
                        q->m_writeBuffer.clear();
                        m_state = TcpSocket::State::Unconnected;
                    }
                    else
                        abort();
                    q->disconnected();
                    return;
                }
            }
            break;
    }
}

void TcpSocketPrivate::hostFoundCallback(const std::vector<std::string> &addresses, void *pRawTcpSocketPrivate)
{
    if (pRawTcpSocketPrivate != nullptr)
    {
        auto *pTcpSocket = (TcpSocketPrivate*)pRawTcpSocketPrivate;
        pTcpSocket->m_isLookingUpHost = false;
        pTcpSocket->onHostFound(addresses);
    }
}

void TcpSocketPrivate::onHostFound(const std::vector<std::string> &addresses)
{
    Q_Q(TcpSocket);
    if (!addresses.empty())
    {
        m_hostAddresses = addresses;
        connectToHost();
    }
    else
    {
        setError(std::string("Failed to connect to ").append(m_peerName).append(". Could not fetch any address for domain."));
        return;
    }
}

bool TcpSocketPrivate::fetchConnectionParameters()
{
    struct sockaddr_storage addr;
    std::memset(&addr, 0, sizeof(addr));
    struct sockaddr_in  *addr4;
    struct sockaddr_in6 *addr6;
    socklen_t len = sizeof(addr);
    if (0 == ::getsockname(m_socketDescriptor, reinterpret_cast<struct sockaddr*>(&addr), &len))
    {
        m_localAddress = QHostAddress(reinterpret_cast<struct sockaddr*>(&addr)).toString().toStdString();
        switch (addr.ss_family)
        {
            case AF_INET:
                addr4 = (struct sockaddr_in *) &addr;
                m_localPort = ::ntohs(addr4->sin_port);
                break;
            case AF_INET6:
                addr6 = (struct sockaddr_in6 *) &addr;
                m_localPort = ::ntohs(addr6->sin6_port);
                break;
            default:
                m_errorMessage = "Failed to fetch local IP/Port.";
                return false;
        }
    }
    else
    {
        m_errorMessage = RuntimeError("Failed to fetch local IP/port.", RuntimeError::ErrorType::POSIX).error();
        return false;
    }
    std::memset(&addr, 0, sizeof(addr));
    len = sizeof(addr);
    if (0 == ::getpeername(m_socketDescriptor, reinterpret_cast<struct sockaddr*>(&addr), &len))
    {
        m_peerAddress = QHostAddress(reinterpret_cast<struct sockaddr*>(&addr)).toString().toStdString();
        switch (addr.ss_family)
        {
            case AF_INET:
                addr4 = (struct sockaddr_in *) &addr;
                m_peerPort = ::ntohs(addr4->sin_port);
                break;
            case AF_INET6:
                addr6 = (struct sockaddr_in6 *) &addr;
                m_peerPort = ::ntohs(addr6->sin6_port);
                break;
            default:
                m_errorMessage = "Failed to fetch peer IP/port.";
                return false;
        }
    }
    else
    {
        m_errorMessage = RuntimeError("Failed to fetch peer IP/port.", RuntimeError::ErrorType::POSIX).error();
        return false;
    }
    return true;
}

void TcpSocketPrivate::setError(std::string_view errorMessage)
{
    Q_Q(TcpSocket);
    m_errorMessage = errorMessage;
    const auto contextId = m_contextId;
    q->error();
    if (contextId == m_contextId)
    {
        abort();
        m_errorMessage = errorMessage;
    }
}

void TcpSocketPrivate::onEvent(const uint32_t epollEvents)
{
    Q_Q(TcpSocket);
    size_t receivedDataSize = 0;
    size_t sentDataSize = 0;
    bool hasDisconnected = false;
    if ((epollEvents & EPOLLIN) && (m_state == TcpSocket::State::Connected))
        receivedDataSize = q->readDataFromChannel();
    if (epollEvents & EPOLLOUT)
    {
        if (m_state == TcpSocket::State::Connected)
            sentDataSize = q->writeDataToChannel();
        else if (m_state == TcpSocket::State::Disconnecting)
        {
            sentDataSize = q->writeDataToChannel();
            if (q->m_writeBuffer.isEmpty())
            {
                q->setWriteChannelNotificationEnabled(false);
                setEventTypes(eventTypes() & ~EPOLLOUT);
                if (::shutdown(m_socketDescriptor, SHUT_WR) != 0)
                {
                    m_disconnectTimer.stop();
                    hasDisconnected = true;
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
        while (contextId == m_contextId && m_tcpSocketDataSource.dataAvailable() > 0 && q->readDataFromChannel() > 0)
            q->receivedData();
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
                q->m_writeBuffer.clear();
                m_state = TcpSocket::State::Unconnected;
            }
            else
                abort();
            if (hasToEmitDisconnected)
                q->disconnected();
        }
    }
}

void TcpSocketPrivate::onConnected()
{
    Q_Q(TcpSocket);
    q->connected();
}

void TcpSocketPrivate::onConnectTimeout()
{
    if (m_state == TcpSocket::State::Connecting)
        connectToHost();
    return;
}

void TcpSocketPrivate::onDisconnectTimeoutImpl()
{
    Q_Q(TcpSocket);
    if (m_state == TcpSocket::State::Disconnecting)
    {
        if (q->dataAvailable() > 0)
        {
            setEnabled(false);
            UnixUtils::safeClose(m_socketDescriptor);
            m_socketDescriptor = -1;
            q->m_writeBuffer.clear();
            m_state = TcpSocket::State::Unconnected;
        }
        else
            abort();
        q->disconnected();
    }
}

//
//
//
// TcpSocket methods
//
//
//

TcpSocket::TcpSocket()
{
    d_ptr = new TcpSocketPrivate;
    d_ptr->q_ptr = this;
}

TcpSocket::TcpSocket(int64_t socketDescriptor)
{
    d_ptr = new TcpSocketPrivate;
    d_ptr->q_ptr = this;
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

TcpSocket::~TcpSocket()
{
    Q_D(TcpSocket);
    d->TcpSocketPrivate::abort();
    d_ptr->scheduleForDeletion();
}

size_t TcpSocket::read(char *pBuffer, size_t maxSize)
{
    Q_D(TcpSocket);
    auto bytesRead = IOChannel::read(pBuffer, maxSize);
    if (d->m_hasToAddSocketToReadyEventSourceListAfterReading)
    {
        d->eventNotifier()->postEvent(d, EPOLLIN);
        d->m_hasToAddSocketToReadyEventSourceListAfterReading = false;
    }
    return bytesRead;
}

size_t TcpSocket::write(const char *pData, size_t maxSize)
{
    assert(pData != nullptr);
    Q_D(TcpSocket);
    if (d->m_state == TcpSocket::State::Connected)
    {
        m_writeBuffer.write(pData, maxSize);
        if (!d->m_hasAlreadyScheduledWriteEvent)
        {
            d->eventNotifier()->postEvent(d, EPOLLOUT);
            d->m_hasAlreadyScheduledWriteEvent = true;
        }
        return maxSize;
    }
    else
        return 0;
}

std::string_view TcpSocket::readAll()
{
    Q_D(TcpSocket);
    const bool wasFull = m_readBuffer.isFull();
    auto data = m_readBuffer.readAll();
    if (wasFull)
        d->eventNotifier()->postEvent(d, EPOLLIN);
    return data;
}

size_t TcpSocket::skip(size_t maxSize)
{
    Q_D(TcpSocket);
    const bool wasFull = m_readBuffer.isFull();
    const auto poppedBytes = m_readBuffer.popFront(maxSize);
    if (wasFull && poppedBytes > 0)
        d->eventNotifier()->postEvent(d, EPOLLIN);
    return poppedBytes;
}

void TcpSocket::setBindAddressAndPort(std::string_view address, uint16_t port)
{
    Q_D(TcpSocket);
    d->bind(address, port);
}

void TcpSocket::connect(std::string_view host, uint16_t port)
{
    Q_D(TcpSocket);
    try
    {
        d->connect(host, port);
    }
    catch (const RuntimeError &runtimeError)
    {
        d->setError(runtimeError.error());
    }
}

void TcpSocket::disconnectFromPeer()
{
    Q_D(TcpSocket);
    try
    {
        d->disconnectFromPeer();
    }
    catch (const RuntimeError &runtimeError)
    {
        d->setError(runtimeError.error());
    }
}

void TcpSocket::abort()
{
    Q_D(TcpSocket);
    d->abort();
}

std::string_view TcpSocket::errorMessage() const
{
    Q_D(const TcpSocket);
    return d->errorMessage();
}

std::string_view TcpSocket::localAddress() const
{
    Q_D(const TcpSocket);
    return d->localAddress();
}

uint16_t TcpSocket::localPort() const
{
    Q_D(const TcpSocket);
    return d->localPort();
}

std::string_view TcpSocket::peerName() const
{
    Q_D(const TcpSocket);
    return d->peerName();
}

std::string_view TcpSocket::peerAddress() const
{
    Q_D(const TcpSocket);
    return d->peerAddress();
}

uint16_t TcpSocket::peerPort() const
{
    Q_D(const TcpSocket);
    return d->peerPort();
}

std::string_view TcpSocket::proxyAddress() const
{
    Q_D(const TcpSocket);
    return d->proxyAddress();
}

uint16_t TcpSocket::proxyPort() const
{
    Q_D(const TcpSocket);
    return d->proxyPort();
}

TcpSocket::State TcpSocket::state() const
{
    Q_D(const TcpSocket);
    return d->state();
}

int TcpSocket::getSocketOption(SocketOption option) const
{
    Q_D(const TcpSocket);
    return d->getSocketOption(option);
}

void TcpSocket::setSocketOption(SocketOption option, int value)
{
    Q_D(TcpSocket);
    d->setSocketOption(option, value);
}

Signal TcpSocket::connected() KOURIER_SIGNAL(&TcpSocket::connected)
Signal TcpSocket::disconnected() KOURIER_SIGNAL(&TcpSocket::disconnected)
Signal TcpSocket::error() KOURIER_SIGNAL(&TcpSocket::error)

TcpSocket::TcpSocket(TcpSocketPrivate *pTcpSocketPrivate)
{
    d_ptr = pTcpSocketPrivate;
    d_ptr->q_ptr = this;
}

//
//
//
// TcpSocket methods inherited from IOChannel
//
//
//

size_t TcpSocket::readDataFromChannel()
{
    Q_D(TcpSocket);
    const auto bytesRead = IOChannel::readDataFromChannel();
    if (dataSource().dataAvailable() > 0)
    {
        if (!m_readBuffer.isFull())
            d->eventNotifier()->postEvent(d, EPOLLIN);
        else
            d->m_hasToAddSocketToReadyEventSourceListAfterReading = true;
    }
    return bytesRead;
}

size_t TcpSocket::writeDataToChannel()
{
    Q_D(TcpSocket);
    const auto bytesWritten = IOChannel::writeDataToChannel();
    d->m_hasAlreadyScheduledWriteEvent = false;
    return bytesWritten;
}

DataSource &TcpSocket::dataSource()
{
    Q_D(TcpSocket);
    return d->tcpSocketDataSource();
}

DataSink &TcpSocket::dataSink()
{
    Q_D(TcpSocket);
    return d->tcpSocketDataSink();
}

void TcpSocket::onReadNotificationChanged()
{
    Q_D(TcpSocket);
    d->setReadEnabled(isReadNotificationEnabled());
}

void TcpSocket::onWriteNotificationChanged()
{
    Q_D(TcpSocket);
    d->setWriteEnabled(isWriteNotificationEnabled());
}

}
