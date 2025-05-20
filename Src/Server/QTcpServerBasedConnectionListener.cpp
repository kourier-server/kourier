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

#include "QTcpServerBasedConnectionListener.h"
#include "QTcpServerBasedConnectionListenerPrivate.h"
#include "../Core/NoDestroy.h"
#include <QHostAddress>
#include <QMutex>
#include <sys/socket.h>
#include <netinet/in.h>


namespace Kourier
{

QTcpServerBasedConnectionListener::QTcpServerBasedConnectionListener() :
    m_pListener(new QTcpServerBasedConnectionListenerPrivate(this))
{
}

QTcpServerBasedConnectionListener::~QTcpServerBasedConnectionListener()
{
}

bool QTcpServerBasedConnectionListener::start(QVariant data)
{
    if (m_hasAlreadyStarted)
    {
        m_errorMessage = "Failed to start connection listener. Connection listener has already started.";
        return false;
    }
    m_hasAlreadyStarted = true;
    if (data.typeId() != QMetaType::QVariantMap)
    {
        m_errorMessage = "Failed to start connection listener. Given data is not a QVariantMap.";
        return false;
    }
    const auto variantMap = data.toMap();
    if (variantMap.contains("backlogSize"))
    {
        if (variantMap["backlogSize"].typeId() != QMetaType::Int)
        {
            m_errorMessage = "Failed to start connection listener. Given backlogSize must be an integer.";
            return false;
        }
        const auto backlogSize = variantMap["backlogSize"].toInt();
        if (backlogSize <= 0)
        {
            m_errorMessage = "Failed to start connection listener. Given backlogSize is not a positive integer.";
            return false;
        }
        else
            m_pListener->setListenBacklogSize(backlogSize);
    }
    if (variantMap.contains("socketDescriptor"))
    {
        if (variantMap["socketDescriptor"].typeId() != qMetaTypeId<qintptr>())
        {
            m_errorMessage = "Failed to start connection listener. Given socketDescriptor must be a qintptr.";
            return false;
        }
        else
        {
            if (m_pListener->setSocketDescriptor(variantMap["socketDescriptor"].value<qintptr>()))
                return true;
            else
            {
                m_errorMessage = std::string("Failed to start connection listener. ").append(m_pListener->errorString().toStdString());
                return false;
            }
        }
    }
    if (!variantMap.contains("address"))
    {
        m_errorMessage = "Failed to start connection listener. Given data does not contain an address.";
        return false;
    }
    if (!variantMap.contains("port"))
    {
        m_errorMessage = "Failed to start connection listener. Given data does not contain a port.";
        return false;
    }
    if (variantMap["address"].typeId() != QMetaType::QByteArray)
    {
        m_errorMessage = "Failed to start connection listener. Given address must be a QByteArray.";
        return false;
    }
    const QHostAddress address(QString::fromLatin1(variantMap["address"].toByteArray()));
    if (address.isNull())
    {
        m_errorMessage = "Failed to start connection listener. Given address is not valid.";
        return false;
    }
    if (variantMap["port"].typeId() != QMetaType::UShort)
    {
        m_errorMessage = "Failed to start connection listener. Given port must be a quint16.";
        return false;
    }
    const auto port = variantMap["port"].value<quint16>();
    if (port == 0)
    {
        m_errorMessage = "Failed to start connection listener. Given port must be positive.";
        return false;
    }
    else
    {
        static NoDestroy<QMutex> mutex;
        QMutexLocker locker(&mutex());
        if (address.isNull() || port == 0 || port > 65535)
        {
            m_errorMessage = "Failed to create listening socket. Invalid address/port.";
            return false;
        }
        struct sockaddr_storage addr{0};
        struct sockaddr_in  *addr4;
        struct sockaddr_in6 *addr6;
        int domain;
        if (address.protocol() == QHostAddress::IPv4Protocol)
        {
            domain = AF_INET;
            addr4 = (struct sockaddr_in *) &addr;
            addr4->sin_family = AF_INET;
            addr4->sin_addr.s_addr = htonl(address.toIPv4Address());
            addr4->sin_port = htons(port);
        }
        else
        {
            domain = AF_INET6;
            addr6 = (struct sockaddr_in6 *) &addr;
            addr6->sin6_family = AF_INET6;
            auto qtIpv6Addr = address.toIPv6Address();
            memcpy(&addr6->sin6_addr.s6_addr, &qtIpv6Addr, sizeof(qtIpv6Addr));
            addr6->sin6_port = htons(port);
        }
        const int socketFd = socket(domain, SOCK_STREAM, 0);
        if (socketFd < 0)
        {
            m_errorMessage = "Failed to create listening socket.";
            return false;
        }
        const int option = 1;
        if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEPORT, &option, sizeof(option)))
        {
            m_errorMessage = "Failed to set SO_REUSEPORT option on socket.";
            return false;
        }
        QDeadlineTimer deadlineTimer(20000);
        while(bind(socketFd, (struct sockaddr *) &addr, sizeof(addr)) && !deadlineTimer.hasExpired())
            QThread::msleep(100);
        if (deadlineTimer.hasExpired())
        {
            m_errorMessage = "Failed to bind receive socket";
            return false;
        }
        if (listen(socketFd, m_pListener->listenBacklogSize()))
        {
            m_errorMessage = "Failed to make socket listen for connections.";
            return false;
        }
        if (m_pListener->setSocketDescriptor(socketFd))
            return true;
        else
        {
            m_errorMessage = std::string("Failed to start Connection listener. QTcpServer::setSocketDescriptor failed. ").append(m_pListener->errorString().toStdString());
            return false;
        }
    }
}

int QTcpServerBasedConnectionListener::backlogSize() const
{
    return m_pListener->listenBacklogSize();
}

qintptr QTcpServerBasedConnectionListener::socketDescriptor() const
{
    return m_pListener->socketDescriptor();
}

}
