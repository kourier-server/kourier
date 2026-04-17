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
#include <Tests/Resources/TcpServer.h>
#include <Tests/Resources/TestHostNamesFetcher.h>
#include <QTcpSocket>
#include <QTcpServer>
#include <QSemaphore>
#include <QElapsedTimer>
#include <QDeadlineTimer>
#include <QRandomGenerator64>
#include <QHostInfo>
#include <QHostAddress>
#include <chrono>
#include <memory>
#include <sys/socket.h>
#include <sys/mman.h>
#include <Spectator.h>

using Kourier::TcpServer;
using Kourier::TcpSocket;
using Kourier::Object;
using Kourier::TestResources::TestHostNamesFetcher;


namespace TcpSocketTests
{

const static QByteArray largeData = []
{
    static QVector<quint64> dataVector(125000);
    QRandomGenerator64::global()->fillRange(dataVector.data(), dataVector.size());
    const static auto data = QByteArray::fromRawData(reinterpret_cast<const char*>(dataVector.data()), dataVector.size() * sizeof(qint64));
    return data;
}();
}

using namespace TcpSocketTests;


SCENARIO("TcpSocket connects to server, sends a PING and gets a PONG as response")
{
    GIVEN("a running server")
    {
        auto [hostName, hostAddresses] = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses();
        const auto connectByName = GENERATE(AS(bool), true, false);
        TcpServer server;
        const auto serverAddress = GENERATE(AS(QHostAddress),
                                               QHostAddress("127.10.20.50"),
                                               QHostAddress("127.10.20.60"),
                                               QHostAddress("127.10.20.70"),
                                               QHostAddress("127.10.20.80"),
                                               QHostAddress("127.10.20.90"),
                                               QHostAddress("::1"));
        REQUIRE(hostAddresses.contains(serverAddress));
        REQUIRE(server.listen(serverAddress));
        std::unique_ptr<TcpSocket> serverPeer;
        QSemaphore serverPeerConnectedSemaphore;
        QSemaphore serverPeerDisconnectedSemaphore;
        QSemaphore serverPeerReceivedPingSemaphore;
        QByteArray serverPeerReceivedData;
        Object::connect(&server, &TcpServer::newConnection, [&](TcpSocket *pNewSocket)
            {
                REQUIRE(!serverPeer);
                serverPeer.reset(pNewSocket);
                Object::connect(serverPeer.get(), &TcpSocket::disconnected, [&](){serverPeerDisconnectedSemaphore.release();});
                Object::connect(serverPeer.get(), &TcpSocket::receivedData, [&]()
                    {
                        serverPeerReceivedData.append(serverPeer->readAll());
                        if (serverPeerReceivedData == "PING")
                        {
                            serverPeer->write("PONG");
                            serverPeer->disconnectFromPeer();
                            serverPeerReceivedPingSemaphore.release();
                        }
                        else if (serverPeerReceivedData.size() >= 4)
                        {
                            FAIL("Server peer expects a single PING message.");
                        }
                    });
                serverPeerConnectedSemaphore.release();
            });

        WHEN("TcpSocket connects to server")
        {
            TcpSocket clientPeer;
            QSemaphore clientPeerConnectedSemaphore;
            QSemaphore clientPeerDisconnectedSemaphore;
            QSemaphore clientPeerReceivedPongSemaphore;
            QByteArray clientPeerReceivedData;
            Object::connect(&clientPeer, &TcpSocket::connected, [&](){clientPeerConnectedSemaphore.release();});
            Object::connect(&clientPeer, &TcpSocket::disconnected, [&](){clientPeerDisconnectedSemaphore.release();});
            Object::connect(&clientPeer, &TcpSocket::receivedData, [&]()
                {
                    clientPeerReceivedData.append(clientPeer.readAll());
                    if (clientPeerReceivedData == "PONG")
                        clientPeerReceivedPongSemaphore.release();
                    else if (clientPeerReceivedData.size() >= 4)
                    {
                        FAIL("Client peer expects a single PONG message.");
                    }
                });
            if (connectByName)
                clientPeer.connect(hostName, server.serverPort());
            else
                 clientPeer.connect(serverAddress.toString().toStdString(), server.serverPort());

            THEN("client connects to server peer")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));

                AND_WHEN("client peer sends a PING message to the server peer")
                {
                    clientPeer.write("PING");

                    THEN("server peer responds with a PONG message and closes the connection")
                    {
                        REQUIRE(TRY_ACQUIRE(serverPeerReceivedPingSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(clientPeerReceivedPongSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                    }
                }
            }
        }
    }
}


SCENARIO("TcpSocket allows binding to an address/port prior to connecting to server")
{
    GIVEN("a running server")
    {
        const auto testIpv4 = GENERATE(AS(bool), true, false);
        auto [hostName, hostAddresses] = testIpv4 ? TestHostNamesFetcher::hostNameWithIpv4Address() : TestHostNamesFetcher::hostNameWithIpv6Address();
        REQUIRE(hostAddresses.size() == 1);
        const auto serverAddress = hostAddresses[0];
        TcpServer server;
        REQUIRE(server.listen(serverAddress));
        std::unique_ptr<TcpSocket> serverPeer;
        QSemaphore serverPeerConnectedSemaphore;
        QSemaphore serverPeerDisconnectedSemaphore;
        QSemaphore serverPeerReceivedPingSemaphore;
        QByteArray serverPeerReceivedData;
        Object::connect(&server, &TcpServer::newConnection, [&](TcpSocket *pNewSocket)
            {
                REQUIRE(!serverPeer);
                serverPeer.reset(pNewSocket);
                Object::connect(serverPeer.get(), &TcpSocket::disconnected, [&](){serverPeerDisconnectedSemaphore.release();});
                Object::connect(serverPeer.get(), &TcpSocket::receivedData, [&]()
                    {
                        serverPeerReceivedData.append(serverPeer->readAll());
                        if (serverPeerReceivedData == "PING")
                        {
                            serverPeer->write("PONG");
                            serverPeer->disconnectFromPeer();
                            serverPeerReceivedPingSemaphore.release();
                        }
                        else if (serverPeerReceivedData.size() >= 4)
                        {
                            FAIL("Server peer expects a single PING message.");
                        }
                    });
                serverPeerConnectedSemaphore.release();
            });

        WHEN("TcpSocket binds to address/port prior to connecting to running server")
        {
            const auto bindWithExplicitPort = GENERATE(AS(bool), true, false);
            const static QHostAddress bindIpv4Address("127.4.8.12");
            const static QHostAddress bindIpv6Address("::1");
            const auto bindAddress = testIpv4 ? bindIpv4Address : bindIpv6Address;
            constexpr static auto fetchAvailablePort = [](QHostAddress address) -> uint16_t
            {
                QTcpSocket socket;
                Spectator::REQUIRE(socket.bind(address));
                const auto availablePort = socket.localPort();
                socket.abort();
                return availablePort;
            };
            const auto bindPort = bindWithExplicitPort ? fetchAvailablePort(bindAddress) : 0;
            TcpSocket clientPeer;
            clientPeer.setBindAddressAndPort(bindAddress.toString().toStdString(), bindPort);
            QSemaphore clientPeerConnectedSemaphore;
            QSemaphore clientPeerDisconnectedSemaphore;
            QSemaphore clientPeerReceivedPongSemaphore;
            QByteArray clientPeerReceivedData;
            Object::connect(&clientPeer, &TcpSocket::connected, [&](){clientPeerConnectedSemaphore.release();});
            Object::connect(&clientPeer, &TcpSocket::disconnected, [&](){clientPeerDisconnectedSemaphore.release();});
            Object::connect(&clientPeer, &TcpSocket::receivedData, [&]()
                {
                    clientPeerReceivedData.append(clientPeer.readAll());
                    if (clientPeerReceivedData == "PONG")
                        clientPeerReceivedPongSemaphore.release();
                    else if (clientPeerReceivedData.size() >= 4)
                    {
                        FAIL("Client peer expects a single PONG message.");
                    }
                });
            const auto connectByName = GENERATE(AS(bool), true, false);
            if (connectByName)
                clientPeer.connect(hostName, server.serverPort());
            else
                clientPeer.connect(serverAddress.toString().toStdString(), server.serverPort());

            THEN("server connects to client and client emits the connected signal")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                REQUIRE(clientPeer.localAddress() == bindAddress.toString().toStdString());
                if (bindPort != 0)
                {
                    REQUIRE(clientPeer.localPort() == bindPort);
                }

                AND_WHEN("client peer sends a ping message to the server peer")
                {
                    clientPeer.write("PING");

                    THEN("server peer responds with a PONG message and closes the connection")
                    {
                        REQUIRE(TRY_ACQUIRE(serverPeerReceivedPingSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(clientPeerReceivedPongSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                    }
                }
            }
        }
    }
}


SCENARIO("TcpSocket allows OS-level socket options to be set")
{
    GIVEN("two TcpSockets configured after connecting to each other")
    {
        TcpServer server;
        REQUIRE(server.listen(QHostAddress::LocalHost));
        std::unique_ptr<TcpSocket> pServerPeer;
        QSemaphore serverPeerConnectedSemaphore;
        QSemaphore serverPeerDisconnectedSemaphore;
        QSemaphore serverPeerReceivedPingSemaphore;
        QByteArray receivedServerPeerData;
        Object::connect(&server, &TcpServer::newConnection, [&](TcpSocket *pNewSocket)
            {
                REQUIRE(!pServerPeer);
                pServerPeer.reset(pNewSocket);
                Object::connect(pServerPeer.get(), &TcpSocket::disconnected, [&](){serverPeerDisconnectedSemaphore.release();});
                Object::connect(pServerPeer.get(), &TcpSocket::receivedData, [&]()
                    {
                        receivedServerPeerData.append(pServerPeer->readAll());
                        if (receivedServerPeerData == "PING")
                        {
                            pServerPeer->write("PONG");
                            pServerPeer->disconnectFromPeer();
                            serverPeerReceivedPingSemaphore.release();
                        }
                        else if (receivedServerPeerData.size() >= 4)
                        {
                            FAIL("Server peer expects a single PING message.");
                        }
                    });
                serverPeerConnectedSemaphore.release();
            });
        std::unique_ptr<TcpSocket> pClientPeer(new TcpSocket);
        QSemaphore clientPeerConnectedSemaphore;
        QSemaphore clientPeerDisconnectedSemaphore;
        QSemaphore clientPeerReceivedPongSemaphore;
        QByteArray receivedClientPeerData;
        Object::connect(pClientPeer.get(), &TcpSocket::connected, [&](){clientPeerConnectedSemaphore.release();});
        Object::connect(pClientPeer.get(), &TcpSocket::disconnected, [&](){clientPeerDisconnectedSemaphore.release();});
        Object::connect(pClientPeer.get(), &TcpSocket::receivedData, [&]()
            {
                receivedClientPeerData.append(pClientPeer->readAll());
                if (receivedClientPeerData == "PONG")
                    clientPeerReceivedPongSemaphore.release();
                else if (receivedClientPeerData.size() >= 4)
                {
                    FAIL("Client peer expects a single PONG message.");
                }
            });
        pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());
        REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
        REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
        const auto disableLowDelayOption = GENERATE(AS(bool), true, false);
        const auto setKeepAliveOption = GENERATE(AS(bool), true, false);
        const auto readBufferCapacity = GENERATE(AS(size_t), 0, 1024, 16384, 65536);
        if (readBufferCapacity > 0)
        {
            pClientPeer->setReadBufferCapacity(readBufferCapacity);
            pServerPeer->setReadBufferCapacity(readBufferCapacity);
        }
        if (disableLowDelayOption)
        {
            pClientPeer->setSocketOption(TcpSocket::SocketOption::LowDelay, 0);
            pServerPeer->setSocketOption(TcpSocket::SocketOption::LowDelay, 0);
        }
        REQUIRE(pClientPeer->getSocketOption(TcpSocket::SocketOption::LowDelay) == disableLowDelayOption ? 0 : 1);
        REQUIRE(pServerPeer->getSocketOption(TcpSocket::SocketOption::LowDelay) == disableLowDelayOption ? 0 : 1);
        if (setKeepAliveOption)
        {
            pClientPeer->setSocketOption(TcpSocket::SocketOption::KeepAlive, 1);
            pServerPeer->setSocketOption(TcpSocket::SocketOption::KeepAlive, 1);
        }
        REQUIRE(pClientPeer->getSocketOption(TcpSocket::SocketOption::KeepAlive) == setKeepAliveOption ? 1 : 0);
        REQUIRE(pServerPeer->getSocketOption(TcpSocket::SocketOption::KeepAlive) == setKeepAliveOption ? 1 : 0);

        WHEN("client peer sends PING message to server peer")
        {
            pClientPeer->write("PING");

            THEN("server peer responds with a PONG message and closes the connection")
            {
                REQUIRE(TRY_ACQUIRE(serverPeerReceivedPingSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(clientPeerReceivedPongSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
            }
        }
    }
}


SCENARIO("Connected TcpSocket peers interact with each other")
{
    GIVEN("two connected TcpSockets")
    {
        TcpServer server;
        REQUIRE(server.listen(QHostAddress::LocalHost));
        std::unique_ptr<TcpSocket> pServerPeer;
        QSemaphore serverPeerConnectedSemaphore;
        QSemaphore serverPeerDisconnectedSemaphore;
        QSemaphore serverPeerFailedSemaphore;
        QSemaphore serverPeerReceivedDataSemaphore;
        QByteArray receivedServerPeerData;
        Object::connect(&server, &TcpServer::newConnection, [&](TcpSocket *pNewSocket)
            {
                REQUIRE(!pServerPeer);
                pServerPeer.reset(pNewSocket);
                Object::connect(pServerPeer.get(), &TcpSocket::disconnected, [&](){serverPeerDisconnectedSemaphore.release();});
                Object::connect(pServerPeer.get(), &TcpSocket::error, [&](){serverPeerFailedSemaphore.release();});
                Object::connect(pServerPeer.get(), &TcpSocket::receivedData, [&]()
                    {
                        receivedServerPeerData.append(pServerPeer->readAll());
                        serverPeerReceivedDataSemaphore.release();
                    });
                serverPeerConnectedSemaphore.release();
            });
        std::unique_ptr<TcpSocket> pClientPeer(new TcpSocket);
        QSemaphore clientPeerConnectedSemaphore;
        QSemaphore clientPeerDisconnectedSemaphore;
        QSemaphore clientPeerFailedSemaphore;
        QSemaphore clientPeerReceivedDataSemaphore;
        QByteArray receivedClientPeerData;
        Object::connect(pClientPeer.get(), &TcpSocket::connected, [&](){clientPeerConnectedSemaphore.release();});
        Object::connect(pClientPeer.get(), &TcpSocket::disconnected, [&](){clientPeerDisconnectedSemaphore.release();});
        Object::connect(pClientPeer.get(), &TcpSocket::error, [&](){clientPeerFailedSemaphore.release();});
        Object::connect(pClientPeer.get(), &TcpSocket::receivedData, [&]()
            {
                receivedClientPeerData.append(pClientPeer->readAll());
                clientPeerReceivedDataSemaphore.release();
            });
        pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());
        REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
        REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));

        WHEN("TcpSocket sends data to connected peer")
        {
            const auto dataToSend = GENERATE(AS(QByteArray),
                                                QByteArray("a"),
                                                QByteArray("abcdefgh"),
                                                largeData);
            const bool isClientTheSendingPeer = GENERATE(AS(bool), true, false);
            auto &pSendingPeer = isClientTheSendingPeer ? pClientPeer : pServerPeer;
            auto &pReceivingPeer = isClientTheSendingPeer ? pServerPeer : pClientPeer;
            pSendingPeer->write(dataToSend);

            THEN("connected peer receives sent data")
            {
                auto &receivedData = isClientTheSendingPeer ? receivedServerPeerData : receivedClientPeerData;
                auto &connectedPeerReceivedDataSemaphore = isClientTheSendingPeer ? serverPeerReceivedDataSemaphore : clientPeerReceivedDataSemaphore;
                while(dataToSend != receivedData)
                {
                    REQUIRE(TRY_ACQUIRE(connectedPeerReceivedDataSemaphore, 1));
                }

                AND_WHEN("TcpSocket sends more data to connected peer")
                {
                    receivedData.clear();
                    const QByteArray someMoreData("0123456789");
                    pSendingPeer->write(someMoreData);

                    THEN("connected peer receives sent data")
                    {
                        while(someMoreData != receivedData)
                        {
                            REQUIRE(TRY_ACQUIRE(connectedPeerReceivedDataSemaphore, 1));
                        }

                        AND_WHEN("TcpSocket disconnects from connected peer")
                        {
                            pSendingPeer->disconnectFromPeer();

                            THEN("both peers disconnect")
                            {
                                REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                            }
                        }
                    }
                }
            }
        }

        WHEN("TcpSocket closes connection after sending data to connected peer")
        {
            const auto dataToSend = GENERATE(AS(QByteArray),
                                                QByteArray("a"),
                                                QByteArray("abcdefgh"),
                                                largeData);
            const bool isClientTheSendingPeer = GENERATE(AS(bool), true, false);
            auto &pSendingPeer = isClientTheSendingPeer ? pClientPeer : pServerPeer;
            auto &pReceivingPeer = isClientTheSendingPeer ? pServerPeer : pClientPeer;
            pSendingPeer->write(dataToSend);
            pSendingPeer->disconnectFromPeer();

            THEN("connected peer receives sent data")
            {
                auto &receivedData = isClientTheSendingPeer ? receivedServerPeerData : receivedClientPeerData;
                auto &connectedPeerReceivedDataSemaphore = isClientTheSendingPeer ? serverPeerReceivedDataSemaphore : clientPeerReceivedDataSemaphore;
                while(dataToSend != receivedData)
                {
                    REQUIRE(TRY_ACQUIRE(connectedPeerReceivedDataSemaphore, 1));
                }

                AND_THEN("both peers disconnect without emitting any errors")
                {
                    REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                    REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                    REQUIRE(pSendingPeer->errorMessage().empty());
                    REQUIRE(pReceivingPeer->errorMessage().empty());

                    AND_WHEN("sending peer is deleted")
                    {
                        pSendingPeer.reset(nullptr);

                        THEN("neither peer emit any error")
                        {
                            REQUIRE(!TRY_ACQUIRE(clientPeerFailedSemaphore, QDeadlineTimer(1)));
                            REQUIRE(!serverPeerFailedSemaphore.tryAcquire());
                        }
                    }

                    AND_WHEN("receiving peer is deleted")
                    {
                        pReceivingPeer.reset(nullptr);

                        THEN("neither peer emit any error")
                        {
                            REQUIRE(!TRY_ACQUIRE(clientPeerFailedSemaphore, QDeadlineTimer(1)));
                            REQUIRE(!serverPeerFailedSemaphore.tryAcquire());
                        }
                    }
                }
            }
        }

        WHEN("TcpSocket aborts after sending data to connected peer")
        {
            const auto dataToSend = GENERATE(AS(QByteArray),
                                                QByteArray("a"),
                                                QByteArray("abcdefgh"),
                                                largeData);
            const bool isClientTheSendingPeer = GENERATE(AS(bool), true, false);
            auto &pSendingPeer = isClientTheSendingPeer ? pClientPeer : pServerPeer;
            auto &pReceivingPeer = isClientTheSendingPeer ? pServerPeer : pClientPeer;
            pSendingPeer->write(dataToSend);
            pSendingPeer->abort();

            THEN("connected peer disconnects without peers emitting any errors")
            {
                auto &connectedPeerDisconnectedSemaphore = isClientTheSendingPeer ? serverPeerDisconnectedSemaphore : clientPeerDisconnectedSemaphore;
                REQUIRE(TRY_ACQUIRE(connectedPeerDisconnectedSemaphore, 10));
                REQUIRE(pSendingPeer->errorMessage().empty());
                REQUIRE(pReceivingPeer->errorMessage().empty());
            }
        }

        WHEN("TcpSocket is deleted after sending data to connected peer")
        {
            const auto dataToSend = GENERATE(AS(QByteArray),
                                                QByteArray("a"),
                                                QByteArray("abcdefgh"),
                                                largeData);
            const bool isClientTheSendingPeer = GENERATE(AS(bool), true, false);
            auto &pSendingPeer = isClientTheSendingPeer ? pClientPeer : pServerPeer;
            auto &pReceivingPeer = isClientTheSendingPeer ? pServerPeer : pClientPeer;
            pSendingPeer->write(dataToSend);
            pSendingPeer.reset(nullptr);

            THEN("connected peer disconnects without emitting any errors")
            {
                auto &connectedPeerDisconnectedSemaphore = isClientTheSendingPeer ? serverPeerDisconnectedSemaphore : clientPeerDisconnectedSemaphore;
                REQUIRE(TRY_ACQUIRE(connectedPeerDisconnectedSemaphore, 10));
                REQUIRE(pReceivingPeer->errorMessage().empty());
            }
        }

        WHEN("TcpSocket disconnects from connected peer")
        {
            const bool isTcpSocketTheClient = GENERATE(AS(bool), true, false);
            auto &pTcpSocket = isTcpSocketTheClient ? pClientPeer : pServerPeer;
            auto &pConnectedPeer = isTcpSocketTheClient ? pServerPeer : pClientPeer;
            pTcpSocket->disconnectFromPeer();

            THEN("both peers disconnect")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                REQUIRE(pTcpSocket->errorMessage().empty());
                REQUIRE(pConnectedPeer->errorMessage().empty());

                AND_WHEN("TcpSocket is deleted")
                {
                    auto &tcpSocketFailedSemaphore = isTcpSocketTheClient ? clientPeerFailedSemaphore : serverPeerFailedSemaphore;
                    auto &connectedPeerFailedSemaphore = isTcpSocketTheClient ? serverPeerFailedSemaphore : clientPeerFailedSemaphore;
                    pTcpSocket.reset(nullptr);

                    THEN("neither peer emit any error")
                    {
                        REQUIRE(!TRY_ACQUIRE(connectedPeerFailedSemaphore, QDeadlineTimer(1)));
                        REQUIRE(!tcpSocketFailedSemaphore.tryAcquire());
                    }
                }
            }
        }

        WHEN("TcpSocket aborts connection")
        {
            const bool isTcpSocketTheClient = GENERATE(AS(bool), true, false);
            auto &pTcpSocket = isTcpSocketTheClient ? pClientPeer : pServerPeer;
            auto &pConnectedPeer = isTcpSocketTheClient ? pServerPeer : pClientPeer;
            pTcpSocket->abort();

            THEN("connected peer disconnect")
            {
                auto &connectedPeerDisconnectedSemaphore = isTcpSocketTheClient ? serverPeerDisconnectedSemaphore : clientPeerDisconnectedSemaphore;
                REQUIRE(TRY_ACQUIRE(connectedPeerDisconnectedSemaphore, 10));
                REQUIRE(pTcpSocket->errorMessage().empty());
                REQUIRE(pConnectedPeer->errorMessage().empty());

                AND_WHEN("TcpSocket is deleted")
                {
                    auto &tcpSocketFailedSemaphore = isTcpSocketTheClient ? clientPeerFailedSemaphore : serverPeerFailedSemaphore;
                    auto &connectedPeerFailedSemaphore = isTcpSocketTheClient ? serverPeerFailedSemaphore : clientPeerFailedSemaphore;
                    pTcpSocket.reset(nullptr);

                    THEN("neither peer emit any error")
                    {
                        REQUIRE(!TRY_ACQUIRE(connectedPeerFailedSemaphore, QDeadlineTimer(1)));
                        REQUIRE(!tcpSocketFailedSemaphore.tryAcquire());
                    }
                }
            }
        }

        WHEN("both peers disconnect")
        {
            const bool isTcpSocketTheClient = GENERATE(AS(bool), true, false);
            auto &pTcpSocket = isTcpSocketTheClient ? pClientPeer : pServerPeer;
            auto &pConnectedPeer = isTcpSocketTheClient ? pServerPeer : pClientPeer;
            pTcpSocket->disconnectFromPeer();
            pConnectedPeer->disconnectFromPeer();

            THEN("both peers disconnect")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                REQUIRE(pTcpSocket->errorMessage().empty());
                REQUIRE(pConnectedPeer->errorMessage().empty());

                AND_WHEN("TcpSocket is deleted")
                {
                    auto &tcpSocketFailedSemaphore = isTcpSocketTheClient ? clientPeerFailedSemaphore : serverPeerFailedSemaphore;
                    auto &connectedPeerFailedSemaphore = isTcpSocketTheClient ? serverPeerFailedSemaphore : clientPeerFailedSemaphore;
                    pTcpSocket.reset(nullptr);

                    THEN("neither peer emit any error")
                    {
                        REQUIRE(!TRY_ACQUIRE(connectedPeerFailedSemaphore, QDeadlineTimer(1)));
                        REQUIRE(!tcpSocketFailedSemaphore.tryAcquire());
                    }
                }
            }
        }

        WHEN("both peers abort connection")
        {
            const bool isTcpSocketTheClient = GENERATE(AS(bool), true, false);
            auto &pTcpSocket = isTcpSocketTheClient ? pClientPeer : pServerPeer;
            auto &pConnectedPeer = isTcpSocketTheClient ? pServerPeer : pClientPeer;
            pTcpSocket->abort();
            pConnectedPeer->abort();

            THEN("both peers abort connection without errors")
            {
                REQUIRE(pTcpSocket->errorMessage().empty());
                REQUIRE(pConnectedPeer->errorMessage().empty());

                AND_WHEN("TcpSocket is deleted")
                {
                    auto &tcpSocketFailedSemaphore = isTcpSocketTheClient ? clientPeerFailedSemaphore : serverPeerFailedSemaphore;
                    auto &connectedPeerFailedSemaphore = isTcpSocketTheClient ? serverPeerFailedSemaphore : clientPeerFailedSemaphore;
                    pTcpSocket.reset(nullptr);

                    THEN("neither peer emit any error")
                    {
                        REQUIRE(!TRY_ACQUIRE(connectedPeerFailedSemaphore, QDeadlineTimer(1)));
                        REQUIRE(!tcpSocketFailedSemaphore.tryAcquire());
                    }
                }
            }
        }

        WHEN("TcpSocket is deleted")
        {
            const bool isTcpSocketTheClient = GENERATE(AS(bool), true, false);
            auto &pTcpSocket = isTcpSocketTheClient ? pClientPeer : pServerPeer;
            auto &pConnectedPeer = isTcpSocketTheClient ? pServerPeer : pClientPeer;
            pTcpSocket.reset(nullptr);

            THEN("connected peer disconnects without errors")
            {
                auto &connectedPeerDisconnectedSemaphore = isTcpSocketTheClient ? serverPeerDisconnectedSemaphore : clientPeerDisconnectedSemaphore;
                REQUIRE(TRY_ACQUIRE(connectedPeerDisconnectedSemaphore, 10));
                auto &tcpSocketFailedSemaphore = isTcpSocketTheClient ? clientPeerFailedSemaphore : serverPeerFailedSemaphore;
                auto &connectedPeerFailedSemaphore = isTcpSocketTheClient ? serverPeerFailedSemaphore : clientPeerFailedSemaphore;
                REQUIRE(!TRY_ACQUIRE(connectedPeerFailedSemaphore, QDeadlineTimer(1)));
                REQUIRE(!tcpSocketFailedSemaphore.tryAcquire());
                REQUIRE(pConnectedPeer->errorMessage().empty());
            }
        }
    }
}


SCENARIO("TcpSocket fails as expected")
{
    GIVEN("no server running on any IP related to host name with IPV4/IPV6 addresses")
    {
        WHEN("TcpSocket is connected to host name with IPV4/IPV6 addresses")
        {
            TcpSocket socket;
            QSemaphore socketFailedSemaphore;
            Object::connect(&socket, &TcpSocket::error, [&](){socketFailedSemaphore.release();});
            auto hostName = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses().first;
            socket.connect(hostName, 5000);

            THEN("connection fails")
            {
                REQUIRE(TRY_ACQUIRE(socketFailedSemaphore, 10));
                REQUIRE(socket.state() == TcpSocket::State::Unconnected);
                REQUIRE(socket.errorMessage().starts_with(std::string("Failed to connect to ").append(hostName).append(" at")));
            }
        }
    }

    GIVEN("a server running on IPV6 localhost")
    {
        TcpServer server;
        auto [hostName, hostAddresses] = TestHostNamesFetcher::hostNameWithIpv6Address();
        REQUIRE(hostAddresses.size() == 1 && hostAddresses[0] == QHostAddress("::1"));
        REQUIRE(server.listen(QHostAddress("::1")));
        size_t connectionCount = 0;
        Object::connect(&server, &TcpServer::newConnection, [&](){++connectionCount;});

        WHEN("a TcpSocket bounded to a IPV4 address is connected to the IPV6 server")
        {
            TcpSocket socket;
            QSemaphore socketFailedSemaphore;
            Object::connect(&socket, &TcpSocket::error, [&](){socketFailedSemaphore.release();});
            socket.setBindAddressAndPort("127.2.2.5");
            const auto connectByName = GENERATE(AS(bool), true, false);
            if (connectByName)
                socket.connect(hostName, server.serverPort());
            else
                socket.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(TRY_ACQUIRE(socketFailedSemaphore, 10));
                REQUIRE(socket.state() == TcpSocket::State::Unconnected);
                const auto expectedErrorMessage = connectByName ? std::string("Failed to connect to ").append(hostName).append(" at [::1]:") : std::string("Failed to connect to [::1]:");
                REQUIRE(socket.errorMessage().starts_with(expectedErrorMessage));
                REQUIRE(connectionCount == 0);
            }
        }

        WHEN("TcpSocket bounded to a privileged port on IPV6 is connected to the server")
        {
            TcpSocket socket;
            QSemaphore socketFailedSemaphore;
            Object::connect(&socket, &TcpSocket::error, [&](){socketFailedSemaphore.release();});
            socket.setBindAddressAndPort("::1", 443);
            const auto connectByName = GENERATE(AS(bool), true, false);
            if (connectByName)
                socket.connect(hostName, server.serverPort());
            else
                socket.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(TRY_ACQUIRE(socketFailedSemaphore, 10));
                REQUIRE(socket.state() == TcpSocket::State::Unconnected);
                REQUIRE(socket.errorMessage() == "Failed to bind socket to [::1]:443. POSIX error EACCES(13): Permission denied.");
                REQUIRE(connectionCount == 0);
            }
        }

        WHEN("TcpSocket bound to an already used address/port pair is connected to server")
        {
            TcpSocket previouslyConnectedSocket;
            QSemaphore previouslyConnectedSocketSemaphore;
            Object::connect(&previouslyConnectedSocket, &TcpSocket::connected, [&](){previouslyConnectedSocketSemaphore.release();});
            previouslyConnectedSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());
            REQUIRE(TRY_ACQUIRE(previouslyConnectedSocketSemaphore, 10));
            TcpSocket socket;
            QSemaphore socketFailedSemaphore;
            Object::connect(&socket, &TcpSocket::error, [&](){socketFailedSemaphore.release();});
            socket.setBindAddressAndPort(previouslyConnectedSocket.localAddress(), previouslyConnectedSocket.localPort());
            const auto connectByName = GENERATE(AS(bool), true, false);
            if (connectByName)
                socket.connect(hostName, server.serverPort());
            else
                socket.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(TRY_ACQUIRE(socketFailedSemaphore, 10));
                REQUIRE(socket.state() == TcpSocket::State::Unconnected);
                REQUIRE(socket.errorMessage() == std::string("Failed to bind socket to [::1]:").append(std::to_string(previouslyConnectedSocket.localPort())).append(". POSIX error EADDRINUSE(98): Address already in use."));
                REQUIRE(connectionCount == 1);
            }
        }
    }

    GIVEN("a server running on IPV4 localhost")
    {
        auto [hostName, hostAddresses] = TestHostNamesFetcher::hostNameWithIpv4Address();
        REQUIRE(hostAddresses.size() == 1 && hostAddresses[0].protocol() == QHostAddress::IPv4Protocol);
        const auto hostAddress = hostAddresses[0];
        TcpServer server;
        REQUIRE(server.listen(hostAddress));
        size_t connectionCount = 0;
        Object::connect(&server, &TcpServer::newConnection, [&](){++connectionCount;});

        WHEN("a TcpSocket bounded to a IPV6 address is connected to the IPV4 server")
        {
            TcpSocket socket;
            QSemaphore socketFailedSemaphore;
            Object::connect(&socket, &TcpSocket::error, [&](){socketFailedSemaphore.release();});
            socket.setBindAddressAndPort("::1");
            const auto connectByName = GENERATE(AS(bool), true, false);
            if (connectByName)
                socket.connect(hostName, server.serverPort());
            else
                socket.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(TRY_ACQUIRE(socketFailedSemaphore, 10));
                REQUIRE(socket.state() == TcpSocket::State::Unconnected);
                const auto expectedErrorMessage = connectByName ? std::string("Failed to connect to ").append(hostName).append(" at ").append(hostAddress.toString().toStdString()).append(":") : std::string("Failed to connect to ").append(hostAddress.toString().toStdString()).append(":");
                REQUIRE(socket.errorMessage().starts_with(expectedErrorMessage));
                REQUIRE(connectionCount == 0);
            }
        }

        WHEN("TcpSocket bound to a privileged port on IPV4 is connected to server")
        {
            TcpSocket socket;
            QSemaphore socketFailedSemaphore;
            Object::connect(&socket, &TcpSocket::error, [&](){socketFailedSemaphore.release();});
            socket.setBindAddressAndPort("127.0.0.1", 443);
            const auto connectByName = GENERATE(AS(bool), true, false);
            if (connectByName)
                socket.connect(hostName, server.serverPort());
            else
                socket.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(TRY_ACQUIRE(socketFailedSemaphore, 10));
                REQUIRE(socket.state() == TcpSocket::State::Unconnected);
                REQUIRE(socket.errorMessage() == "Failed to bind socket to 127.0.0.1:443. POSIX error EACCES(13): Permission denied.");
                REQUIRE(connectionCount == 0);
            }
        }

        WHEN("TcpSocket bound to an already used address/port pair is connected to server")
        {
            TcpSocket previouslyConnectedSocket;
            QSemaphore previouslyConnectedSocketSemaphore;
            Object::connect(&previouslyConnectedSocket, &TcpSocket::connected, [&](){previouslyConnectedSocketSemaphore.release();});
            previouslyConnectedSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());
            REQUIRE(TRY_ACQUIRE(previouslyConnectedSocketSemaphore, 10));
            TcpSocket socket;
            QSemaphore socketFailedSemaphore;
            Object::connect(&socket, &TcpSocket::error, [&](){socketFailedSemaphore.release();});
            socket.setBindAddressAndPort(previouslyConnectedSocket.localAddress(), previouslyConnectedSocket.localPort());
            const auto connectByName = GENERATE(AS(bool), true, false);
            if (connectByName)
                socket.connect(hostName, server.serverPort());
            else
                socket.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(TRY_ACQUIRE(socketFailedSemaphore, 10));
                REQUIRE(socket.state() == TcpSocket::State::Unconnected);
                REQUIRE(socket.errorMessage() == std::string("Failed to bind socket to 127.0.0.1:").append(std::to_string(previouslyConnectedSocket.localPort())).append(". POSIX error EADDRINUSE(98): Address already in use."));
                REQUIRE(connectionCount == 1);
            }
        }
    }

    GIVEN("a descriptor that does not represent a socket")
    {
        const auto fileDescriptor = ::memfd_create("Kourier_tcp_socket_spec_a_descriptor_that_does_not_represent_a_socket", 0);
        REQUIRE(fileDescriptor >= 0);

        WHEN("a TcpSocket is created with the given descritor")
        {
            TcpSocket socket(fileDescriptor);

            THEN("socket is created in the unconnected state")
            {
                REQUIRE(socket.state() == TcpSocket::State::Unconnected);
            }
        }
    }

    GIVEN("an invalid descriptor")
    {
        const int invalidDescriptor = std::numeric_limits<int>::max();

        WHEN("a TcpSocket is created with the given descritor")
        {
            TcpSocket socket(invalidDescriptor);

            THEN("socket is created in the unconnected state")
            {
                REQUIRE(socket.state() == TcpSocket::State::Unconnected);
            }
        }
    }

    GIVEN("an unconnected socket descriptor")
    {
        const auto socketDescriptor = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        REQUIRE(socketDescriptor >= 0);

        WHEN("a TcpSocket is created with the given descritor")
        {
            TcpSocket socket(socketDescriptor);

            THEN("socket is created in the unconnected state")
            {
                REQUIRE(socket.state() == TcpSocket::State::Unconnected);
            }
        }
    }

    GIVEN("a server with a full listen backlog size that does not accept new connections")
    {
        QTcpServer server;
        static constexpr int backlogSize = 128;
        server.setListenBacklogSize(backlogSize);
        QObject::connect(&server, &QTcpServer::newConnection, [](){Spectator::FAIL("This code is supposed to be unreachable.");});
        auto hostAddress("127.10.20.82");
        REQUIRE(server.listen(QHostAddress(hostAddress)));
        REQUIRE(server.listenBacklogSize() == backlogSize);
        server.pauseAccepting();
        QSemaphore connectedSemaphore;
        QSemaphore errorSemaphore;
        std::list<std::shared_ptr<TcpSocket>> sockets;
        bool isServerAcceptingConnections = true;
        do
        {
            std::shared_ptr<TcpSocket> pSocket(new TcpSocket);
            sockets.push_front(pSocket);
            Object::connect(pSocket.get(), &TcpSocket::connected, [&connectedSemaphore](){connectedSemaphore.release();});
            Object::connect(pSocket.get(), &TcpSocket::error, [&errorSemaphore](){errorSemaphore.release();});
            pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());
            isServerAcceptingConnections = TRY_ACQUIRE(connectedSemaphore, QDeadlineTimer(10));
        }
        while (isServerAcceptingConnections);
        sockets.front()->abort();

        WHEN("a TcpSocket tries to connect to server")
        {
            TcpSocket socket;
            socket.setConnectTimeout(std::chrono::milliseconds(5));
            QSemaphore socketFailedSemaphore;
            Object::connect(&socket, &TcpSocket::error, [&]() {socketFailedSemaphore.release();});
            socket.connect(hostAddress, server.serverPort());

            THEN("TcpSocket times out while trying to connect to server")
            {
                REQUIRE(TRY_ACQUIRE(socketFailedSemaphore, 10));
                REQUIRE(socket.state() == TcpSocket::State::Unconnected);
                std::string expectedErrorMessage("Failed to connect to ");
                expectedErrorMessage.append(hostAddress);
                expectedErrorMessage.append(":");
                expectedErrorMessage.append(std::to_string(server.serverPort()));
                expectedErrorMessage.append(".");
                REQUIRE(socket.errorMessage() == expectedErrorMessage);
            }
        }
    }
}


SCENARIO("TcpSocket allows connected slots to take any action")
{
    GIVEN("a TcpSocket and a running server")
    {
        QSemaphore serverPeerConnectedSemaphore;
        QSemaphore serverPeerDisconnectedSemaphore;
        QSemaphore serverPeerFailedSemaphore;
        TcpServer server;
        std::unique_ptr<TcpSocket> pServerPeer;
        Object::connect(&server, &TcpServer::newConnection, [&](TcpSocket *pSocket)
        {
            REQUIRE(!pServerPeer);
            pServerPeer.reset(pSocket);
            REQUIRE(pServerPeer.get() != nullptr);
            Object::connect(pServerPeer.get(), &TcpSocket::error, [&]() {serverPeerFailedSemaphore.release();});
            Object::connect(pServerPeer.get(), &TcpSocket::disconnected, [&](){serverPeerDisconnectedSemaphore.release();});
            serverPeerConnectedSemaphore.release(2);
        });
        // This test creates a lot of sockets in TIME_WAIT state if run
        // repeatedly (by using the -r command line option), as this test 
        // intentionally interrupts the connection abruptly.
        // To minimize the problem, we always change the localhost ipv4
        // address used by the server.
        static uint16_t secondIPv4 = 1;
        static uint16_t thirdIPv4 = 1;
        static uint16_t forthIPv4 = 1;
        const QHostAddress hostAddress(QString::fromLatin1("127.%1.%2.%3").arg(secondIPv4).arg(thirdIPv4).arg(forthIPv4));
        if (++forthIPv4 == 250)
        {
            forthIPv4 = 1;
            if (++thirdIPv4 == 250)
            {
                thirdIPv4 = 1;
                if (++secondIPv4 == 250)
                    secondIPv4 = 1;
            }
        }
        REQUIRE(server.listen(hostAddress));
        QSemaphore clientPeerConnectedSemaphore;
        QSemaphore clientPeerDisconnectedSemaphore;
        QSemaphore clientPeerFailedSemaphore;
        std::unique_ptr<TcpSocket> pClientPeer(new TcpSocket);
        Object::connect(pClientPeer.get(), &TcpSocket::connected, [&]()
            {
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                clientPeerConnectedSemaphore.release();
            });
        Object::connect(pClientPeer.get(), &TcpSocket::disconnected, [&](){clientPeerDisconnectedSemaphore.release();});
        Object::connect(pClientPeer.get(), &TcpSocket::error, [&](){clientPeerFailedSemaphore.release();});

        WHEN("client peer connects to server and disconnects while handling the connected signal")
        {
            Object::connect(pClientPeer.get(), &TcpSocket::connected, [&]() {pClientPeer->disconnectFromPeer();});
            pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("peers connect and then disconnect")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                REQUIRE(pClientPeer->state() == TcpSocket::State::Unconnected);
            }
        }

        WHEN("client peer connects to server and aborts connection while handling the connected signal")
        {
            Object::connect(pClientPeer.get(), &TcpSocket::connected, [&]() {pClientPeer->abort();});
            pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("peers connect and then server peer disconnects")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                REQUIRE(pClientPeer->state() == TcpSocket::State::Unconnected);
            }
        }

        WHEN("client peer connects to server and is destroyed while handling the connected signal")
        {
            Object::connect(pClientPeer.get(), &TcpSocket::connected, [&]() {pClientPeer.release()->scheduleForDeletion();});
            pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("peers connect, client peer aborts connection and server peer disconnects")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
            }
        }
        
        WHEN("client peer connects to server and connects again while handling the connected signal")
        {
            Object *pCtxObject = new Object;
            Object::connect(pClientPeer.get(), &TcpSocket::connected, pCtxObject, [&, ptrCtxObject = pCtxObject]()
            {
                while (!pServerPeer) {QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents, 1);}
                Object::connect(pServerPeer.get(), &TcpSocket::disconnected, [pPeer = pServerPeer.get()](){pPeer->scheduleForDeletion();});
                pServerPeer.release();
                pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());
                delete ptrCtxObject;
            });
            pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("peers connect, client peer aborts and peer disconnects and then both client peer connects to another server peer")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                pClientPeer->disconnectFromPeer();
                REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
            }
        }

        
        WHEN("client peer connects to server and connects to a non-existent server address while handling the connected signal")
        {
            Object::connect(pClientPeer.get(), &TcpSocket::connected, [&]()
            {
                while (!pServerPeer) {QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents, 1);}
                Object::connect(pServerPeer.get(), &TcpSocket::disconnected, [pPeer = pServerPeer.get()](){pPeer->scheduleForDeletion();});
                pServerPeer.release();
                auto const serverAddress = QHostAddress("127.1.2.3");
                QTcpSocket socket;
                REQUIRE(socket.bind(serverAddress));
                const auto unusedPortForNow = socket.localPort();
                socket.abort();
                pClientPeer->connect(serverAddress.toString().toStdString(), unusedPortForNow);
            });
            pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("peers connect, client peer aborts and server peer disconnects and then client peer fails to connect to the non-existent server")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(clientPeerFailedSemaphore, 10));
                REQUIRE(pClientPeer->errorMessage().starts_with("Failed to connect to 127.1.2.3:"));
            }
        }

        WHEN("client peer connects to server")
        {
            pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());
            REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
            REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));

            THEN("connected peers can start exchanging data")
            {
                AND_WHEN("server peer sends some data to client peer which disconnects while handling the receivedData signal")
                {
                    Object::connect(pClientPeer.get(), &TcpSocket::receivedData, [&](){pClientPeer->disconnectFromPeer();});
                    pServerPeer->write("abcdefgh");

                    THEN("peers disconnect")
                    {
                        REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                    }
                }

                AND_WHEN("server peer sends some data to client peer which aborts the connection while handling the receivedData signal")
                {
                    Object::connect(pClientPeer.get(), &TcpSocket::receivedData, [&](){pClientPeer->abort();});
                    pServerPeer->write("abcdefgh");

                    THEN("client peer aborts and server peer disconnects")
                    {
                        REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                        REQUIRE(pClientPeer->state() == TcpSocket::State::Unconnected);
                    }
                }

                AND_WHEN("server peer sends some data to client peer which is destroyed while handling the receivedData signal")
                {
                    auto *pReleasedSocket = pClientPeer.release();
                    Object::connect(pReleasedSocket, &TcpSocket::receivedData, [=](){pReleasedSocket->scheduleForDeletion();});
                    pServerPeer->write("abcdefgh");

                    THEN("client peer is destroyed and server peer disconnects")
                    {
                        REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                    }
                }

                AND_WHEN("server peer sends some data to client peer which reconnects while handling the receivedData signal")
                {
                    Object::connect(pServerPeer.get(), &TcpSocket::disconnected, [pPeer = pServerPeer.get()](){pPeer->scheduleForDeletion();});
                    Object::connect(pClientPeer.get(), &TcpSocket::receivedData, [&]()
                    {
                        pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());
                    });
                    pServerPeer->write("abcdefgh");
                    pServerPeer.release();

                    THEN("client peer aborts connection and disconnects server peer before reconnecting to another server peer")
                    {
                        REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                        pClientPeer->disconnectFromPeer();
                        REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                    }
                }

                AND_WHEN("client peer sends more data the underlying socket's send buffer can store and disconnects while handling the sentData signal with data still to be written")
                {
                    Object::connect(pClientPeer.get(), &TcpSocket::sentData, [&]()
                    {
                        if (pClientPeer->dataToWrite())
                            pClientPeer->disconnectFromPeer();
                    });
                    const auto socketSendBufferSize = pClientPeer->getSocketOption(TcpSocket::SocketOption::SendBufferSize);
                    REQUIRE(socketSendBufferSize > 1);
                    QByteArray dataToSend(3 * socketSendBufferSize, ' ');
                    pClientPeer->write(dataToSend.data(), dataToSend.size());

                    THEN("peers disconnect")
                    {
                        REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                    }
                }

                AND_WHEN("client peer sends more data the underlying socket's send buffer can store and disconnects while handling the sentData signal with  no more data still to be written")
                {
                    Object::connect(pClientPeer.get(), &TcpSocket::sentData, [&]()
                    {
                        if (!pClientPeer->dataToWrite())
                            pClientPeer->disconnectFromPeer();
                    });
                    const auto socketSendBufferSize = pClientPeer->getSocketOption(TcpSocket::SocketOption::SendBufferSize);
                    REQUIRE(socketSendBufferSize > 1);
                    QByteArray dataToSend(3 * socketSendBufferSize, ' ');
                    pClientPeer->write(dataToSend.data(), dataToSend.size());

                    THEN("peers disconnect")
                    {
                        REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                    }
                }

                AND_WHEN("client peer sends more data the underlying socket's send buffer can store and aborts the connection while handling the sentData signal with data still to be written")
                {
                    Object::connect(pClientPeer.get(), &TcpSocket::sentData, [&]()
                    {
                        if (pClientPeer->dataToWrite())
                            pClientPeer->abort();
                    });
                    const auto socketSendBufferSize = pClientPeer->getSocketOption(TcpSocket::SocketOption::SendBufferSize);
                    REQUIRE(socketSendBufferSize > 1);
                    QByteArray dataToSend(3 * socketSendBufferSize, ' ');
                    pClientPeer->write(dataToSend.data(), dataToSend.size());

                    THEN("client peer aborts the connection, disconnecting the server peer")
                    {
                        REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                        REQUIRE(pClientPeer->state() == TcpSocket::State::Unconnected);
                    }
                }

                AND_WHEN("client peer sends more data the underlying socket's send buffer can store and aborts the connection while handling the sentData signal with no more data data still to be written")
                {
                    Object::connect(pClientPeer.get(), &TcpSocket::sentData, [&]()
                    {
                        if (!pClientPeer->dataToWrite())
                            pClientPeer->abort();
                    });
                    const auto socketSendBufferSize = pClientPeer->getSocketOption(TcpSocket::SocketOption::SendBufferSize);
                    REQUIRE(socketSendBufferSize > 1);
                    QByteArray dataToSend(3 * socketSendBufferSize, ' ');
                    pClientPeer->write(dataToSend.data(), dataToSend.size());

                    THEN("client peer aborts the connection, disconnecting the server peer")
                    {
                        REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                        REQUIRE(pClientPeer->state() == TcpSocket::State::Unconnected);
                    }
                }

                AND_WHEN("client peer sends more data the underlying socket's send buffer can store and is destroyed while handling the sentData signal with data still to be written")
                {
                    Object::connect(pClientPeer.get(), &TcpSocket::sentData, [&, ptrSocket = pClientPeer.get()]()
                    {
                        if (ptrSocket->dataToWrite())
                        {
                            ptrSocket->scheduleForDeletion();
                            pClientPeer.release();
                        }
                    });
                    const auto socketSendBufferSize = pClientPeer->getSocketOption(TcpSocket::SocketOption::SendBufferSize);
                    REQUIRE(socketSendBufferSize > 1);
                    QByteArray dataToSend(3 * socketSendBufferSize, ' ');
                    pClientPeer->write(dataToSend.data(), dataToSend.size());

                    THEN("client peer aborts the connection during destruction, disconnecting the server peer")
                    {
                        REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                    }
                }

                AND_WHEN("client peer sends more data the underlying socket's send buffer can store and is destroyed while handling the sentData signal with no more data still to be written")
                {
                    Object::connect(pClientPeer.get(), &TcpSocket::sentData, [&]()
                    {
                        if (!pClientPeer->dataToWrite())
                            pClientPeer.release()->scheduleForDeletion();
                    });
                    const auto socketSendBufferSize = pClientPeer->getSocketOption(TcpSocket::SocketOption::SendBufferSize);
                    REQUIRE(socketSendBufferSize > 1);
                    QByteArray dataToSend(3 * socketSendBufferSize, ' ');
                    pClientPeer->write(dataToSend.data(), dataToSend.size());

                    THEN("client peer aborts the connection during destruction, disconnecting the server peer")
                    {
                        REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                    }
                }

                AND_WHEN("client peer sends more data the underlying socket's send buffer can store and is reconnected while handling the sentData signal with data still to be written")
                {
                    auto *pPeer = pServerPeer.release();
                    Object::connect(pPeer, &TcpSocket::disconnected, [=](){pPeer->scheduleForDeletion();});
                    Object::connect(pClientPeer.get(), &TcpSocket::sentData, [&]()
                    {
                        if (pClientPeer->dataToWrite())
                            pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());
                    });
                    const auto socketSendBufferSize = pClientPeer->getSocketOption(TcpSocket::SocketOption::SendBufferSize);
                    REQUIRE(socketSendBufferSize > 1);
                    QByteArray dataToSend(3 * socketSendBufferSize, ' ');
                    pClientPeer->write(dataToSend.data(), dataToSend.size());

                    THEN("client peer aborts the connection disconnecting the server peer and reconnects to another server peer")
                    {
                        REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                        pClientPeer->disconnectFromPeer();
                        REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                    }
                }

                AND_WHEN("client peer sends more data the underlying socket's send buffer can store and is reconnected while handling the sentData signal with no more data still to be written")
                {
                    auto *pPeer = pServerPeer.release();
                    Object::connect(pPeer, &TcpSocket::disconnected, [=](){pPeer->scheduleForDeletion();});
                    Object::connect(pClientPeer.get(), &TcpSocket::sentData, [&]()
                    {
                        if (!pClientPeer->dataToWrite())
                            pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());
                    });
                    const auto socketSendBufferSize = pClientPeer->getSocketOption(TcpSocket::SocketOption::SendBufferSize);
                    REQUIRE(socketSendBufferSize > 1);
                    QByteArray dataToSend(3 * socketSendBufferSize, ' ');
                    pClientPeer->write(dataToSend.data(), dataToSend.size());

                    THEN("client peer aborts the connection disconnecting the server peer and reconnects to another server peer")
                    {
                        REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                        pClientPeer->disconnectFromPeer();
                        REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                    }
                }
            }

            AND_WHEN("server peer disconnects and client peer is disconnected while handling the disconnected signal")
            {
                QSemaphore socketDisconnectedFromPeerSemaphore;
                Object::connect(pClientPeer.get(), &TcpSocket::disconnected, [&]()
                {
                    pClientPeer->disconnectFromPeer();
                    socketDisconnectedFromPeerSemaphore.release();
                });
                pServerPeer->disconnectFromPeer();

                THEN("peers disconnect")
                {
                    REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                    REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                    REQUIRE(TRY_ACQUIRE(socketDisconnectedFromPeerSemaphore, 10));
                }
            }

            AND_WHEN("server peer disconnects and client peer aborts connection while handling the disconnected signal")
            {
                QSemaphore socketDisconnectedFromPeerSemaphore;
                Object::connect(pClientPeer.get(), &TcpSocket::disconnected, [&]()
                {
                    pClientPeer->abort();
                    socketDisconnectedFromPeerSemaphore.release();
                });
                pServerPeer->disconnectFromPeer();

                THEN("peers disconnect")
                {
                    REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                    REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                    REQUIRE(TRY_ACQUIRE(socketDisconnectedFromPeerSemaphore, 10));
                }
            }

            AND_WHEN("server peer disconnects and client peer is destroyed while handling the disconnected signal")
            {
                QSemaphore socketDisconnectedFromPeerSemaphore;
                Object::connect(pClientPeer.get(), &TcpSocket::disconnected, [&]()
                {
                    pClientPeer.release()->scheduleForDeletion();
                    socketDisconnectedFromPeerSemaphore.release();
                });
                pServerPeer->disconnectFromPeer();

                THEN("peers disconnect")
                {
                    REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                    REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                    REQUIRE(TRY_ACQUIRE(socketDisconnectedFromPeerSemaphore, 10));
                }
            }

            AND_WHEN("server peer disconnects and client peer is reconnected while handling the disconnected signal")
            {
                QSemaphore socketDisconnectedFromPeerSemaphore;
                auto *pCtxObject = new Object;
                Object::connect(pClientPeer.get(), &TcpSocket::disconnected, pCtxObject, [&, ptrCtxObject = pCtxObject]()
                {
                    socketDisconnectedFromPeerSemaphore.release();
                    pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());
                    delete ptrCtxObject;
                });
                auto *ptrServerPeer = pServerPeer.release();
                ptrServerPeer->disconnectFromPeer();
                ptrServerPeer->scheduleForDeletion();

                THEN("peers disconnect and client peer reconnects to another server peer")
                {
                    REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                    REQUIRE(TRY_ACQUIRE(socketDisconnectedFromPeerSemaphore, 10));
                    REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                    REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                    pClientPeer->disconnectFromPeer();
                    REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                    REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                }
            }
        }

        WHEN("client peer tries to connect to a non-existent server by address and is disconnected while handling the error signal")
        {
            QSemaphore socketHandledErrorSemaphore;
            Object::connect(pClientPeer.get(), &TcpSocket::error, [&]()
            {
                REQUIRE(!pClientPeer->errorMessage().empty());
                pClientPeer->disconnectFromPeer();
                socketHandledErrorSemaphore.release();
            });
            auto const serverAddress = QHostAddress("127.1.2.3");
            QTcpSocket socket;
            REQUIRE(socket.bind(serverAddress));
            const auto unusedPortForNow = socket.localPort();
            socket.abort();
            pClientPeer->connect(serverAddress.toString().toStdString(), unusedPortForNow);

            THEN("client peer handles error")
            {
                REQUIRE(TRY_ACQUIRE(socketHandledErrorSemaphore, 10));
            }
        }

        WHEN("client peer tries to connect to a non-existent server by address and aborts connection while handling the error signal")
        {
            QSemaphore socketHandledErrorSemaphore;
            Object::connect(pClientPeer.get(), &TcpSocket::error, [&]()
            {
                REQUIRE(!pClientPeer->errorMessage().empty());
                pClientPeer->abort();
                socketHandledErrorSemaphore.release();
            });
            auto const serverAddress = QHostAddress("127.1.2.3");
            QTcpSocket socket;
            REQUIRE(socket.bind(serverAddress));
            const auto unusedPortForNow = socket.localPort();
            socket.abort();
            pClientPeer->connect(serverAddress.toString().toStdString(), unusedPortForNow);

            THEN("client peer handles error")
            {
                REQUIRE(TRY_ACQUIRE(socketHandledErrorSemaphore, 10));
            }
        }

        WHEN("client peer tries to connect to a non-existent server by address and is destroyed while handling the error signal")
        {
            QSemaphore socketHandledErrorSemaphore;
            Object::connect(pClientPeer.get(), &TcpSocket::error, [&]()
            {
                REQUIRE(!pClientPeer->errorMessage().empty());
                pClientPeer.release()->scheduleForDeletion();
                socketHandledErrorSemaphore.release();
            });
            auto const serverAddress = QHostAddress("127.1.2.3");
            QTcpSocket socket;
            REQUIRE(socket.bind(serverAddress));
            const auto unusedPortForNow = socket.localPort();
            socket.abort();
            pClientPeer->connect(serverAddress.toString().toStdString(), unusedPortForNow);

            THEN("client peer handles error")
            {
                REQUIRE(TRY_ACQUIRE(socketHandledErrorSemaphore, 10));
            }
        }

        WHEN("client peer tries to connect to a non-existent server by address and connects to the running server while handling the error signal")
        {
            QSemaphore socketHandledErrorSemaphore;
            Object::connect(pClientPeer.get(), &TcpSocket::error, [&]()
                {
                    REQUIRE(!pClientPeer->errorMessage().empty());
                    REQUIRE(!clientPeerConnectedSemaphore.tryAcquire());
                    pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());
                    socketHandledErrorSemaphore.release();
                });
            auto const serverAddress = QHostAddress("127.1.2.3");
            QTcpSocket socket;
            REQUIRE(socket.bind(serverAddress));
            const auto unusedPortForNow = socket.localPort();
            socket.abort();
            pClientPeer->connect(serverAddress.toString().toStdString(), unusedPortForNow);

            THEN("client peer handles error, aborts and connects to server peer")
            {
                REQUIRE(TRY_ACQUIRE(socketHandledErrorSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                REQUIRE(pClientPeer->errorMessage().empty());
                pClientPeer->disconnectFromPeer();
                REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
            }
        }

        WHEN("client peer tries to connect to host name with IPV4/IPV6 addresses without any server running and disconnects while handling the error signal")
        {
            QSemaphore socketHandledErrorSemaphore;
            Object::connect(pClientPeer.get(), &TcpSocket::error, [&]()
                {
                    REQUIRE(!pClientPeer->errorMessage().empty());
                    pClientPeer->disconnectFromPeer();
                    socketHandledErrorSemaphore.release();
                });
            pClientPeer->setConnectTimeout(std::chrono::milliseconds(5));
            auto hostName = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses().first;
            pClientPeer->connect(hostName, 3008);

            THEN("client peer handles error")
            {
                REQUIRE(TRY_ACQUIRE(socketHandledErrorSemaphore, 10));
            }
        }

        WHEN("client peer tries to connect to host name with IPV4/IPV6 addresses without any server running and aborts connection while handling the error signal")
        {
            QSemaphore socketHandledErrorSemaphore;
            Object::connect(pClientPeer.get(), &TcpSocket::error, [&]()
            {
                REQUIRE(!pClientPeer->errorMessage().empty());
                pClientPeer->abort();
                socketHandledErrorSemaphore.release();
            });
            auto hostName = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses().first;
            pClientPeer->connect(hostName, 3008);

            THEN("client peer handles error")
            {
                REQUIRE(TRY_ACQUIRE(socketHandledErrorSemaphore, 10));
            }
        }

        WHEN("client peer tries to connect to host name with IPV4/IPV6 addresses without any server running and is destroyed while handling the error signal")
        {
            QSemaphore socketHandledErrorSemaphore;
            Object::connect(pClientPeer.get(), &TcpSocket::error, [&]()
            {
                REQUIRE(!pClientPeer->errorMessage().empty());
                pClientPeer.release()->scheduleForDeletion();
                socketHandledErrorSemaphore.release();
            });
            auto hostName = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses().first;
            pClientPeer->connect(hostName, 3008);

            THEN("client peer handles error")
            {
                REQUIRE(TRY_ACQUIRE(socketHandledErrorSemaphore, 10));
            }
        }

        WHEN("client peer tries to connect to host name with IPV4/IPV6 addresses without any server running and is reconnected to the running server while handling the error signal")
        {
            QSemaphore socketHandledErrorSemaphore;
            Object::connect(pClientPeer.get(), &TcpSocket::error, [&]()
            {
                REQUIRE(!pClientPeer->errorMessage().empty());
                REQUIRE(!clientPeerConnectedSemaphore.tryAcquire());
                pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());
                socketHandledErrorSemaphore.release();
            });
            auto hostName = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses().first;
            pClientPeer->connect(hostName, 3008);

            THEN("client peer handles error and aborts before connecting to server peer")
            {
                REQUIRE(TRY_ACQUIRE(socketHandledErrorSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                pClientPeer->disconnectFromPeer();
                REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
            }
        }
    }
}


SCENARIO("TcpSockets can be reused")
{
    GIVEN("A running server whose sockets close the connections just after they get established")
    {
        TcpServer server;
        QSemaphore serverPeerConnectedSemaphore;
        QSemaphore serverPeerDisconnectedSemaphore;
        std::unique_ptr<TcpSocket> pServerPeer;
        Object::connect(&server, &TcpServer::newConnection, [&](TcpSocket *pSocket)
            {
                REQUIRE(!pServerPeer);
                pServerPeer.reset(pSocket);
                Object::connect(pServerPeer.get(), &TcpSocket::disconnected, [&](){pServerPeer.release()->scheduleForDeletion(); serverPeerDisconnectedSemaphore.release();});
                pServerPeer->disconnectFromPeer();
                serverPeerConnectedSemaphore.release();
            });
        REQUIRE(server.listen(QHostAddress::LocalHost));

        WHEN("client peer connects with server three times")
        {
            TcpSocket clientPeer;
            QSemaphore clientPeerConnectedSemaphore;
            QSemaphore clientPeerDisconnectedSemaphore;
            Object::connect(&clientPeer, &TcpSocket::connected, [&](){clientPeerConnectedSemaphore.release();});
            Object::connect(&clientPeer, &TcpSocket::disconnected, [&](){clientPeerDisconnectedSemaphore.release();});

            THEN("peers connect and disconnect as many times as client peer connected to server")
            {
                for (auto i = 0; i < 3; ++i)
                {
                    clientPeer.connect(server.serverAddress().toString().toStdString(), server.serverPort());
                    REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                    REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                    REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                    REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                }
            }
        }
    }
}
