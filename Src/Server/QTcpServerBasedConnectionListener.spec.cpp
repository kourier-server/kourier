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
#include <QVariant>
#include <QHostAddress>
#include <QMap>
#include <QTcpSocket>
#include <QSemaphore>
#include <vector>
#include <Spectator.h>
#include <sys/socket.h>
#include <netinet/in.h>


using Kourier::QTcpServerBasedConnectionListener;
using Kourier::ConnectionListener;
using Kourier::Object;
using Spectator::SemaphoreAwaiter;


SCENARIO("QTcpServerBasedConnectionListener listens for incoming connections")
{
    GIVEN("a valid address/port combination")
    {
        auto address = GENERATE(AS(std::string_view), "127.100.100.15", "127.110.110.15", "127.100.100.35");
        QTcpSocket socket;
        REQUIRE(socket.bind(QHostAddress(QString::fromLatin1(address))));
        const auto port = socket.localPort();
        REQUIRE(port > 0 && port <= 65535);
        socket.abort();
        QMap<QString, QVariant> connectionListenerData;
        connectionListenerData.insert(qUtf8Printable("address"), QByteArray(address.data()));
        connectionListenerData.insert(qUtf8Printable("port"), QVariant::fromValue(port));
        const auto backlogSize = GENERATE(AS(int), 0, 10, 25, 1500);
        if (backlogSize > 0)
            connectionListenerData.insert(qUtf8Printable("backlogSize"), QVariant::fromValue(backlogSize));

        WHEN("connection listener starts to listen for incoming connections on the given address/port")
        {
            QTcpServerBasedConnectionListener connectionListener;
            const auto listeningSucceeded = connectionListener.start(QVariant::fromValue(connectionListenerData));

            THEN("listener successfully listens on given address/port")
            {
                REQUIRE(listeningSucceeded);
                REQUIRE(connectionListener.errorMessage().empty());
                REQUIRE(backlogSize == 0 || backlogSize == connectionListener.backlogSize());

                AND_WHEN("client connects to server")
                {
                    QSemaphore emittedNewConnectionSemaphore;
                    QList<qintptr> socketDescriptors;
                    Object::connect(&connectionListener, &ConnectionListener::newConnection, [&emittedNewConnectionSemaphore, &socketDescriptors](qintptr socketDescriptor)
                    {
                        REQUIRE(!socketDescriptors.contains(socketDescriptor));
                        socketDescriptors.append(socketDescriptor);
                        emittedNewConnectionSemaphore.release();
                    });
                    const auto clientsToConnect = GENERATE(AS(int), 1, 3, 5);
                    std::vector<QTcpSocket> clients(clientsToConnect);
                    for (auto &client : clients)
                        client.connectToHost(QHostAddress(QString::fromLatin1(address)), port);

                    THEN("server emits newConnection signal as many times as clients connected to server")
                    {
                        for (auto i = 0; i < clientsToConnect; ++i)
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(emittedNewConnectionSemaphore, 1));
                        }
                    }
                }
            }
        }
    }

    GIVEN("a valid socket descriptor")
    {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        const auto socketDescriptor = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        REQUIRE(socketDescriptor >= 0);
        REQUIRE(::bind(socketDescriptor, (struct sockaddr*) &addr, sizeof(addr)) == 0);
        REQUIRE(::listen(socketDescriptor, 100) == 0);
        QMap<QString, QVariant> connectionListenerData;
        connectionListenerData.insert(qUtf8Printable("socketDescriptor"), QVariant::fromValue<qintptr>(socketDescriptor));

        WHEN("connection listener starts to listen for incoming connections on the given address/port")
        {
            QTcpServerBasedConnectionListener connectionListener;
            const auto listeningSucceeded = connectionListener.start(QVariant::fromValue(connectionListenerData));

            THEN("listener successfully listens on given address/port")
            {
                REQUIRE(listeningSucceeded);
                REQUIRE(connectionListener.errorMessage().empty());
                REQUIRE(connectionListener.socketDescriptor() == socketDescriptor);
            }
        }
    }

    GIVEN("an invalid address")
    {
        const auto invalidAddress = GENERATE(AS(QVariant),
                                             QVariant(QString("127.0.0.1")),
                                             QVariant(int(3)),
                                             QVariant(QByteArray("invalid address")));

        WHEN("server is set to listen for incoming connections")
        {
            QMap<QString, QVariant> connectionListenerData;
            connectionListenerData.insert(qUtf8Printable("address"), invalidAddress);
            connectionListenerData.insert(qUtf8Printable("port"), 35780);

            THEN("server fails to listen for incoming connections")
            {
                QTcpServerBasedConnectionListener connectionListener;
                REQUIRE(!connectionListener.start(QVariant::fromValue(connectionListenerData)));
                REQUIRE(connectionListener.errorMessage().starts_with("Failed to start connection listener."));
            }
        }
    }

    GIVEN("an invalid backlog size")
    {
        const auto invalidBacklogSize = GENERATE(AS(QVariant),
                                                 QVariant(int(0)),
                                                 QVariant(int(-1)),
                                                 QVariant(int(-3)),
                                                 QVariant(qintptr(150)));

        WHEN("server is set to listen for incoming connections")
        {
            std::string_view address("127.10.20.35");
            QTcpSocket socket;
            REQUIRE(socket.bind(QHostAddress(QString::fromLatin1(address))));
            const auto port = socket.localPort();
            REQUIRE(port > 0 && port <= 65535);
            socket.abort();
            QMap<QString, QVariant> connectionListenerData;
            connectionListenerData.insert(qUtf8Printable("address"), QByteArray(address.data()));
            connectionListenerData.insert(qUtf8Printable("port"), QVariant::fromValue(port));
            const auto setInvalidBacklogSize = GENERATE(AS(bool), true, false);
            if (setInvalidBacklogSize)
                connectionListenerData.insert(qUtf8Printable("backlogSize"), invalidBacklogSize);
            QTcpServerBasedConnectionListener connectionListener;

            THEN("server fails to listen for incoming connections if invalid backlog size is set")
            {
                REQUIRE(setInvalidBacklogSize != connectionListener.start(QVariant::fromValue(connectionListenerData)));
                if (setInvalidBacklogSize)
                {
                    REQUIRE(connectionListener.errorMessage().starts_with("Failed to start connection listener."));
                }
                else
                {
                    REQUIRE(connectionListener.errorMessage().empty());
                }
            }
        }
    }

    GIVEN("an invalid socket descriptor")
    {
        const auto invalidSocketDescriptor = GENERATE(AS(QVariant),
                                                 QVariant(qintptr(-1)),
                                                 QVariant(qintptr(-3)),
                                                 QVariant(int(150)));

        WHEN("server is set to listen for incoming connections")
        {
            std::string_view address("127.10.20.35");
            QTcpSocket socket;
            REQUIRE(socket.bind(QHostAddress(QString::fromLatin1(address))));
            const auto port = socket.localPort();
            REQUIRE(port > 0 && port <= 65535);
            socket.abort();
            QMap<QString, QVariant> connectionListenerData;
            connectionListenerData.insert(qUtf8Printable("address"), QByteArray(address.data()));
            connectionListenerData.insert(qUtf8Printable("port"), QVariant::fromValue(port));
            const auto setInvalidSocketDescriptor = GENERATE(AS(bool), true, false);
            if (setInvalidSocketDescriptor)
                connectionListenerData.insert(qUtf8Printable("socketDescriptor"), invalidSocketDescriptor);
            QTcpServerBasedConnectionListener connectionListener;

            THEN("server fails to listen for incoming connections if invalid socket descriptor is set")
            {
                REQUIRE(setInvalidSocketDescriptor != connectionListener.start(QVariant::fromValue(connectionListenerData)));
                if (setInvalidSocketDescriptor)
                {
                    REQUIRE(connectionListener.errorMessage().starts_with("Failed to start connection listener."));
                }
                else
                {
                    REQUIRE(connectionListener.errorMessage().empty());
                }
            }
        }
    }
}
