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
#include "AsyncQObject.h"
#include <Tests/Resources/TcpServer.h>
#include <Tests/Resources/TestHostNamesFetcher.h>
#include <QTcpSocket>
#include <QTcpServer>
#include <QSemaphore>
#include <QFile>
#include <QThread>
#include <QElapsedTimer>
#include <QDeadlineTimer>
#include <QRandomGenerator64>
#include <QHostInfo>
#include <QHostAddress>
#include <chrono>
#include <memory>
#include <fstream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <Spectator.h>

using Kourier::TcpServer;
using Kourier::TcpSocket;
using Kourier::Object;
using Kourier::AsyncQObject;
using Kourier::TestResources::TestHostNamesFetcher;
using Spectator::SemaphoreAwaiter;


namespace TcpSocketTests
{
struct MemoryLimits
{
    int minValue = 0;
    int defaultValue = 0;
    int maxValue = 0;
    static MemoryLimits fromFile(QString filePath)
    {
        QFile file(filePath);
        REQUIRE(file.open(QIODevice::ReadOnly));
        const auto contents = file.readAll();
        REQUIRE(!contents.isEmpty());
        const auto values = contents.simplified().split(' ');
        REQUIRE(values.size() == 3);
        bool ok = false;
        MemoryLimits mLimits;
        mLimits.minValue = values[0].toInt(&ok);
        REQUIRE(ok);
        mLimits.defaultValue = values[1].toInt(&ok);
        REQUIRE(ok);
        mLimits.maxValue = values[2].toInt(&ok);
        REQUIRE(ok);
        return mLimits;
    }
};

const static auto wMemLimits = MemoryLimits::fromFile("/proc/sys/net/ipv4/tcp_wmem");
const static auto rMemLimits = MemoryLimits::fromFile("/proc/sys/net/ipv4/tcp_rmem");

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
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerConnectedSemaphore, 10));

                AND_WHEN("client peer sends a PING message to the server peer")
                {
                    clientPeer.write("PING");

                    THEN("server peer responds with a PONG message and closes the connection")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerReceivedPingSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerReceivedPongSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerDisconnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerDisconnectedSemaphore, 10));
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
                REQUIRE(socket.bind(address));
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
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerConnectedSemaphore, 10));
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
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerReceivedPingSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerReceivedPongSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerDisconnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerDisconnectedSemaphore, 10));
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
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerConnectedSemaphore, 10));
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerConnectedSemaphore, 10));
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
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerReceivedPingSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerReceivedPongSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerDisconnectedSemaphore, 10));
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
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerConnectedSemaphore, 10));
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerConnectedSemaphore, 10));

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
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(connectedPeerReceivedDataSemaphore, 1));
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
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(connectedPeerReceivedDataSemaphore, 1));
                        }

                        AND_WHEN("TcpSocket disconnects from connected peer")
                        {
                            pSendingPeer->disconnectFromPeer();

                            THEN("both peers disconnect")
                            {
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerDisconnectedSemaphore, 10));
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerDisconnectedSemaphore, 10));
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
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(connectedPeerReceivedDataSemaphore, 1));
                }

                AND_THEN("both peers disconnect without emitting any errors")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerDisconnectedSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerDisconnectedSemaphore, 10));
                    REQUIRE(pSendingPeer->errorMessage().empty());
                    REQUIRE(pReceivingPeer->errorMessage().empty());

                    AND_WHEN("sending peer is deleted")
                    {
                        pSendingPeer.reset(nullptr);

                        THEN("neither peer emit any error")
                        {
                            REQUIRE(!SemaphoreAwaiter::signalSlotAwareWait(clientPeerFailedSemaphore, QDeadlineTimer(1)));
                            REQUIRE(!serverPeerFailedSemaphore.tryAcquire());
                        }
                    }

                    AND_WHEN("receiving peer is deleted")
                    {
                        pReceivingPeer.reset(nullptr);

                        THEN("neither peer emit any error")
                        {
                            REQUIRE(!SemaphoreAwaiter::signalSlotAwareWait(clientPeerFailedSemaphore, QDeadlineTimer(1)));
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
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(connectedPeerDisconnectedSemaphore, 10));
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
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(connectedPeerDisconnectedSemaphore, 10));
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
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerDisconnectedSemaphore, 10));
                REQUIRE(pTcpSocket->errorMessage().empty());
                REQUIRE(pConnectedPeer->errorMessage().empty());

                AND_WHEN("TcpSocket is deleted")
                {
                    auto &tcpSocketFailedSemaphore = isTcpSocketTheClient ? clientPeerFailedSemaphore : serverPeerFailedSemaphore;
                    auto &connectedPeerFailedSemaphore = isTcpSocketTheClient ? serverPeerFailedSemaphore : clientPeerFailedSemaphore;
                    pTcpSocket.reset(nullptr);

                    THEN("neither peer emit any error")
                    {
                        REQUIRE(!SemaphoreAwaiter::signalSlotAwareWait(connectedPeerFailedSemaphore, QDeadlineTimer(1)));
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
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(connectedPeerDisconnectedSemaphore, 10));
                REQUIRE(pTcpSocket->errorMessage().empty());
                REQUIRE(pConnectedPeer->errorMessage().empty());

                AND_WHEN("TcpSocket is deleted")
                {
                    auto &tcpSocketFailedSemaphore = isTcpSocketTheClient ? clientPeerFailedSemaphore : serverPeerFailedSemaphore;
                    auto &connectedPeerFailedSemaphore = isTcpSocketTheClient ? serverPeerFailedSemaphore : clientPeerFailedSemaphore;
                    pTcpSocket.reset(nullptr);

                    THEN("neither peer emit any error")
                    {
                        REQUIRE(!SemaphoreAwaiter::signalSlotAwareWait(connectedPeerFailedSemaphore, QDeadlineTimer(1)));
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
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerDisconnectedSemaphore, 10));
                REQUIRE(pTcpSocket->errorMessage().empty());
                REQUIRE(pConnectedPeer->errorMessage().empty());

                AND_WHEN("TcpSocket is deleted")
                {
                    auto &tcpSocketFailedSemaphore = isTcpSocketTheClient ? clientPeerFailedSemaphore : serverPeerFailedSemaphore;
                    auto &connectedPeerFailedSemaphore = isTcpSocketTheClient ? serverPeerFailedSemaphore : clientPeerFailedSemaphore;
                    pTcpSocket.reset(nullptr);

                    THEN("neither peer emit any error")
                    {
                        REQUIRE(!SemaphoreAwaiter::signalSlotAwareWait(connectedPeerFailedSemaphore, QDeadlineTimer(1)));
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
                        REQUIRE(!SemaphoreAwaiter::signalSlotAwareWait(connectedPeerFailedSemaphore, QDeadlineTimer(1)));
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
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(connectedPeerDisconnectedSemaphore, 10));
                auto &tcpSocketFailedSemaphore = isTcpSocketTheClient ? clientPeerFailedSemaphore : serverPeerFailedSemaphore;
                auto &connectedPeerFailedSemaphore = isTcpSocketTheClient ? serverPeerFailedSemaphore : clientPeerFailedSemaphore;
                REQUIRE(!SemaphoreAwaiter::signalSlotAwareWait(connectedPeerFailedSemaphore, QDeadlineTimer(1)));
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
            socket.setConnectTimeout(std::chrono::milliseconds(5));
            auto hostName = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses().first;
            socket.connect(hostName, 5000);

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketFailedSemaphore, 10));
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

        WHEN("a TcpSocket bounded to a IPV4 address is connected to the IPV6 server by host address")
        {
            TcpSocket socket;
            QSemaphore socketFailedSemaphore;
            Object::connect(&socket, &TcpSocket::error, [&](){socketFailedSemaphore.release();});
            socket.setBindAddressAndPort("127.2.2.5");
            socket.setConnectTimeout(std::chrono::milliseconds(5));
            socket.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketFailedSemaphore, 10));
                REQUIRE(socket.state() == TcpSocket::State::Unconnected);
                REQUIRE(socket.errorMessage().starts_with("Failed to connect to [::1]:"));
            }
        }

        WHEN("a TcpSocket bounded to a IPV4 address is connected to the IPV6 server by host name")
        {
            TcpSocket socket;
            QSemaphore socketFailedSemaphore;
            Object::connect(&socket, &TcpSocket::error, [&](){socketFailedSemaphore.release();});
            socket.setBindAddressAndPort("127.2.2.5");
            socket.setConnectTimeout(std::chrono::milliseconds(5));
            socket.connect(hostName, server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketFailedSemaphore, 10));
                REQUIRE(socket.state() == TcpSocket::State::Unconnected);
                REQUIRE(socket.errorMessage().starts_with(std::string("Failed to connect to ").append(hostName).append(" at [::1]:")));
            }
        }

        WHEN("TcpSocket bounded to a privileged port on IPV6 is connected to the server")
        {
            TcpSocket socket;
            QSemaphore socketFailedSemaphore;
            Object::connect(&socket, &TcpSocket::error, [&](){socketFailedSemaphore.release();});
            socket.setBindAddressAndPort("::1", 443);
            socket.setConnectTimeout(std::chrono::milliseconds(5));
            socket.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketFailedSemaphore, 10));
                REQUIRE(socket.state() == TcpSocket::State::Unconnected);
                REQUIRE(socket.errorMessage() == "Failed to bind socket to [::1]:443. POSIX error EACCES(13): Permission denied.");
            }
        }

        WHEN("TcpSocket bound to an already used address/port pair is connected to server")
        {
            TcpSocket previouslyConnectedSocket;
            QSemaphore previouslyConnectedSocketSemaphore;
            Object::connect(&previouslyConnectedSocket, &TcpSocket::connected, [&](){previouslyConnectedSocketSemaphore.release();});
            previouslyConnectedSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());
            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(previouslyConnectedSocketSemaphore, 10));
            TcpSocket socket;
            QSemaphore socketFailedSemaphore;
            Object::connect(&socket, &TcpSocket::error, [&](){socketFailedSemaphore.release();});
            socket.setBindAddressAndPort(previouslyConnectedSocket.localAddress(), previouslyConnectedSocket.localPort());
            socket.setConnectTimeout(std::chrono::milliseconds(5));
            socket.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketFailedSemaphore, 10));
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

        WHEN("a TcpSocket bounded to a IPV6 address is connected to the IPV4 server by host address")
        {
            TcpSocket socket;
            QSemaphore socketFailedSemaphore;
            Object::connect(&socket, &TcpSocket::error, [&](){socketFailedSemaphore.release();});
            socket.setBindAddressAndPort("::1");
            socket.setConnectTimeout(std::chrono::milliseconds(5));
            socket.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketFailedSemaphore, 10));
                REQUIRE(socket.state() == TcpSocket::State::Unconnected);
                REQUIRE(socket.errorMessage().starts_with(std::string("Failed to connect to ") + hostAddress.toString().toStdString() + std::string(":")));
                REQUIRE(connectionCount == 0);
            }
        }

        WHEN("a TcpSocket bounded to a IPV6 address is connected to the IPV4 server by host name")
        {
            TcpSocket socket;
            QSemaphore socketFailedSemaphore;
            Object::connect(&socket, &TcpSocket::error, [&](){socketFailedSemaphore.release();});
            socket.setBindAddressAndPort("::1");
            socket.setConnectTimeout(std::chrono::milliseconds(5));
            socket.connect(hostName, server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketFailedSemaphore, 10));
                REQUIRE(socket.state() == TcpSocket::State::Unconnected);
                REQUIRE(socket.errorMessage().starts_with(std::string("Failed to connect to ").append(hostName).append(" at ").append(hostAddress.toString().toStdString()).append(":")));
                REQUIRE(connectionCount == 0);
            }
        }

        WHEN("TcpSocket bound to a privileged port on IPV4 is connected to server")
        {
            TcpSocket socket;
            QSemaphore socketFailedSemaphore;
            Object::connect(&socket, &TcpSocket::error, [&](){socketFailedSemaphore.release();});
            socket.setBindAddressAndPort("127.0.0.1", 443);
            socket.setConnectTimeout(std::chrono::milliseconds(5));
            socket.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketFailedSemaphore, 10));
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
            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(previouslyConnectedSocketSemaphore, 10));
            TcpSocket socket;
            QSemaphore socketFailedSemaphore;
            Object::connect(&socket, &TcpSocket::error, [&](){socketFailedSemaphore.release();});
            socket.setBindAddressAndPort(previouslyConnectedSocket.localAddress(), previouslyConnectedSocket.localPort());
            socket.setConnectTimeout(std::chrono::milliseconds(5));
            socket.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketFailedSemaphore, 10));
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
        QObject::connect(&server, &QTcpServer::newConnection, [](){FAIL("This code is supposed to be unreachable.");});
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
            isServerAcceptingConnections = SemaphoreAwaiter::signalSlotAwareWait(connectedSemaphore, QDeadlineTimer(100));
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
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketFailedSemaphore, 10));
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
        QSemaphore peerConnectedSemaphore;
        QSemaphore peerDisconnectedSemaphore;
        QSemaphore peerFailedSemaphore;
        TcpServer server;
        std::unique_ptr<TcpSocket> pPeerSocket;
        Object::connect(&server, &TcpServer::newConnection, [&](TcpSocket *pSocket)
        {
            REQUIRE(!pPeerSocket);
            pPeerSocket.reset(pSocket);
            REQUIRE(pPeerSocket.get() != nullptr);
            Object::connect(pPeerSocket.get(), &TcpSocket::error, [&]() {peerFailedSemaphore.release();});
            Object::connect(pPeerSocket.get(), &TcpSocket::disconnected, [&](){peerDisconnectedSemaphore.release();});
            peerConnectedSemaphore.release(2);
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
        QSemaphore socketConnectedSemaphore;
        QSemaphore socketDisconnectedSemaphore;
        QSemaphore socketFailedSemaphore;
        std::unique_ptr<TcpSocket> pSocket(new TcpSocket);
        Object::connect(pSocket.get(), &TcpSocket::disconnected, [&](){socketDisconnectedSemaphore.release();});
        Object::connect(pSocket.get(), &TcpSocket::error, [&](){socketFailedSemaphore.release();});

        WHEN("TcpSocket connects to server and disconnects while handling the connected signal")
        {
            Object::connect(pSocket.get(), &TcpSocket::connected, [&]()
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                    socketConnectedSemaphore.release();
                    pSocket->disconnectFromPeer();
                });
            pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("TcpSocket and peer connect and then disconnect")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                REQUIRE(pSocket->state() == TcpSocket::State::Unconnected);
            }
        }

        WHEN("TcpSocket connects to server and aborts connection while handling the connected signal")
        {
            Object::connect(pSocket.get(), &TcpSocket::connected, [&]()
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                    socketConnectedSemaphore.release();
                    pSocket->abort();
                });
            pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("TcpSocket and peer connect and then peer disconnects")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                REQUIRE(pSocket->state() == TcpSocket::State::Unconnected);
            }
        }

        WHEN("TcpSocket connects to server and is destroyed while handling the connected signal")
        {
            Object::connect(pSocket.get(), &TcpSocket::connected, [&]()
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                    socketConnectedSemaphore.release();
                    pSocket.release()->scheduleForDeletion();
                });
            pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("TcpSocket and peer connect and then peer disconnects")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
            }
        }
        
        WHEN("TcpSocket connects to server and connects again while handling the connected signal")
        {
            Object *pCtxObject = new Object;
            Object::connect(pSocket.get(), &TcpSocket::connected, pCtxObject, [&, ptrCtxObject = pCtxObject]()
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                socketConnectedSemaphore.release();
                Object::connect(pPeerSocket.get(), &TcpSocket::disconnected, [pPeer = pPeerSocket.get()](){pPeer->scheduleForDeletion();});
                pPeerSocket.release();
                Object::connect(pSocket.get(), &TcpSocket::connected, [&]()
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                        socketConnectedSemaphore.release();
                    });
                pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());
                delete ptrCtxObject;
            });
            pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("TcpSocket and peer connect, TcpSocket aborts and peer disconnects and then both TcpSocket and peer connect")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
            }
        }

        
        WHEN("TcpSocket connects to server and connects to a non-existent server address while handling the connected signal")
        {
            Object::connect(pSocket.get(), &TcpSocket::connected, [&]()
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                socketConnectedSemaphore.release();
                Object::connect(pPeerSocket.get(), &TcpSocket::disconnected, [pPeer = pPeerSocket.get()](){pPeer->scheduleForDeletion();});
                pPeerSocket.release();
                auto const serverAddress = QHostAddress("127.1.2.3");
                QTcpSocket socket;
                REQUIRE(socket.bind(serverAddress));
                const auto unusedPortForNow = socket.localPort();
                socket.abort();
                pSocket->connect(serverAddress.toString().toStdString(), unusedPortForNow);
            });
            pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("TcpSocket and peer connect, TcpSocket aborts and peer disconnects and then TcpSocket fails to connect to the non-existent server")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketFailedSemaphore, 10));
                REQUIRE(pSocket->errorMessage().starts_with("Failed to connect to 127.1.2.3:"));
            }
        }

        WHEN("TcpSocket connects to server")
        {
            Object::connect(pSocket.get(), &TcpSocket::connected, [&]()
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                    socketConnectedSemaphore.release();
                });
            pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());
            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));

            THEN("connected peers can start exchanging data")
            {
                AND_WHEN("connected peer sends some data to TcpSocket and TcpSocket disconnects while handling the receivedData signal")
                {
                    Object::connect(pSocket.get(), &TcpSocket::receivedData, [&](){pSocket->disconnectFromPeer();});
                    pPeerSocket->write("abcdefgh");

                    THEN("TcpSocket and peer disconnect")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                    }
                }

                AND_WHEN("connected peer sends some data to TcpSocket and TcpSocket aborts connection while handling the receivedData signal")
                {
                    Object::connect(pSocket.get(), &TcpSocket::receivedData, [&](){pSocket->abort();});
                    pPeerSocket->write("abcdefgh");

                    THEN("TcpSocket aborts and peer disconnects")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                        REQUIRE(pSocket->state() == TcpSocket::State::Unconnected);
                    }
                }

                AND_WHEN("connected peer sends some data to TcpSocket and TcpSocket is destroyed while handling the receivedData signal")
                {
                    auto *pReleasedSocket = pSocket.release();
                    Object::connect(pReleasedSocket, &TcpSocket::receivedData, [=](){pReleasedSocket->scheduleForDeletion();});
                    pPeerSocket->write("abcdefgh");

                    THEN("peer disconnects")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                    }
                }

                AND_WHEN("connected peer sends some data to TcpSocket and TcpSocket is reconnected while handling the receivedData signal")
                {
                    Object::connect(pPeerSocket.get(), &TcpSocket::disconnected, [pPeer = pPeerSocket.get()](){pPeer->scheduleForDeletion();});
                    Object::connect(pSocket.get(), &TcpSocket::receivedData, [&]()
                    {
                        pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());
                    });
                    pPeerSocket->write("abcdefgh");
                    pPeerSocket.release();

                    THEN("TcpSocket aborts and peer disconnects and then TcpSocket reconnects to another peer")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                        pSocket->disconnectFromPeer();
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                    }
                }

                AND_WHEN("TcpSocket sends more data than the socket's send buffer can store and TcpSocket disconnects while handling the sentData signal with data still to be written")
                {
                    Object::connect(pSocket.get(), &TcpSocket::sentData, [&]()
                    {
                        if (pSocket->dataToWrite())
                            pSocket->disconnectFromPeer();
                    });
                    const auto socketSendBufferSize = pSocket->getSocketOption(TcpSocket::SocketOption::SendBufferSize);
                    REQUIRE(socketSendBufferSize > 1);
                    QByteArray dataToSend(3 * socketSendBufferSize, ' ');
                    pSocket->write(dataToSend.data(), dataToSend.size());

                    THEN("TcpSocket and peer disconnect")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                    }
                }

                AND_WHEN("TcpSocket sends more data than the socket's send buffer can store and TcpSocket disconnects while handling the sentData signal with no more data still to be written")
                {
                    Object::connect(pSocket.get(), &TcpSocket::sentData, [&]()
                    {
                        if (!pSocket->dataToWrite())
                            pSocket->disconnectFromPeer();
                    });
                    const auto socketSendBufferSize = pSocket->getSocketOption(TcpSocket::SocketOption::SendBufferSize);
                    REQUIRE(socketSendBufferSize > 1);
                    QByteArray dataToSend(3 * socketSendBufferSize, ' ');
                    pSocket->write(dataToSend.data(), dataToSend.size());

                    THEN("TcpSocket and peer disconnect")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                    }
                }

                AND_WHEN("TcpSocket sends more data than the socket's send buffer can store and TcpSocket aborts connection while handling the sentData signal with data still to be written")
                {
                    Object::connect(pSocket.get(), &TcpSocket::sentData, [&]()
                    {
                        if (pSocket->dataToWrite())
                            pSocket->abort();
                    });
                    const auto socketSendBufferSize = pSocket->getSocketOption(TcpSocket::SocketOption::SendBufferSize);
                    REQUIRE(socketSendBufferSize > 1);
                    QByteArray dataToSend(3 * socketSendBufferSize, ' ');
                    pSocket->write(dataToSend.data(), dataToSend.size());

                    THEN("peer disconnects")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                        REQUIRE(pSocket->state() == TcpSocket::State::Unconnected);
                    }
                }

                AND_WHEN("TcpSocket sends more data than the socket's send buffer can store and TcpSocket aborts connection while handling the sentData signal with no more data data still to be written")
                {
                    Object::connect(pSocket.get(), &TcpSocket::sentData, [&]()
                    {
                        if (!pSocket->dataToWrite())
                            pSocket->abort();
                    });
                    const auto socketSendBufferSize = pSocket->getSocketOption(TcpSocket::SocketOption::SendBufferSize);
                    REQUIRE(socketSendBufferSize > 1);
                    QByteArray dataToSend(3 * socketSendBufferSize, ' ');
                    pSocket->write(dataToSend.data(), dataToSend.size());

                    THEN("peer disconnects")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                        REQUIRE(pSocket->state() == TcpSocket::State::Unconnected);
                    }
                }

                AND_WHEN("TcpSocket sends more data than the socket's send buffer can store and TcpSocket is destroyed while handling the sentData signal with data still to be written")
                {
                    Object::connect(pSocket.get(), &TcpSocket::sentData, [&, ptrSocket = pSocket.get()]()
                    {
                        if (ptrSocket->dataToWrite())
                        {
                            ptrSocket->scheduleForDeletion();
                            pSocket.release();
                        }
                    });
                    const auto socketSendBufferSize = pSocket->getSocketOption(TcpSocket::SocketOption::SendBufferSize);
                    REQUIRE(socketSendBufferSize > 1);
                    QByteArray dataToSend(3 * socketSendBufferSize, ' ');
                    pSocket->write(dataToSend.data(), dataToSend.size());

                    THEN("peer disconnects")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                    }
                }

                AND_WHEN("TcpSocket sends more data than the socket's send buffer can store and TcpSocket is destroyed while handling the sentData signal with no more data still to be written")
                {
                    Object::connect(pSocket.get(), &TcpSocket::sentData, [&]()
                    {
                        if (!pSocket->dataToWrite())
                            pSocket.release()->scheduleForDeletion();
                    });
                    const auto socketSendBufferSize = pSocket->getSocketOption(TcpSocket::SocketOption::SendBufferSize);
                    REQUIRE(socketSendBufferSize > 1);
                    QByteArray dataToSend(3 * socketSendBufferSize, ' ');
                    pSocket->write(dataToSend.data(), dataToSend.size());

                    THEN("peer disconnects")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                    }
                }

                AND_WHEN("TcpSocket sends more data than the socket's send buffer can store and TcpSocket is reconnected while handling the sentData signal with data still to be written")
                {
                    auto *pPeer = pPeerSocket.release();
                    Object::connect(pPeer, &TcpSocket::disconnected, [=](){pPeer->scheduleForDeletion();});

                    Object::connect(pSocket.get(), &TcpSocket::sentData, [&]()
                    {
                        if (pSocket->dataToWrite())
                            pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());
                    });
                    const auto socketSendBufferSize = pSocket->getSocketOption(TcpSocket::SocketOption::SendBufferSize);
                    REQUIRE(socketSendBufferSize > 1);
                    QByteArray dataToSend(3 * socketSendBufferSize, ' ');
                    pSocket->write(dataToSend.data(), dataToSend.size());

                    THEN("peer disconnects and TcpSocket aborts and reconnects")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                        pSocket->disconnectFromPeer();
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                    }
                }

                AND_WHEN("TcpSocket sends more data than the socket's send buffer can store and TcpSocket is reconnected while handling the sentData signal with no more data still to be written")
                {
                    auto *pPeer = pPeerSocket.release();
                    Object::connect(pPeer, &TcpSocket::disconnected, [=](){pPeer->scheduleForDeletion();});

                    Object::connect(pSocket.get(), &TcpSocket::sentData, [&]()
                    {
                        if (!pSocket->dataToWrite())
                            pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());
                    });
                    const auto socketSendBufferSize = pSocket->getSocketOption(TcpSocket::SocketOption::SendBufferSize);
                    REQUIRE(socketSendBufferSize > 1);
                    QByteArray dataToSend(3 * socketSendBufferSize, ' ');
                    pSocket->write(dataToSend.data(), dataToSend.size());

                    THEN("peer disconnects and TcpSocket aborts and reconnects")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                        pSocket->disconnectFromPeer();
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                    }
                }
            }

            AND_WHEN("connected peer disconnects and TcpSocket is disconnected while handling the disconnected signal")
            {
                QSemaphore socketDisconnectedFromPeerSemaphore;
                Object::connect(pSocket.get(), &TcpSocket::disconnected, [&]()
                {
                    pSocket->disconnectFromPeer();
                    socketDisconnectedFromPeerSemaphore.release();
                });
                pPeerSocket->disconnectFromPeer();

                THEN("TcpSocket and peer disconnect")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedFromPeerSemaphore, 10));
                }
            }

            AND_WHEN("connected peer disconnects and TcpSocket aborts connection while handling the disconnected signal")
            {
                QSemaphore socketDisconnectedFromPeerSemaphore;
                Object::connect(pSocket.get(), &TcpSocket::disconnected, [&]()
                {
                    pSocket->abort();
                    socketDisconnectedFromPeerSemaphore.release();
                });
                pPeerSocket->disconnectFromPeer();

                THEN("TcpSocket and peer disconnect")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedFromPeerSemaphore, 10));
                }
            }

            AND_WHEN("connected peer disconnects and TcpSocket is destroyed while handling the disconnected signal")
            {
                QSemaphore socketDisconnectedFromPeerSemaphore;
                Object::connect(pSocket.get(), &TcpSocket::disconnected, [&]()
                {
                    pSocket.release()->scheduleForDeletion();
                    socketDisconnectedFromPeerSemaphore.release();
                });
                pPeerSocket->disconnectFromPeer();

                THEN("TcpSocket and peer disconnect")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedFromPeerSemaphore, 10));
                }
            }

            AND_WHEN("connected peer disconnects and TcpSocket is reconnected while handling the disconnected signal")
            {
                QSemaphore socketDisconnectedFromPeerSemaphore;
                auto *pCtxObject = new Object;
                Object::connect(pSocket.get(), &TcpSocket::disconnected, pCtxObject, [&, ptrCtxObject = pCtxObject]()
                {
                    socketDisconnectedFromPeerSemaphore.release();
                    pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());
                    delete ptrCtxObject;
                });
                pPeerSocket->disconnectFromPeer();
                pPeerSocket.release()->scheduleForDeletion();

                THEN("TcpSocket and peer disconnect and then TcpSocket reconnects to another peer")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedFromPeerSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                    pSocket->disconnectFromPeer();
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                }
            }
        }

        WHEN("TcpSocket tries to connect to a non-existent server by address and TcpSocket is disconnected while handling the error signal")
        {
            QSemaphore socketHandledErrorSemaphore;
            Object::connect(pSocket.get(), &TcpSocket::error, [&]()
            {
                REQUIRE(!pSocket->errorMessage().empty());
                pSocket->disconnectFromPeer();
                socketHandledErrorSemaphore.release();
            });
            auto const serverAddress = QHostAddress("127.1.2.3");
            QTcpSocket socket;
            REQUIRE(socket.bind(serverAddress));
            const auto unusedPortForNow = socket.localPort();
            socket.abort();
            pSocket->connect(serverAddress.toString().toStdString(), unusedPortForNow);

            THEN("socket handles error")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
            }
        }

        WHEN("TcpSocket tries to connect to a non-existent server by address and TcpSocket aborts connection while handling the error signal")
        {
            QSemaphore socketHandledErrorSemaphore;
            Object::connect(pSocket.get(), &TcpSocket::error, [&]()
            {
                REQUIRE(!pSocket->errorMessage().empty());
                pSocket->abort();
                socketHandledErrorSemaphore.release();
            });
            auto const serverAddress = QHostAddress("127.1.2.3");
            QTcpSocket socket;
            REQUIRE(socket.bind(serverAddress));
            const auto unusedPortForNow = socket.localPort();
            socket.abort();
            pSocket->connect(serverAddress.toString().toStdString(), unusedPortForNow);

            THEN("socket handles error")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
            }
        }

        WHEN("TcpSocket tries to connect to a non-existent server by address and TcpSocket is destroyed while handling the error signal")
        {
            QSemaphore socketHandledErrorSemaphore;
            Object::connect(pSocket.get(), &TcpSocket::error, [&]()
            {
                REQUIRE(!pSocket->errorMessage().empty());
                pSocket.release()->scheduleForDeletion();
                socketHandledErrorSemaphore.release();
            });
            auto const serverAddress = QHostAddress("127.1.2.3");
            QTcpSocket socket;
            REQUIRE(socket.bind(serverAddress));
            const auto unusedPortForNow = socket.localPort();
            socket.abort();
            pSocket->connect(serverAddress.toString().toStdString(), unusedPortForNow);

            THEN("socket handles error")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
            }
        }

        WHEN("TcpSocket tries to connect to a non-existent server by address and TcpSocket is reconnected to the running server while handling the error signal")
        {
            QSemaphore socketHandledErrorSemaphore;
            Object::connect(pSocket.get(), &TcpSocket::connected, [&]()
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                    socketConnectedSemaphore.release();
                });
            Object::connect(pSocket.get(), &TcpSocket::error, [&]()
                {
                    REQUIRE(!pSocket->errorMessage().empty());
                    REQUIRE(!socketConnectedSemaphore.tryAcquire());
                    pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());
                    socketHandledErrorSemaphore.release();
                });
            auto const serverAddress = QHostAddress("127.1.2.3");
            QTcpSocket socket;
            REQUIRE(socket.bind(serverAddress));
            const auto unusedPortForNow = socket.localPort();
            socket.abort();
            pSocket->connect(serverAddress.toString().toStdString(), unusedPortForNow);

            THEN("TcpSocket handles error, aborts and connects to peer")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                REQUIRE(pSocket->errorMessage().empty());
                pSocket->disconnectFromPeer();
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
            }
        }

        WHEN("TcpSocket tries to connect to host name with IPV4/IPV6 addresses without any server running and disconnects while handling the error signal")
        {
            QSemaphore socketHandledErrorSemaphore;
            Object::connect(pSocket.get(), &TcpSocket::error, [&]()
                {
                    REQUIRE(!pSocket->errorMessage().empty());
                    pSocket->disconnectFromPeer();
                    socketHandledErrorSemaphore.release();
                });
            pSocket->setConnectTimeout(std::chrono::milliseconds(5));
            auto hostName = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses().first;
            pSocket->connect(hostName, 3008);

            THEN("TcpSocket handles error")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
            }
        }

        WHEN("TcpSocket tries to connect to host name with IPV4/IPV6 addresses without any server running and aborts connection while handling the error signal")
        {
            QSemaphore socketHandledErrorSemaphore;
            Object::connect(pSocket.get(), &TcpSocket::error, [&]()
            {
                REQUIRE(!pSocket->errorMessage().empty());
                pSocket->abort();
                socketHandledErrorSemaphore.release();
            });
            auto hostName = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses().first;
            pSocket->connect(hostName, 3008);

            THEN("TcpSocket handles error")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
            }
        }

        WHEN("TcpSocket tries to connect to host name with IPV4/IPV6 addresses without any server running and is destroyed while handling the error signal")
        {
            QSemaphore socketHandledErrorSemaphore;
            Object::connect(pSocket.get(), &TcpSocket::error, [&]()
            {
                REQUIRE(!pSocket->errorMessage().empty());
                pSocket.release()->scheduleForDeletion();
                socketHandledErrorSemaphore.release();
            });
            auto hostName = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses().first;
            pSocket->connect(hostName, 3008);

            THEN("TcpSocket handles error")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
            }
        }

        WHEN("TcpSocket tries to connect to host name with IPV4/IPV6 addresses without any server running and is reconnected to the running server while handling the error signal")
        {
            QSemaphore socketHandledErrorSemaphore;
            Object::connect(pSocket.get(), &TcpSocket::error, [&]()
            {
                REQUIRE(!pSocket->errorMessage().empty());
                REQUIRE(!socketConnectedSemaphore.tryAcquire());
                pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());
                socketHandledErrorSemaphore.release();
            });
            Object::connect(pSocket.get(), &TcpSocket::connected, [&]()
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                    socketConnectedSemaphore.release();
                });
            auto hostName = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses().first;
            pSocket->connect(hostName, 3008);

            THEN("TcpSocket handles error and aborts before connecting to peer")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                pSocket->disconnectFromPeer();
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
            }
        }
    }
}


SCENARIO("TcpSockets can be reused")
{
    GIVEN("A running server whose sockets close the connections just after they get established")
    {
        TcpServer server;
        QSemaphore peerConnectedSemaphore;
        QSemaphore peerDisconnectedSemaphore;
        std::unique_ptr<TcpSocket> pPeerSocket;
        Object::connect(&server, &TcpServer::newConnection, [&](TcpSocket *pSocket)
            {
                REQUIRE(!pPeerSocket);
                pPeerSocket.reset(pSocket);
                Object::connect(pPeerSocket.get(), &TcpSocket::disconnected, [&](){pPeerSocket.release()->scheduleForDeletion(); peerDisconnectedSemaphore.release();});
                pPeerSocket->disconnectFromPeer();
                peerConnectedSemaphore.release();
            });
        REQUIRE(server.listen(QHostAddress::LocalHost));

        WHEN("the same socket connects with server three times")
        {
            TcpSocket socket;
            QSemaphore socketConnectedSemaphore;
            QSemaphore socketDisconnectedSemaphore;
            Object::connect(&socket, &TcpSocket::connected, [&](){socketConnectedSemaphore.release();});
            Object::connect(&socket, &TcpSocket::disconnected, [&](){socketDisconnectedSemaphore.release();});

            THEN("socket connects and then disconnects from server as many times as socket was connected to server")
            {
                for (auto i = 0; i < 3; ++i)
                {
                    socket.connect(server.serverAddress().toString().toStdString(), server.serverPort());
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                }
            }
        }
    }
}


namespace TcpSocketTests
{

class ClientTcpSockets : public QObject
{
Q_OBJECT
public:
    ClientTcpSockets(std::string_view serverAddress,
                     uint16_t serverPort,
                     std::string_view bindAddress,
                     size_t totalConnections,
                     size_t workingConnections,
                     size_t requestsPerWorkingConnection,
                     int a,
                     int b) :
        m_serverAddress(serverAddress),
        m_serverPort(serverPort),
        m_bindAddress(bindAddress),
        m_totalConnections(totalConnections),
        m_workingConnections(workingConnections),
        m_requestsPerWorkingConnection(requestsPerWorkingConnection),
        m_a(a),
        m_b(b)
    {
        REQUIRE(!m_serverAddress.empty()
                && (m_serverPort >= 1024)
                && m_totalConnections > 0
                && m_workingConnections > 0
                && (m_totalConnections >= m_workingConnections)
                && m_requestsPerWorkingConnection > 0);
        m_socketRequestCountPairs.resize(m_totalConnections);
        for (auto &pSocketRequestCount : m_socketRequestCountPairs)
            pSocketRequestCount = {new TcpSocket, 0};
    }
    ~ClientTcpSockets() override = default;

public slots:
    void connectToServer() {connectToServerInternal();}
    void sendRequests()
    {
        for (auto i = 0; i < m_workingConnections; ++i)
        {
            auto &socketRequestCountPair = m_socketRequestCountPairs[i];
            ++socketRequestCountPair.second;
            socketRequestCountPair.first->write((char*)&m_a, sizeof(m_a));
            socketRequestCountPair.first->write((char*)&m_b, sizeof(m_b));
        }
    }
    void disconnectFromServer()
    {
        for (auto &socketRequestCountPair : m_socketRequestCountPairs)
            socketRequestCountPair.first->disconnectFromPeer();
    }

signals:
    void connectedToServer();
    void receivedResponses();
    void disconnectedFromServer();

private slots:
    void connectToServerInternal()
    {
        const auto upTo = qMin(m_totalConnections, m_connectionsPerBatch + m_currentConnectIndex);
        const auto startIndex = m_currentConnectIndex;
        for (auto i = startIndex; i < upTo; ++i)
        {
            ++m_currentConnectIndex;
            auto *socketRequestCounterPair = &m_socketRequestCountPairs[i];
            Object::connect(socketRequestCounterPair->first, &TcpSocket::connected, [this]()
            {
                if (++m_connectionCount == m_totalConnections)
                {
                    m_hasConnectedAllClients = true;
                    emit connectedToServer();
                }
                else if (++m_batchConnectionCount == m_connectionsPerBatch)
                {
                    m_batchConnectionCount = 0;
                    QMetaObject::invokeMethod(this, "connectToServerInternal", Qt::QueuedConnection);
                }
            });
            Object::connect(socketRequestCounterPair->first, &TcpSocket::receivedData, [this, socketRequestCounterPair]()
            {
                if (socketRequestCounterPair->first->dataAvailable() != sizeof(int))
                    return;
                else
                {
                    int sum = 0;
                    socketRequestCounterPair->first->read((char*)&sum, sizeof(sum));
                    REQUIRE(sum == (m_a + m_b));
                    ++m_responseCount;
                    if (++socketRequestCounterPair->second <= m_requestsPerWorkingConnection)
                    {
                        socketRequestCounterPair->first->write((char*)&m_a, sizeof(m_a));
                        socketRequestCounterPair->first->write((char*)&m_b, sizeof(m_b));
                    }
                    else
                    {
                        if (m_responseCount == (m_requestsPerWorkingConnection * m_workingConnections))
                            emit receivedResponses();
                    }
                }
            });
            Object::connect(socketRequestCounterPair->first, &TcpSocket::disconnected, [this, socketRequestCounterPair]()
            {
                REQUIRE(m_hasConnectedAllClients);
                socketRequestCounterPair->first->scheduleForDeletion();
                if (++m_disconnectionCount == m_totalConnections)
                    emit disconnectedFromServer();
            });
            Object::connect(socketRequestCounterPair->first, &TcpSocket::error, [this, socketRequestCounterPair]()
            {
                REQUIRE(!m_hasConnectedAllClients);
                // binding failed
                REQUIRE(m_currentBindPort < 65534);
                socketRequestCounterPair->first->setBindAddressAndPort(m_bindAddress, ++m_currentBindPort);
                socketRequestCounterPair->first->connect(m_serverAddress, m_serverPort);
            });
            REQUIRE(m_currentBindPort < 65534);
            socketRequestCounterPair->first->setBindAddressAndPort(m_bindAddress, ++m_currentBindPort);
            socketRequestCounterPair->first->connect(m_serverAddress, m_serverPort);
        }
    }

private:
    size_t m_connectionCount = 0;
    size_t m_responseCount = 0;
    size_t m_disconnectionCount = 0;
    std::vector<std::pair<TcpSocket*, size_t>> m_socketRequestCountPairs;
    size_t m_currentConnectIndex = 0;
    size_t m_batchConnectionCount = 0;
    const size_t m_connectionsPerBatch = 2500;
    std::string m_serverAddress;
    std::string m_bindAddress;
    uint16_t m_currentBindPort = 33000;
    const uint16_t m_serverPort;
    const size_t m_totalConnections;
    const size_t m_workingConnections;
    const size_t m_requestsPerWorkingConnection;
    const int m_a;
    const int m_b;
    bool m_hasConnectedAllClients = false;
};


class ServerTcpSockets : public QObject
{
    Q_OBJECT
public:
    ServerTcpSockets(std::string_view serverAddress,
                     size_t totalConnections,
                     size_t requestsPerWorkingConnection) :
        m_serverAddress(serverAddress),
        m_totalConnections(totalConnections),
        m_requestsPerWorkingConnection(requestsPerWorkingConnection)
    {
        REQUIRE(!m_serverAddress.empty()
                && m_totalConnections > 0);
        m_pTcpServer = new TcpServer;
        m_pTcpServer->setListenBacklogSize(30000);
        m_pTcpServer->setMaxPendingConnections(30000);
        Object::connect(m_pTcpServer, &TcpServer::newConnection, [this](TcpSocket *pSocket)
        {
            Object::connect(pSocket, &TcpSocket::receivedData, [this, pSocket]()
            {
                if (pSocket->dataAvailable() != (2 * sizeof(int)))
                    return;
                else
                {
                    int a = 0;
                    pSocket->read((char*)&a, sizeof(a));
                    int b = 0;
                    pSocket->read((char*)&b, sizeof(b));
                    const int sum = a + b;
                    pSocket->write((char*)&sum, sizeof(sum));
                }
            });
            Object::connect(pSocket, &TcpSocket::disconnected, [this, pSocket]()
            {
                REQUIRE(m_hasConnectedToClients);
                pSocket->scheduleForDeletion();
                if (++m_disconnectionCount == m_totalConnections)
                {
                    m_pTcpServer->scheduleForDeletion();
                    m_pTcpServer = nullptr;
                    emit disconnectedFromClients();
                }
            });
            Object::connect(pSocket, &TcpSocket::error, [pSocket]()
            {
                FAIL("This code is supposed to be unreachable.");
            });
            if (++m_connectionCount == m_totalConnections)
            {
                m_pTcpServer->close();
                m_hasConnectedToClients = true;
                emit connectedToClients();
            }
        });
        REQUIRE(m_pTcpServer->listen(QHostAddress(QString::fromStdString(std::string(m_serverAddress)))));
        m_serverPort = m_pTcpServer->serverPort();
        REQUIRE(m_serverPort > 0);
    }
    ~ServerTcpSockets() override = default;
    uint16_t serverPort() const {return m_serverPort;}

signals:
    void connectedToClients();
    void disconnectedFromClients();

private:
    TcpServer *m_pTcpServer = nullptr;
    size_t m_connectionCount = 0;
    size_t m_disconnectionCount = 0;
    std::string_view m_serverAddress;
    uint16_t m_serverPort = 0;
    const size_t m_totalConnections;
    const size_t m_requestsPerWorkingConnection;
    bool m_hasConnectedToClients = false;
};

#if defined(__linux__)
static size_t getUsedMemory()
{
    int programMemory = 0;
    int nonProgramMemory = 0;
    int sharedMemory = 0;
    std::ifstream buffer("/proc/self/statm");
    buffer >> programMemory >> nonProgramMemory >> sharedMemory;
    buffer.close();
    static const auto pageSize = sysconf(_SC_PAGE_SIZE);
    return (nonProgramMemory - sharedMemory) * pageSize;
}
#endif

}

using namespace TcpSocketTests;


// SCENARIO("TcpSocket benchmarks")
// {
//     static constexpr std::string_view serverAddress("127.25.24.20");
//     static constexpr size_t totalConnectionsPerThread = 30000;
//     static constexpr size_t workingConnectionsPerThread = 30000;
//     static constexpr size_t clientThreadCount = 5;
//     static constexpr size_t totalConnections = totalConnectionsPerThread * clientThreadCount;
//     static constexpr size_t requestsPerWorkingConnection = 10;
//     static constexpr int a = 5;
//     static constexpr int b = 3;
//     size_t memoryConsumedAfterCreatingClientSockets = 0;
//     size_t memoryConsumedAfterConnecting = 0;
//     size_t memoryConsumedAfterResponses = 0;
//     size_t memoryConsumedAfterDisconnecting = 0;
//     QElapsedTimer elapsedTimer;
//     double connectionsPerSecond = 0;
//     double requestsPerSecond = 0;
//     double disconnectionsPerSecond = 0;
//     std::atomic_size_t connectedClientCount = 0;
//     std::atomic_size_t receivedResponseCount = 0;
//     std::atomic_size_t disconnectedClientCount = 0;
//     QSemaphore clientSocketsDisconnectedSemaphore;
//     QSemaphore serverSocketsConnectedSemaphore;
//     QSemaphore serverSocketsDisconnectedSemaphore;
//     std::unique_ptr<AsyncQObject<ServerTcpSockets, std::string_view, size_t, size_t>> server(new AsyncQObject<ServerTcpSockets, std::string_view, size_t, size_t>(serverAddress, totalConnections, requestsPerWorkingConnection));
//     const auto serverPort = server->get()->serverPort();
//     QObject::connect(server->get(), &ServerTcpSockets::connectedToClients, [&](){serverSocketsConnectedSemaphore.release();});
//     QObject::connect(server->get(), &ServerTcpSockets::disconnectedFromClients, [&](){serverSocketsDisconnectedSemaphore.release();});
//     std::vector<std::unique_ptr<AsyncQObject<ClientTcpSockets, std::string_view, uint16_t, std::string_view, size_t, size_t, size_t, int, int>>> clients(clientThreadCount);
//     size_t counter = 0;
//     for (auto &client : clients)
//     {
//         std::string currentBindAddress("127.25.2.");
//         currentBindAddress.append(std::to_string(++counter));
//         client.reset(new AsyncQObject<ClientTcpSockets, std::string_view, uint16_t, std::string_view, size_t, size_t, size_t, int, int>(serverAddress, serverPort, currentBindAddress, totalConnectionsPerThread, workingConnectionsPerThread, requestsPerWorkingConnection, a, b));
//     }
//     memoryConsumedAfterCreatingClientSockets = getUsedMemory();
//     QObject ctxObject;
//     for (auto &client : clients)
//     {
//         QObject::connect(client->get(), &ClientTcpSockets::connectedToServer, &ctxObject, [&]()
//         {
//             if (++connectedClientCount == clientThreadCount)
//             {
//                 REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverSocketsConnectedSemaphore, 10000));
//                 connectionsPerSecond = (1000.0 * totalConnections)/elapsedTimer.elapsed();
//                 memoryConsumedAfterConnecting = getUsedMemory();
//                 elapsedTimer.start();
//                 for (auto &client : clients)
//                     QMetaObject::invokeMethod(client->get(), "sendRequests", Qt::QueuedConnection);
//             }
//         });
//         QObject::connect(client->get(), &ClientTcpSockets::receivedResponses, &ctxObject, [&]()
//         {
//             if (++receivedResponseCount == clientThreadCount)
//             {
//                 requestsPerSecond = (1000.0 * clientThreadCount * workingConnectionsPerThread * requestsPerWorkingConnection)/elapsedTimer.elapsed();
//                 memoryConsumedAfterResponses = getUsedMemory();
//                 elapsedTimer.start();
//                 for (auto &client : clients)
//                     QMetaObject::invokeMethod(client->get(), "disconnectFromServer", Qt::QueuedConnection);
//             }
//         });
//         QObject::connect(client->get(), &ClientTcpSockets::disconnectedFromServer, &ctxObject, [&]()
//         {
//             if (++disconnectedClientCount == clientThreadCount)
//             {
//                 REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverSocketsDisconnectedSemaphore, 10000));
//                 disconnectionsPerSecond = (1000.0 * totalConnections)/elapsedTimer.elapsed();
//                 memoryConsumedAfterDisconnecting = getUsedMemory();
//                 clientSocketsDisconnectedSemaphore.release();
//             }
//         });
//     }
//     elapsedTimer.start();
//     for (auto &client : clients)
//         QMetaObject::invokeMethod(client->get(), "connectToServer", Qt::QueuedConnection);
//     REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientSocketsDisconnectedSemaphore, 10000));
//     WARN(QByteArray("Memory consumed after creating client sockets: ").append(QByteArray::number(memoryConsumedAfterCreatingClientSockets)));
//     WARN(QByteArray("Memory consumed after connecting: ").append(QByteArray::number(memoryConsumedAfterConnecting)));
//     WARN(QByteArray("Memory consumed after responses: ").append(QByteArray::number(memoryConsumedAfterResponses)));
//     WARN(QByteArray("Memory consumed after disconnecting: ").append(QByteArray::number(memoryConsumedAfterDisconnecting)));
//     WARN(QByteArray("Connections per second: ").append(QByteArray::number(connectionsPerSecond)));
//     WARN(QByteArray("Requests per second: ").append(QByteArray::number(requestsPerSecond)));
//     WARN(QByteArray("Disconnections per second: ").append(QByteArray::number(disconnectionsPerSecond)));
// }


namespace TcpSocketTests
{

class ClientQTcpSockets : public QObject
{
    Q_OBJECT
public:
    ClientQTcpSockets(std::string_view serverAddress,
                      uint16_t serverPort,
                      std::string_view bindAddress,
                      size_t totalConnections,
                      size_t workingConnections,
                      size_t requestsPerWorkingConnection,
                      int a,
                      int b) :
        m_serverAddress(QString::fromStdString(std::string(serverAddress))),
        m_serverPort(serverPort),
        m_bindAddress(QString::fromStdString(std::string(bindAddress))),
        m_totalConnections(totalConnections),
        m_workingConnections(workingConnections),
        m_requestsPerWorkingConnection(requestsPerWorkingConnection),
        m_a(a),
        m_b(b)
    {
        REQUIRE(!m_serverAddress.isNull()
                && !m_bindAddress.isNull()
                && m_serverPort > 0
                && m_totalConnections > 0
                && m_workingConnections > 0
                && (m_totalConnections >= m_workingConnections)
                && m_requestsPerWorkingConnection > 0);
        m_sockets.resize(m_totalConnections);
        for (auto *&pSocket : m_sockets)
            pSocket = new QTcpSocket;
    }
    ~ClientQTcpSockets() override = default;

public slots:
    void connectToServer() {connectToServerInternal();}
    void sendRequests()
    {
        for (auto i = 0; i < m_workingConnections; ++i)
        {
            auto *pSocket = m_sockets[i];
            for (auto k = 0; k < m_requestsPerWorkingConnection; ++k)
            {
                pSocket->write((char*)&m_a, sizeof(m_a));
                pSocket->write((char*)&m_b, sizeof(m_b));
            }
        }
    }
    void disconnectFromServer()
    {
        for (auto *pSocket : m_sockets)
            pSocket->disconnectFromHost();
    }

signals:
    void connectedToServer();
    void receivedResponses();
    void disconnectedFromServer();

private slots:
    void connectToServerInternal()
    {
        const auto upTo = qMin(m_totalConnections, m_connectionsPerBatch + m_currentConnectIndex);
        const auto startIndex = m_currentConnectIndex;
        for (auto i = startIndex; i < upTo; ++i)
        {
            ++m_currentConnectIndex;
            auto *pSocket = m_sockets[i];
            QObject::connect(pSocket, &QTcpSocket::connected, [this, pSocket]()
            {
                pSocket->setSocketOption(QAbstractSocket::SocketOption::LowDelayOption, 1);
                pSocket->setSocketOption(QAbstractSocket::SocketOption::KeepAliveOption, 1);
                if (++m_connectionCount == m_totalConnections)
                {
                    m_hasConnectedAllClients = true;
                    emit connectedToServer();
                }
                else if (++m_batchConnectionCount == m_connectionsPerBatch)
                {
                    m_batchConnectionCount = 0;
                    QMetaObject::invokeMethod(this, "connectToServerInternal", Qt::QueuedConnection);
                }
            });
            QObject::connect(pSocket, &QTcpSocket::readyRead, [this, pSocket]()
            {
                if (pSocket->bytesAvailable() != (m_requestsPerWorkingConnection * sizeof(int)))
                    return;
                else
                {
                    for (auto i = 0; i < m_requestsPerWorkingConnection; ++i)
                    {
                        int sum = 0;
                        pSocket->read((char*)&sum, sizeof(sum));
                        REQUIRE(sum == (m_a + m_b));
                    }
                    if (++m_responseCount == m_workingConnections)
                        emit receivedResponses();
                }
            });
            QObject::connect(pSocket, &QTcpSocket::disconnected, [this, pSocket]()
            {
                REQUIRE(m_hasConnectedAllClients);
                pSocket->deleteLater();
                if (++m_disconnectionCount == m_totalConnections)
                    emit disconnectedFromServer();
            });
            while (!pSocket->bind(m_bindAddress, ++m_currentBindPort)) {}
            QObject::connect(pSocket, &QTcpSocket::errorOccurred, [this, pSocket]()
            {
                REQUIRE(!m_hasConnectedAllClients);
                REQUIRE(pSocket->error() == QAbstractSocket::SocketError::AddressInUseError);
                // binding failed
                QMetaObject::invokeMethod(this, "reconnectSocket", Qt::QueuedConnection, pSocket);
            });
            pSocket->connectToHost(m_serverAddress, m_serverPort);
        }
    }

    void reconnectSocket(QTcpSocket *pSocket)
    {
        while (!pSocket->bind(m_bindAddress, ++m_currentBindPort)) {}
        pSocket->connectToHost(m_serverAddress, m_serverPort);
    }

private:
    size_t m_connectionCount = 0;
    size_t m_responseCount = 0;
    size_t m_disconnectionCount = 0;
    std::vector<QTcpSocket*> m_sockets;
    size_t m_currentConnectIndex = 0;
    size_t m_batchConnectionCount = 0;
    const size_t m_connectionsPerBatch = 250;
    QHostAddress m_serverAddress;
    QHostAddress m_bindAddress;
    uint16_t m_currentBindPort = 1024;
    const uint16_t m_serverPort;
    const size_t m_totalConnections;
    const size_t m_workingConnections;
    const size_t m_requestsPerWorkingConnection;
    const int m_a;
    const int m_b;
    bool m_hasConnectedAllClients = false;
};


class ServerQTcpSockets : public QObject
{
    Q_OBJECT
public:
    ServerQTcpSockets(std::string_view serverAddress,
                     size_t totalConnections,
                     size_t requestsPerWorkingConnection) :
        m_serverAddress(serverAddress),
        m_totalConnections(totalConnections),
        m_requestsPerWorkingConnection(requestsPerWorkingConnection)
    {
        REQUIRE(!m_serverAddress.empty()
                && m_totalConnections > 0);
        m_pTcpServer = new QTcpServer;
        m_pTcpServer->setListenBacklogSize(30000);
        m_pTcpServer->setMaxPendingConnections(30000);
        QObject::connect(m_pTcpServer, &QTcpServer::newConnection, [this]()
        {
            while (m_pTcpServer->hasPendingConnections())
            {
                auto *pSocket = m_pTcpServer->nextPendingConnection();
                REQUIRE(pSocket->state() == QAbstractSocket::ConnectedState);
                pSocket->setSocketOption(QAbstractSocket::SocketOption::LowDelayOption, 1);
                pSocket->setSocketOption(QAbstractSocket::SocketOption::KeepAliveOption, 1);
                QObject::connect(pSocket, &QTcpSocket::readyRead, [this, pSocket]()
                {
                    if (pSocket->bytesAvailable() != (2 * m_requestsPerWorkingConnection * sizeof(int)))
                        return;
                    else
                    {
                        for (auto i = 0; i < m_requestsPerWorkingConnection; ++i)
                        {
                            int a = 0;
                            pSocket->read((char*)&a, sizeof(a));
                            int b = 0;
                            pSocket->read((char*)&b, sizeof(b));
                            const int sum = a + b;
                            pSocket->write((char*)&sum, sizeof(sum));
                        }
                    }
                });
                QObject::connect(pSocket, &QTcpSocket::disconnected, [this, pSocket]()
                {
                    REQUIRE(m_hasConnectedToAllClients);
                    pSocket->deleteLater();
                    if (++m_disconnectionCount == m_totalConnections)
                    {
                        m_pTcpServer->deleteLater();
                        m_pTcpServer = nullptr;
                        emit disconnectedFromClients();
                    }
                });
                QObject::connect(pSocket, &QTcpSocket::errorOccurred, [this, pSocket]()
                {
                    REQUIRE(m_hasConnectedToAllClients);
                    REQUIRE(pSocket->error() == QAbstractSocket::SocketError::RemoteHostClosedError);
                });
                if (++m_connectionCount == m_totalConnections)
                {
                    m_pTcpServer->close();
                    m_hasConnectedToAllClients = true;
                    emit connectedToClients();
                }
            }
        });
        REQUIRE(m_pTcpServer->listen(QHostAddress(QString::fromStdString(std::string(m_serverAddress)))));
        m_serverPort = m_pTcpServer->serverPort();
        REQUIRE(m_serverPort > 0);
    }
    ~ServerQTcpSockets() override = default;
    uint16_t serverPort() const {return m_serverPort;}

signals:
    void connectedToClients();
    void disconnectedFromClients();

private:
    QTcpServer *m_pTcpServer = nullptr;
    size_t m_connectionCount = 0;
    size_t m_disconnectionCount = 0;
    std::string_view m_serverAddress;
    uint16_t m_serverPort = 0;
    const size_t m_totalConnections;
    const size_t m_requestsPerWorkingConnection;
    bool m_hasConnectedToAllClients = false;
};

}


// SCENARIO("QTcpSocket benchmarks")
// {
//     static constexpr std::string_view serverAddress("127.25.24.25");
//     static constexpr size_t totalConnectionsPerThread = 15000;
//     static constexpr size_t workingConnectionsPerThread = 10000;
//     static constexpr size_t clientThreadCount = 5;
//     static constexpr size_t totalConnections = totalConnectionsPerThread * clientThreadCount;
//     static constexpr size_t requestsPerWorkingConnection = 10;
//     static constexpr int a = 5;
//     static constexpr int b = 3;
//     size_t memoryConsumedAfterCreatingClientSockets = 0;
//     size_t memoryConsumedAfterConnecting = 0;
//     size_t memoryConsumedAfterResponses = 0;
//     size_t memoryConsumedAfterDisconnecting = 0;
//     QElapsedTimer elapsedTimer;
//     double connectionsPerSecond = 0;
//     double requestsPerSecond = 0;
//     double disconnectionsPerSecond = 0;
//     std::atomic_size_t connectedClientCount = 0;
//     std::atomic_size_t receivedResponseCount = 0;
//     std::atomic_size_t disconnectedClientCount = 0;
//     QSemaphore clientSocketsDisconnectedSemaphore;
//     QSemaphore serverSocketsConnectedSemaphore;
//     QSemaphore serverSocketsDisconnectedSemaphore;
//     std::unique_ptr<AsyncQObject<ServerQTcpSockets, std::string_view, size_t, size_t>> server(new AsyncQObject<ServerQTcpSockets, std::string_view, size_t, size_t>(serverAddress, totalConnections, requestsPerWorkingConnection));
//     const auto serverPort = server->get()->serverPort();
//     QObject::connect(server->get(), &ServerQTcpSockets::connectedToClients, [&](){serverSocketsConnectedSemaphore.release();});
//     QObject::connect(server->get(), &ServerQTcpSockets::disconnectedFromClients, [&](){serverSocketsDisconnectedSemaphore.release();});
//     std::vector<std::unique_ptr<AsyncQObject<ClientQTcpSockets, std::string_view, uint16_t, std::string_view, size_t, size_t, size_t, int, int>>> clients(clientThreadCount);
//     size_t counter = 0;
//     for (auto &client : clients)
//     {
//         std::string currentBindAddress("127.35.21.");
//         currentBindAddress.append(std::to_string(++counter));
//         client.reset(new AsyncQObject<ClientQTcpSockets, std::string_view, uint16_t, std::string_view, size_t, size_t, size_t, int, int>(serverAddress, serverPort, currentBindAddress, totalConnectionsPerThread, workingConnectionsPerThread, requestsPerWorkingConnection, a, b));
//     }
//     memoryConsumedAfterCreatingClientSockets = getUsedMemory();
//     QObject ctxObject;
//     for (auto &client : clients)
//     {
//         QObject::connect(client->get(), &ClientQTcpSockets::connectedToServer, &ctxObject, [&]()
//         {
//             if (++connectedClientCount == clientThreadCount)
//             {
//                 REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverSocketsConnectedSemaphore, 10000));
//                 connectionsPerSecond = (1000.0 * totalConnections)/elapsedTimer.elapsed();
//                 memoryConsumedAfterConnecting = getUsedMemory();
//                 elapsedTimer.start();
//                 for (auto &client : clients)
//                     QMetaObject::invokeMethod(client->get(), "sendRequests", Qt::QueuedConnection);
//             }
//         });
//         QObject::connect(client->get(), &ClientQTcpSockets::receivedResponses, &ctxObject, [&]()
//         {
//             if (++receivedResponseCount == clientThreadCount)
//             {
//                 requestsPerSecond = (1000.0 * clientThreadCount * workingConnectionsPerThread * requestsPerWorkingConnection)/elapsedTimer.elapsed();
//                 memoryConsumedAfterResponses = getUsedMemory();
//                 elapsedTimer.start();
//                 for (auto &client : clients)
//                     QMetaObject::invokeMethod(client->get(), "disconnectFromServer", Qt::QueuedConnection);
//             }
//         });
//         QObject::connect(client->get(), &ClientQTcpSockets::disconnectedFromServer, &ctxObject, [&]()
//         {
//             if (++disconnectedClientCount == clientThreadCount)
//             {
//                 REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverSocketsDisconnectedSemaphore, 10000));
//                 disconnectionsPerSecond = (1000.0 * totalConnections)/elapsedTimer.elapsed();
//                 memoryConsumedAfterDisconnecting = getUsedMemory();
//                 clientSocketsDisconnectedSemaphore.release();
//             }
//         });
//     }
//     elapsedTimer.start();
//     for (auto &client : clients)
//         QMetaObject::invokeMethod(client->get(), "connectToServer", Qt::QueuedConnection);
//     REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientSocketsDisconnectedSemaphore, 10000));
//     WARN(QByteArray("Memory consumed after creating client sockets: ").append(QByteArray::number(memoryConsumedAfterCreatingClientSockets)));
//     WARN(QByteArray("Memory consumed after connecting: ").append(QByteArray::number(memoryConsumedAfterConnecting)));
//     WARN(QByteArray("Memory consumed after responses: ").append(QByteArray::number(memoryConsumedAfterResponses)));
//     WARN(QByteArray("Memory consumed after disconnecting: ").append(QByteArray::number(memoryConsumedAfterDisconnecting)));
//     WARN(QByteArray("Connections per second: ").append(QByteArray::number(connectionsPerSecond)));
//     WARN(QByteArray("Requests per second: ").append(QByteArray::number(requestsPerSecond)));
//     WARN(QByteArray("Disconnections per second: ").append(QByteArray::number(disconnectionsPerSecond)));
// }

#include "TcpSocket.spec.moc"
