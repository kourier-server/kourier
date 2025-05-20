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
#include <QTcpSocket>
#include <QTcpServer>
#include <QSemaphore>
#include <QFile>
#include <QThread>
#include <QElapsedTimer>
#include <QRandomGenerator64>
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


SCENARIO("TcpSocket interacts with client peer")
{
    GIVEN("a listening server")
    {
        TcpServer server;
        QSemaphore socketConnectedSemaphore;
        QSemaphore socketFailedSemaphore;
        QSemaphore socketDisconnectedSemaphore;
        QSemaphore socketReceivedDataFromPeerSemaphore;
        QByteArray socketReceivedData;
        std::unique_ptr<TcpSocket> pSocket;
        Object::connect(&server, &TcpServer::newConnection, [&](TcpSocket *pNewSocket)
        {
            pSocket.reset(pNewSocket);
            Object::connect(pSocket.get(), &TcpSocket::error, [&]() {socketFailedSemaphore.release();});
            Object::connect(pSocket.get(), &TcpSocket::disconnected, [&socketDisconnectedSemaphore](){socketDisconnectedSemaphore.release();});
            Object::connect(pSocket.get(), &TcpSocket::receivedData, [&]()
            {
                QByteArray readData;
                readData.resize(pSocket->dataAvailable());
                pSocket->read(readData.data(), readData.size());
                socketReceivedData.append(readData);
                socketReceivedDataFromPeerSemaphore.release();
            });
            socketConnectedSemaphore.release();
        });
        const auto serverAddress = GENERATE(AS(QHostAddress),
                                            QHostAddress("127.10.20.50"),
                                            QHostAddress("::1"));
        REQUIRE(server.listen(serverAddress, 0));
        const auto serverPort = server.serverPort();
        REQUIRE(serverPort >= 1024);

        WHEN("peer connects to host")
        {
            QSemaphore peerConnectedSemaphore;
            QSemaphore peerFailedSemaphore;
            QSemaphore peerDisconnectedSemaphore;
            QSemaphore peerReceivedDataFromTcpSocketSemaphore;
            QByteArray peerReceivedData;
            std::unique_ptr<QTcpSocket> pPeerSocket(new QTcpSocket);
            QObject::connect(pPeerSocket.get(), &QTcpSocket::errorOccurred, [&](QAbstractSocket::SocketError error) {peerFailedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QTcpSocket::connected, [&peerConnectedSemaphore](){peerConnectedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QTcpSocket::disconnected, [&peerDisconnectedSemaphore](){peerDisconnectedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QTcpSocket::readyRead, [&]()
            {
                peerReceivedData.append(pPeerSocket->readAll());
                peerReceivedDataFromTcpSocketSemaphore.release();
            });
            pPeerSocket->connectToHost(serverAddress, serverPort);

            THEN("server emits newConnection with a connected socket")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                REQUIRE(pSocket->state() == TcpSocket::State::Connected);

                AND_THEN("connecting peer socket emits connected")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                    REQUIRE(pPeerSocket->localAddress() == QHostAddress(QString::fromStdString(std::string(pSocket->peerAddress()))));
                    REQUIRE(pPeerSocket->localPort() == pSocket->peerPort());
                    REQUIRE(pPeerSocket->peerAddress() == QHostAddress(QString::fromStdString(std::string(pSocket->localAddress()))));
                    REQUIRE(pPeerSocket->peerPort() == pSocket->localPort());

                    AND_THEN("socket is constructed with LowDelay option set")
                    {
                        const auto defaultLowDelayOption = pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay);
                        REQUIRE(defaultLowDelayOption == 1);

                        AND_WHEN("LowDelay option is set to 0")
                        {
                            pSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, 0);

                            THEN("LowDelay option becomes unset")
                            {
                                const auto setLowDelayOption = pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay);
                                REQUIRE(setLowDelayOption == 0);

                                AND_WHEN("LowDelay option is enabled again")
                                {
                                    pSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, 1);

                                    THEN("LowDelay option becomes set")
                                    {
                                        const auto setLowDelayOption = pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay);
                                        REQUIRE(setLowDelayOption == 1);
                                    }
                                }

                                AND_WHEN("LowDelay option is set to a value other than 0 or 1")
                                {
                                    const auto value = GENERATE(AS(int), -3, 2, 5, 117);
                                    pSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, value);

                                    THEN("LowDelay option is set")
                                    {
                                        const auto setLowDelayOption = pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay);
                                        REQUIRE(setLowDelayOption == 1);
                                    }
                                }
                            }
                        }

                        AND_WHEN("LowDelay option is set to 1")
                        {
                            pSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, 1);

                            THEN("LowDelay option stays set")
                            {
                                const auto setLowDelayOption = pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay);
                                REQUIRE(setLowDelayOption == 1);
                            }
                        }

                        AND_WHEN("LowDelay option is set to a value other than 0 or 1")
                        {
                            const auto value = GENERATE(AS(int), -3, 2, 5, 117);
                            pSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, value);

                            THEN("LowDelay option stays set")
                            {
                                const auto setLowDelayOption = pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay);
                                REQUIRE(setLowDelayOption == 1);
                            }
                        }
                    }

                    AND_THEN("socket is constructed with KeepAlive option unset")
                    {
                        const auto defaultKeepAliveOption = pSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive);
                        REQUIRE(defaultKeepAliveOption == 0);

                        AND_WHEN("KeepAlive option is set to 1")
                        {
                            pSocket->setSocketOption(TcpSocket::SocketOption::KeepAlive, 1);

                            THEN("KeepAlive option becomes set")
                            {
                                const auto setKeepAliveOption = pSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive);
                                REQUIRE(setKeepAliveOption == 1);

                                AND_WHEN("KeepAlive option is set to 0")
                                {
                                    pSocket->setSocketOption(TcpSocket::SocketOption::KeepAlive, 0);

                                    THEN("KeepAlive option becomes unset")
                                    {
                                        const auto setKeepAliveOption = pSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive);
                                        REQUIRE(setKeepAliveOption == 0);
                                    }
                                }
                            }
                        }

                        AND_WHEN("KeepAlive option is set to 0")
                        {
                            pSocket->setSocketOption(TcpSocket::SocketOption::KeepAlive, 0);

                            THEN("KeepAlive option stays unset")
                            {
                                const auto setKeepAliveOption = pSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive);
                                REQUIRE(setKeepAliveOption == 0);
                            }
                        }

                        AND_WHEN("KeepAlive option is set to a value other than 0 or 1")
                        {
                            const auto value = GENERATE(AS(int), -3, 2, 5, 117);
                            pSocket->setSocketOption(TcpSocket::SocketOption::KeepAlive, value);

                            THEN("KeepAlive option is set")
                            {
                                const auto setKeepAliveOption = pSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive);
                                REQUIRE(setKeepAliveOption == 1);
                            }
                        }
                    }

                    AND_WHEN("SendBufferSize option is set to a valid value")
                    {
                        const auto value = GENERATE(AS(int),
                                                    wMemLimits.minValue,
                                                    wMemLimits.minValue + 18,
                                                    wMemLimits.minValue + 1024,
                                                    wMemLimits.defaultValue/2,
                                                    wMemLimits.defaultValue);
                        pSocket->setSocketOption(TcpSocket::SocketOption::SendBufferSize, value);

                        THEN("SendBufferSize option value is set to the double of the valid value")
                        {
                            const auto setOption = pSocket->getSocketOption(TcpSocket::SocketOption::SendBufferSize);
                            REQUIRE(setOption == (2 * value));
                        }
                    }

                    AND_WHEN("ReceiveBufferSize option is set to a valid value")
                    {
                        const auto value = GENERATE(AS(int),
                                                    rMemLimits.minValue,
                                                    rMemLimits.minValue + 18,
                                                    rMemLimits.minValue + 1024,
                                                    rMemLimits.defaultValue/2,
                                                    rMemLimits.defaultValue);
                        pSocket->setSocketOption(TcpSocket::SocketOption::ReceiveBufferSize, value);

                        THEN("ReceiveBufferSize option value is set to the double of the valid value")
                        {
                            const auto setOption = pSocket->getSocketOption(TcpSocket::SocketOption::ReceiveBufferSize);
                            REQUIRE(setOption == (2 * value));
                        }
                    }

                    THEN("connected peers can start exchanging data")
                    {
                        const auto dataToSend = GENERATE(AS(QByteArray),
                                                         QByteArray("a"),
                                                         QByteArray("abcdefgh"),
                                                         largeData);
                        const auto disableLowDelayOption = GENERATE(AS(bool), true, false);
                        const auto setKeepAliveOption = GENERATE(AS(bool), true, false);
                        const auto readBufferCapacity = GENERATE(AS(size_t), 0, 1024, 16384, 65536);
                        if (readBufferCapacity > 0)
                            pSocket->setReadBufferCapacity(readBufferCapacity);
                        if (disableLowDelayOption)
                            pSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, 0);
                        REQUIRE(pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay) == disableLowDelayOption ? 0 : 1);
                        if (setKeepAliveOption)
                        {
                            pPeerSocket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
                            pSocket->setSocketOption(TcpSocket::SocketOption::KeepAlive, 1);
                        }
                        REQUIRE(pPeerSocket->socketOption(QAbstractSocket::KeepAliveOption) == setKeepAliveOption ? 1 : 0);
                        REQUIRE(pSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive) == setKeepAliveOption ? 1 : 0);

                        AND_WHEN("peer sends data to TcpSocket")
                        {
                            pPeerSocket->write(dataToSend);

                            THEN("TcpSocket receives sent data")
                            {
                                const auto &sentData = socketReceivedData;
                                while (sentData != dataToSend)
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketReceivedDataFromPeerSemaphore, 1));
                                }

                                AND_WHEN("peer sends some more data to TcpSocket")
                                {
                                    socketReceivedData.clear();
                                    const QByteArray someMoreData("0123456789");
                                    pPeerSocket->write(someMoreData);

                                    THEN("TcpSocket receives sent data")
                                    {
                                        while (sentData != someMoreData)
                                        {
                                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketReceivedDataFromPeerSemaphore, 1));
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("peer closes connection after sending data to TcpSocket")
                        {
                            pPeerSocket->write(dataToSend);
                            pPeerSocket->disconnectFromHost();

                            THEN("TcpSocket receives sent data")
                            {
                                const auto &sentData = socketReceivedData;
                                while (sentData != dataToSend)
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketReceivedDataFromPeerSemaphore, 1));
                                }

                                AND_THEN("both peer and TcpSocket emit disconnected")
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                                    REQUIRE(pPeerSocket->error() == QAbstractSocket::UnknownSocketError);
                                    REQUIRE(pSocket->errorMessage().empty());

                                    AND_WHEN("peer is deleted")
                                    {
                                        while (peerFailedSemaphore.tryAcquire()) {}
                                        pPeerSocket.reset(nullptr);

                                        THEN("peer does not emit any error")
                                        {
                                            REQUIRE(!peerFailedSemaphore.tryAcquire());
                                            REQUIRE(pSocket->errorMessage().empty());
                                        }
                                    }

                                    AND_WHEN("TcpSocket is deleted")
                                    {
                                        while (socketFailedSemaphore.tryAcquire()) {}
                                        pSocket.reset(nullptr);

                                        THEN("neither peer or TcpSocket emit any error")
                                        {
                                            REQUIRE(!peerFailedSemaphore.tryAcquire());
                                            REQUIRE(!socketFailedSemaphore.tryAcquire());
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("peer aborts after writing data")
                        {
                            pPeerSocket->write(dataToSend);
                            pPeerSocket->abort();

                            THEN("both peer and TcpSocket emit disconnected")
                            {
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                                REQUIRE(pSocket->errorMessage().empty());
                            }
                        }

                        AND_WHEN("peer is deleted after writing data")
                        {
                            pPeerSocket->write(dataToSend);
                            pPeerSocket.reset(nullptr);

                            THEN("both peer and TcpSocket emit disconnected")
                            {
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                                REQUIRE(pSocket->errorMessage().empty());
                            }
                        }

                        AND_WHEN("TcpSocket sends data to peer")
                        {
                            pSocket->write(dataToSend.data(), dataToSend.size());

                            THEN("peer receives sent data")
                            {
                                const auto &sentData = peerReceivedData;
                                while (sentData != dataToSend)
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerReceivedDataFromTcpSocketSemaphore, 1));
                                }

                                AND_WHEN("TcpSocket sends some more data to peer")
                                {
                                    peerReceivedData.clear();
                                    const QByteArray someMoreData("0123456789");
                                    pSocket->write(someMoreData.data(), someMoreData.size());

                                    THEN("peer receives sent data")
                                    {
                                        while (sentData != someMoreData)
                                        {
                                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerReceivedDataFromTcpSocketSemaphore, 1));
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("TcpSocket closes connection after sending data to peer")
                        {
                            pSocket->write(dataToSend.data(), dataToSend.size());
                            pSocket->disconnectFromPeer();

                            THEN("peer receives sent data")
                            {
                                QByteArray sentData;
                                while (sentData != dataToSend)
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerReceivedDataFromTcpSocketSemaphore, 1));
                                    sentData = peerReceivedData;
                                }

                                AND_THEN("TcpSocket emits disconnected and peer emits RemoteHostClosedError before emiting disconnected")
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                                    REQUIRE(pPeerSocket->error() == QAbstractSocket::RemoteHostClosedError);
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                                    REQUIRE(pSocket->errorMessage().empty());

                                    AND_WHEN("TcpSocket is deleted")
                                    {
                                        while (socketFailedSemaphore.tryAcquire()) {}
                                        pSocket.reset(nullptr);

                                        THEN("TcpSocket does not emit any error")
                                        {
                                            REQUIRE(pPeerSocket->error() == QAbstractSocket::RemoteHostClosedError);
                                            REQUIRE(!socketFailedSemaphore.tryAcquire());
                                        }
                                    }

                                    AND_WHEN("Peer is deleted")
                                    {
                                        while (peerFailedSemaphore.tryAcquire()) {}
                                        pPeerSocket.reset(nullptr);

                                        THEN("neither peer or TcpSocket emit any error")
                                        {
                                            REQUIRE(!peerFailedSemaphore.tryAcquire());
                                            REQUIRE(!socketFailedSemaphore.tryAcquire());
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("TcpSocket aborts after writing data")
                        {
                            pSocket->write(dataToSend.data(), dataToSend.size());
                            pSocket->abort();

                            THEN("TcpSocket aborts and Peer emits RemoteHostClosedError before emiting disconnected")
                            {
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                                REQUIRE(pPeerSocket->error() == QAbstractSocket::RemoteHostClosedError);
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                                REQUIRE(pSocket->errorMessage().empty());
                            }
                        }

                        AND_WHEN("TcpSocket is deleted after writing data")
                        {
                            pSocket->write(dataToSend.data(), dataToSend.size());
                            pSocket.reset(nullptr);

                            THEN("TcpSocket aborts and Peer emits RemoteHostClosedError before emiting disconnected")
                            {
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                                REQUIRE(pPeerSocket->error() == QAbstractSocket::RemoteHostClosedError);
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            }
                        }
                    }

                    AND_WHEN("peer disconnects from TcpSocket")
                    {
                        pPeerSocket->disconnectFromHost();

                        THEN("peer emits disconnected and TcpSocket emits disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                            REQUIRE(pPeerSocket->error() == QAbstractSocket::UnknownSocketError);
                            REQUIRE(pSocket->errorMessage().empty());

                            AND_WHEN("peer is deleted")
                            {
                                while (peerFailedSemaphore.tryAcquire()) {}
                                pPeerSocket.reset(nullptr);

                                THEN("peer does not emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(pSocket->errorMessage().empty());
                                }
                            }

                            AND_WHEN("TcpSocket is deleted")
                            {
                                while (socketFailedSemaphore.tryAcquire()) {}
                                pSocket.reset(nullptr);

                                THEN("neither peer or TcpSocket emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }
                        }
                    }

                    AND_WHEN("peer aborts connection")
                    {
                        pPeerSocket->abort();

                        THEN("both peer and TcpSocket emit disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                            REQUIRE(pPeerSocket->error() == QAbstractSocket::UnknownSocketError);
                            REQUIRE(pSocket->errorMessage().empty());

                            AND_WHEN("peer is deleted")
                            {
                                while (peerFailedSemaphore.tryAcquire()) {}
                                pPeerSocket.reset(nullptr);

                                THEN("peer does not emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(pSocket->errorMessage().empty());
                                }
                            }

                            AND_WHEN("TcpSocket is deleted")
                            {
                                while (socketFailedSemaphore.tryAcquire()) {}
                                pSocket.reset(nullptr);

                                THEN("neither peer or TcpSocket emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }
                        }
                    }

                    AND_WHEN("TcpSocket disconnects from TcpSocket")
                    {
                        pSocket->disconnectFromPeer();

                        THEN("TcpSocket emits disconnected and peer emits RemoteHostClosedError before emiting disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                            REQUIRE(pPeerSocket->error() == QAbstractSocket::RemoteHostClosedError);
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(pSocket->errorMessage().empty());

                            AND_WHEN("peer is deleted")
                            {
                                while (peerFailedSemaphore.tryAcquire()) {}
                                pPeerSocket.reset(nullptr);

                                THEN("neither peer or TcpSocket emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }

                            AND_WHEN("TcpSocket is deleted")
                            {
                                pSocket.reset(nullptr);

                                THEN("TcpSocket does not emit any error")
                                {
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                    REQUIRE(pPeerSocket->error() == QAbstractSocket::RemoteHostClosedError);
                                }
                            }
                        }
                    }

                    AND_WHEN("TcpSocket aborts connection")
                    {
                        pSocket->abort();

                        THEN("Peer emits RemoteHostClosedError before emiting disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                            REQUIRE(pPeerSocket->error() == QAbstractSocket::RemoteHostClosedError);
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(pSocket->errorMessage().empty());

                            AND_WHEN("peer is deleted")
                            {
                                while (peerFailedSemaphore.tryAcquire()) {}
                                pPeerSocket.reset(nullptr);

                                THEN("neither peer or TcpSocket emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }

                            AND_WHEN("TcpSocket is deleted")
                            {
                                pSocket.reset(nullptr);

                                THEN("TcpSocket does not emit any error")
                                {
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                    REQUIRE(pPeerSocket->error() == QAbstractSocket::RemoteHostClosedError);
                                }
                            }
                        }
                    }

                    AND_WHEN("both peer and TcpSocket disconnects")
                    {
                        pPeerSocket->disconnectFromHost();
                        pSocket->disconnectFromPeer();

                        THEN("peer emits disconnected and TcpSocket emits disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                            REQUIRE(pSocket->errorMessage().empty());

                            AND_WHEN("peer is deleted")
                            {
                                while (peerFailedSemaphore.tryAcquire()) {}
                                pPeerSocket.reset(nullptr);

                                THEN("TcpSocket not emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(pSocket->errorMessage().empty());
                                }
                            }

                            AND_WHEN("TcpSocket is deleted")
                            {
                                while (socketFailedSemaphore.tryAcquire()) {}
                                pSocket.reset(nullptr);

                                THEN("TcpSocket does not emit any error")
                                {
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }
                        }
                    }

                    AND_WHEN("both TcpSocket and peer disconnects")
                    {
                        pSocket->disconnectFromPeer();
                        pPeerSocket->disconnectFromHost();

                        THEN("peer emits disconnected and TcpSocket emits disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                            REQUIRE(pSocket->errorMessage().empty());

                            AND_WHEN("peer is deleted")
                            {
                                while (peerFailedSemaphore.tryAcquire()) {}
                                pPeerSocket.reset(nullptr);

                                THEN("TcpSocket not emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(pSocket->errorMessage().empty());
                                }
                            }

                            AND_WHEN("TcpSocket is deleted")
                            {
                                while (socketFailedSemaphore.tryAcquire()) {}
                                pSocket.reset(nullptr);

                                THEN("TcpSocket does not emit any error")
                                {
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }
                        }
                    }

                    AND_WHEN("peer is deleted")
                    {
                        while (peerFailedSemaphore.tryAcquire()) {}
                        pPeerSocket.reset(nullptr);

                        THEN("peer does not emit any error and TcpSocket emits disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                            REQUIRE(pSocket->errorMessage().empty());
                            REQUIRE(!peerFailedSemaphore.tryAcquire());
                        }
                    }

                    AND_WHEN("TcpSocket is deleted")
                    {
                        while (socketFailedSemaphore.tryAcquire()) {}
                        pSocket.reset(nullptr);
                        QCoreApplication::processEvents();

                        THEN("peer socket emits error and disconnected signal")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(pPeerSocket->error() == QAbstractSocket::SocketError::RemoteHostClosedError);
                            REQUIRE(!socketFailedSemaphore.tryAcquire());
                        }
                    }
                }
            }
        }
    }
}


SCENARIO("TcpSocket interacts with server peer by address")
{
    GIVEN("a listening server")
    {
        QTcpServer server;
        QSemaphore peerConnectedSemaphore;
        QSemaphore peerFailedSemaphore;
        QSemaphore peerDisconnectedSemaphore;
        QSemaphore peerReceivedDataFromTcpSocketSemaphore;
        QByteArray peerReceivedData;
        std::unique_ptr<QTcpSocket> pPeerSocket;
        QObject::connect(&server, &QTcpServer::newConnection, [&]()
        {
            REQUIRE(server.hasPendingConnections());
            REQUIRE(!pPeerSocket);
            pPeerSocket.reset(server.nextPendingConnection());
            REQUIRE(pPeerSocket.get() != nullptr);
            pPeerSocket->setParent(nullptr);
            REQUIRE(!server.hasPendingConnections());
            QObject::connect(pPeerSocket.get(), &QTcpSocket::errorOccurred, [&](QAbstractSocket::SocketError error) {peerFailedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QTcpSocket::disconnected, [&](){peerDisconnectedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QTcpSocket::readyRead, [&]()
            {
                peerReceivedData.append(pPeerSocket->readAll());
                peerReceivedDataFromTcpSocketSemaphore.release();
            });
            peerConnectedSemaphore.release();
        });
        const auto serverAndBindAddresses = GENERATE(AS(std::pair<QHostAddress, std::pair<QHostAddress, bool>>),
                                                     {QHostAddress("127.10.10.25"), {QHostAddress::Any, false}},
                                                     {QHostAddress("127.10.10.25"), {QHostAddress("127.100.200.117"), false}},
                                                     {QHostAddress("127.10.10.25"), {QHostAddress("127.100.200.118"), true}},
                                                     {QHostAddress("::1"), {QHostAddress::Any, false}},
                                                     {QHostAddress("::1"), {QHostAddress("::1"), false}},
                                                     {QHostAddress("::1"), {QHostAddress("::1"), true}},
                                                     {QHostAddress::Any, {QHostAddress("127.110.220.123"), false}},
                                                     {QHostAddress::Any, {QHostAddress("127.110.220.125"), true}});
        const auto serverAddress = serverAndBindAddresses.first;
        REQUIRE(server.listen(serverAddress, 0));
        const auto serverPort = server.serverPort();
        REQUIRE(serverPort >= 1024);

        WHEN("TcpSocket connects to server")
        {
            QSemaphore socketConnectedSemaphore;
            QSemaphore socketFailedSemaphore;
            QSemaphore socketDisconnectedSemaphore;
            QSemaphore socketReceivedDataFromPeerSemaphore;
            QByteArray socketReceivedData;
            std::unique_ptr<TcpSocket> pSocket(new TcpSocket);
            Object::connect(pSocket.get(), &TcpSocket::error, [&]() {socketFailedSemaphore.release();});
            Object::connect(pSocket.get(), &TcpSocket::connected, [&](){socketConnectedSemaphore.release();});
            Object::connect(pSocket.get(), &TcpSocket::disconnected, [&](){socketDisconnectedSemaphore.release();});
            Object::connect(pSocket.get(), &TcpSocket::receivedData, [&]()
            {
                QByteArray readData;
                readData.resize(pSocket->dataAvailable());
                pSocket->read(readData.data(), readData.size());
                socketReceivedData.append(readData);
                socketReceivedDataFromPeerSemaphore.release();
            });
            if (serverAndBindAddresses.second.first != QHostAddress::Any)
            {
                if (serverAndBindAddresses.second.second)
                {
                    QTcpSocket socket;
                    socket.bind(serverAndBindAddresses.second.first);
                    const auto availableBindPort = socket.localPort();
                    socket.abort();
                    REQUIRE(availableBindPort > 0 && availableBindPort <= 65535);
                    pSocket->setBindAddressAndPort(serverAndBindAddresses.second.first.toString().toStdString(), availableBindPort);
                }
                else
                    pSocket->setBindAddressAndPort(serverAndBindAddresses.second.first.toString().toStdString());
            }
            pSocket->connect(serverAddress.toString().toStdString(), serverPort);

            THEN("peer emits newConnection with a connected socket")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                REQUIRE(pPeerSocket->state() == QAbstractSocket::ConnectedState);

                AND_THEN("TcpSocket emits connected")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                    if (serverAndBindAddresses.first.protocol() == serverAndBindAddresses.second.first.protocol())
                    {
                        REQUIRE(pPeerSocket->localAddress().toString().toStdString() == pSocket->peerAddress());
                        REQUIRE(pPeerSocket->localPort() == pSocket->peerPort());
                        REQUIRE(pPeerSocket->peerAddress().toString().toStdString() == pSocket->localAddress());
                        REQUIRE(pPeerSocket->peerPort() == pSocket->localPort());
                    }
                    else
                    {
                        REQUIRE(pPeerSocket->localAddress().isEqual(QHostAddress(QString::fromStdString(std::string(pSocket->peerAddress()))), QHostAddress::ConvertV4MappedToIPv4));
                        REQUIRE(pPeerSocket->localPort() == pSocket->peerPort());
                        REQUIRE(pPeerSocket->peerAddress().isEqual(QHostAddress(QString::fromStdString(std::string(pSocket->localAddress()))), QHostAddress::ConvertV4MappedToIPv4));
                        REQUIRE(pPeerSocket->peerPort() == pSocket->localPort());
                    }

                    AND_THEN("socket is constructed with LowDelay option set")
                    {
                        const auto defaultLowDelayOption = pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay);
                        REQUIRE(defaultLowDelayOption == 1);

                        AND_WHEN("LowDelay option is set to 0")
                        {
                            pSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, 0);

                            THEN("LowDelay option becomes unset")
                            {
                                const auto setLowDelayOption = pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay);
                                REQUIRE(setLowDelayOption == 0);

                                AND_WHEN("LowDelay option is enabled again")
                                {
                                    pSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, 1);

                                    THEN("LowDelay option becomes set")
                                    {
                                        const auto setLowDelayOption = pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay);
                                        REQUIRE(setLowDelayOption == 1);
                                    }
                                }

                                AND_WHEN("LowDelay option is set to a value other than 0 or 1")
                                {
                                    const auto value = GENERATE(AS(int), -3, 2, 5, 117);
                                    pSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, value);

                                    THEN("LowDelay option is set")
                                    {
                                        const auto setLowDelayOption = pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay);
                                        REQUIRE(setLowDelayOption == 1);
                                    }
                                }
                            }
                        }

                        AND_WHEN("LowDelay option is set to 1")
                        {
                            pSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, 1);

                            THEN("LowDelay option stays set")
                            {
                                const auto setLowDelayOption = pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay);
                                REQUIRE(setLowDelayOption == 1);
                            }
                        }

                        AND_WHEN("LowDelay option is set to a value other than 0 or 1")
                        {
                            const auto value = GENERATE(AS(int), -3, 2, 5, 117);
                            pSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, value);

                            THEN("LowDelay option stays set")
                            {
                                const auto setLowDelayOption = pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay);
                                REQUIRE(setLowDelayOption == 1);
                            }
                        }
                    }

                    AND_THEN("socket is constructed with KeepAlive option unset")
                    {
                        const auto defaultKeepAliveOption = pSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive);
                        REQUIRE(defaultKeepAliveOption == 0);

                        AND_WHEN("KeepAlive option is set to 1")
                        {
                            pSocket->setSocketOption(TcpSocket::SocketOption::KeepAlive, 1);

                            THEN("KeepAlive option becomes set")
                            {
                                const auto setKeepAliveOption = pSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive);
                                REQUIRE(setKeepAliveOption == 1);

                                AND_WHEN("KeepAlive option is set to 0")
                                {
                                    pSocket->setSocketOption(TcpSocket::SocketOption::KeepAlive, 0);

                                    THEN("KeepAlive option becomes unset")
                                    {
                                        const auto setKeepAliveOption = pSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive);
                                        REQUIRE(setKeepAliveOption == 0);
                                    }
                                }
                            }
                        }

                        AND_WHEN("KeepAlive option is set to 0")
                        {
                            pSocket->setSocketOption(TcpSocket::SocketOption::KeepAlive, 0);

                            THEN("KeepAlive option stays unset")
                            {
                                const auto setKeepAliveOption = pSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive);
                                REQUIRE(setKeepAliveOption == 0);
                            }
                        }

                        AND_WHEN("KeepAlive option is set to a value other than 0 or 1")
                        {
                            const auto value = GENERATE(AS(int), -3, 2, 5, 117);
                            pSocket->setSocketOption(TcpSocket::SocketOption::KeepAlive, value);

                            THEN("KeepAlive option is set")
                            {
                                const auto setKeepAliveOption = pSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive);
                                REQUIRE(setKeepAliveOption == 1);
                            }
                        }
                    }

                    AND_WHEN("SendBufferSize option is set to a valid value")
                    {
                        const auto value = GENERATE(AS(int),
                                                    wMemLimits.minValue,
                                                    wMemLimits.minValue + 18,
                                                    wMemLimits.minValue + 1024,
                                                    wMemLimits.defaultValue/2,
                                                    wMemLimits.defaultValue);
                        pSocket->setSocketOption(TcpSocket::SocketOption::SendBufferSize, value);

                        THEN("SendBufferSize option value is set to the double of the valid value")
                        {
                            const auto setOption = pSocket->getSocketOption(TcpSocket::SocketOption::SendBufferSize);
                            REQUIRE(setOption == (2 * value));
                        }
                    }

                    AND_WHEN("ReceiveBufferSize option is set to a valid value")
                    {
                        const auto value = GENERATE(AS(int),
                                                    rMemLimits.minValue,
                                                    rMemLimits.minValue + 18,
                                                    rMemLimits.minValue + 1024,
                                                    rMemLimits.defaultValue/2,
                                                    rMemLimits.defaultValue);
                        pSocket->setSocketOption(TcpSocket::SocketOption::ReceiveBufferSize, value);

                        THEN("ReceiveBufferSize option value is set to the double of the valid value")
                        {
                            const auto setOption = pSocket->getSocketOption(TcpSocket::SocketOption::ReceiveBufferSize);
                            REQUIRE(setOption == (2 * value));
                        }
                    }

                    THEN("connected peers can start exchanging data")
                    {
                        const auto dataToSend = GENERATE(AS(QByteArray),
                                                         QByteArray("a"),
                                                         QByteArray("abcdefgh"),
                                                         largeData);
                        const auto disableLowDelayOption = GENERATE(AS(bool), true, false);
                        const auto setKeepAliveOption = GENERATE(AS(bool), true, false);
                        const auto readBufferCapacity = GENERATE(AS(size_t), 0, 1024, 16384, 65536);
                        if (readBufferCapacity > 0)
                            pSocket->setReadBufferCapacity(readBufferCapacity);
                        if (disableLowDelayOption)
                            pSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, 0);
                        REQUIRE(pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay) == disableLowDelayOption ? 0 : 1);
                        if (setKeepAliveOption)
                        {
                            pPeerSocket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
                            pSocket->setSocketOption(TcpSocket::SocketOption::KeepAlive, 1);
                        }
                        REQUIRE(pPeerSocket->socketOption(QAbstractSocket::KeepAliveOption) == setKeepAliveOption ? 1 : 0);
                        REQUIRE(pSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive) == setKeepAliveOption ? 1 : 0);

                        AND_WHEN("peer sends data to TcpSocket")
                        {
                            pPeerSocket->write(dataToSend);

                            THEN("TcpSocket receives sent data")
                            {
                                const auto &sentData = socketReceivedData;
                                while (sentData != dataToSend)
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketReceivedDataFromPeerSemaphore, 1));
                                }

                                AND_WHEN("peer sends some more data to TcpSocket")
                                {
                                    socketReceivedData.clear();
                                    const QByteArray someMoreData("0123456789");
                                    pPeerSocket->write(someMoreData);

                                    THEN("TcpSocket receives sent data")
                                    {
                                        while (sentData != someMoreData)
                                        {
                                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketReceivedDataFromPeerSemaphore, 1));
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("peer closes connection after sending data to TcpSocket")
                        {
                            pPeerSocket->write(dataToSend);
                            pPeerSocket->disconnectFromHost();

                            THEN("TcpSocket receives sent data")
                            {
                                const auto &sentData = socketReceivedData;
                                while (sentData != dataToSend)
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketReceivedDataFromPeerSemaphore, 1));
                                }

                                AND_THEN("both peer and TcpSocket emit disconnected")
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                                    REQUIRE(pPeerSocket->error() == QAbstractSocket::UnknownSocketError);
                                    REQUIRE(pSocket->errorMessage().empty());

                                    AND_WHEN("peer is deleted")
                                    {
                                        while (peerFailedSemaphore.tryAcquire()) {}
                                        pPeerSocket.reset(nullptr);

                                        THEN("peer does not emit any error")
                                        {
                                            REQUIRE(!peerFailedSemaphore.tryAcquire());
                                            REQUIRE(pSocket->errorMessage().empty());
                                        }
                                    }

                                    AND_WHEN("TcpSocket is deleted")
                                    {
                                        while (socketFailedSemaphore.tryAcquire()) {}
                                        pSocket.reset(nullptr);

                                        THEN("neither peer or TcpSocket emit any error")
                                        {
                                            REQUIRE(!peerFailedSemaphore.tryAcquire());
                                            REQUIRE(!socketFailedSemaphore.tryAcquire());
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("peer aborts after writing data")
                        {
                            pPeerSocket->write(dataToSend);
                            pPeerSocket->abort();

                            THEN("both peer and TcpSocket emit disconnected")
                            {
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                                REQUIRE(pSocket->errorMessage().empty());
                            }
                        }

                        AND_WHEN("peer is deleted after writing data")
                        {
                            pPeerSocket->write(dataToSend);
                            pPeerSocket.reset(nullptr);

                            THEN("both peer and TcpSocket emit disconnected")
                            {
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                                REQUIRE(pSocket->errorMessage().empty());
                            }
                        }

                        AND_WHEN("TcpSocket sends data to peer")
                        {
                            pSocket->write(dataToSend.data(), dataToSend.size());

                            THEN("peer receives sent data")
                            {
                                const auto &sentData = peerReceivedData;
                                while (sentData != dataToSend)
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerReceivedDataFromTcpSocketSemaphore, 1));
                                }

                                AND_WHEN("TcpSocket sends some more data to peer")
                                {
                                    peerReceivedData.clear();
                                    const QByteArray someMoreData("0123456789");
                                    pSocket->write(someMoreData.data(), someMoreData.size());

                                    THEN("peer receives sent data")
                                    {
                                        while (sentData != someMoreData)
                                        {
                                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerReceivedDataFromTcpSocketSemaphore, 1));
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("TcpSocket closes connection after sending data to peer")
                        {
                            pSocket->write(dataToSend.data(), dataToSend.size());
                            pSocket->disconnectFromPeer();

                            THEN("peer receives sent data")
                            {
                                QByteArray sentData;
                                while (sentData != dataToSend)
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerReceivedDataFromTcpSocketSemaphore, 1));
                                    sentData = peerReceivedData;
                                }

                                AND_THEN("TcpSocket emits disconnected and peer emits RemoteHostClosedError before emiting disconnected")
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                                    REQUIRE(pPeerSocket->error() == QAbstractSocket::RemoteHostClosedError);
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                                    REQUIRE(pSocket->errorMessage().empty());

                                    AND_WHEN("TcpSocket is deleted")
                                    {
                                        while (socketFailedSemaphore.tryAcquire()) {}
                                        pSocket.reset(nullptr);

                                        THEN("TcpSocket does not emit any error")
                                        {
                                            REQUIRE(pPeerSocket->error() == QAbstractSocket::RemoteHostClosedError);
                                            REQUIRE(!socketFailedSemaphore.tryAcquire());
                                        }
                                    }

                                    AND_WHEN("Peer is deleted")
                                    {
                                        while (peerFailedSemaphore.tryAcquire()) {}
                                        pPeerSocket.reset(nullptr);

                                        THEN("neither peer or TcpSocket emit any error")
                                        {
                                            REQUIRE(!peerFailedSemaphore.tryAcquire());
                                            REQUIRE(!socketFailedSemaphore.tryAcquire());
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("TcpSocket aborts after writing data")
                        {
                            pSocket->write(dataToSend.data(), dataToSend.size());
                            pSocket->abort();

                            THEN("TcpSocket aborts and Peer emits RemoteHostClosedError before emiting disconnected")
                            {
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                                REQUIRE(pPeerSocket->error() == QAbstractSocket::RemoteHostClosedError);
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                                REQUIRE(pSocket->errorMessage().empty());
                            }
                        }

                        AND_WHEN("TcpSocket is deleted after writing data")
                        {
                            pSocket->write(dataToSend.data(), dataToSend.size());
                            pSocket.reset(nullptr);

                            THEN("TcpSocket aborts and Peer emits RemoteHostClosedError before emiting disconnected")
                            {
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                                REQUIRE(pPeerSocket->error() == QAbstractSocket::RemoteHostClosedError);
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            }
                        }
                    }

                    AND_WHEN("peer disconnects from TcpSocket")
                    {
                        pPeerSocket->disconnectFromHost();

                        THEN("peer emits disconnected and TcpSocket emits disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                            REQUIRE(pPeerSocket->error() == QAbstractSocket::UnknownSocketError);
                            REQUIRE(pSocket->errorMessage().empty());

                            AND_WHEN("peer is deleted")
                            {
                                while (peerFailedSemaphore.tryAcquire()) {}
                                pPeerSocket.reset(nullptr);

                                THEN("peer does not emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(pSocket->errorMessage().empty());
                                }
                            }

                            AND_WHEN("TcpSocket is deleted")
                            {
                                while (socketFailedSemaphore.tryAcquire()) {}
                                pSocket.reset(nullptr);

                                THEN("neither peer or TcpSocket emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }
                        }
                    }

                    AND_WHEN("peer aborts connection")
                    {
                        pPeerSocket->abort();

                        THEN("both peer and TcpSocket emit disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                            REQUIRE(pPeerSocket->error() == QAbstractSocket::UnknownSocketError);
                            REQUIRE(pSocket->errorMessage().empty());

                            AND_WHEN("peer is deleted")
                            {
                                while (peerFailedSemaphore.tryAcquire()) {}
                                pPeerSocket.reset(nullptr);

                                THEN("peer does not emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(pSocket->errorMessage().empty());
                                }
                            }

                            AND_WHEN("TcpSocket is deleted")
                            {
                                while (socketFailedSemaphore.tryAcquire()) {}
                                pSocket.reset(nullptr);

                                THEN("neither peer or TcpSocket emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }
                        }
                    }

                    AND_WHEN("TcpSocket disconnects from TcpSocket")
                    {
                        pSocket->disconnectFromPeer();

                        THEN("TcpSocket emits disconnected and peer emits RemoteHostClosedError before emiting disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                            REQUIRE(pPeerSocket->error() == QAbstractSocket::RemoteHostClosedError);
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(pSocket->errorMessage().empty());

                            AND_WHEN("peer is deleted")
                            {
                                while (peerFailedSemaphore.tryAcquire()) {}
                                pPeerSocket.reset(nullptr);

                                THEN("neither peer or TcpSocket emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }

                            AND_WHEN("TcpSocket is deleted")
                            {
                                pSocket.reset(nullptr);

                                THEN("TcpSocket does not emit any error")
                                {
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                    REQUIRE(pPeerSocket->error() == QAbstractSocket::RemoteHostClosedError);
                                }
                            }
                        }
                    }

                    AND_WHEN("TcpSocket aborts connection")
                    {
                        pSocket->abort();

                        THEN("Peer emits RemoteHostClosedError before emiting disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                            REQUIRE(pPeerSocket->error() == QAbstractSocket::RemoteHostClosedError);
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(pSocket->errorMessage().empty());

                            AND_WHEN("peer is deleted")
                            {
                                while (peerFailedSemaphore.tryAcquire()) {}
                                pPeerSocket.reset(nullptr);

                                THEN("neither peer or TcpSocket emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }

                            AND_WHEN("TcpSocket is deleted")
                            {
                                pSocket.reset(nullptr);

                                THEN("TcpSocket does not emit any error")
                                {
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                    REQUIRE(pPeerSocket->error() == QAbstractSocket::RemoteHostClosedError);
                                }
                            }
                        }
                    }

                    AND_WHEN("both peer and TcpSocket disconnects")
                    {
                        pPeerSocket->disconnectFromHost();
                        pSocket->disconnectFromPeer();

                        THEN("peer emits disconnected and TcpSocket emits disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                            REQUIRE(pSocket->errorMessage().empty());

                            AND_WHEN("peer is deleted")
                            {
                                while (peerFailedSemaphore.tryAcquire()) {}
                                pPeerSocket.reset(nullptr);

                                THEN("TcpSocket not emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(pSocket->errorMessage().empty());
                                }
                            }

                            AND_WHEN("TcpSocket is deleted")
                            {
                                while (socketFailedSemaphore.tryAcquire()) {}
                                pSocket.reset(nullptr);

                                THEN("TcpSocket does not emit any error")
                                {
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }
                        }
                    }

                    AND_WHEN("both TcpSocket and peer disconnects")
                    {
                        pSocket->disconnectFromPeer();
                        pPeerSocket->disconnectFromHost();

                        THEN("peer emits disconnected and TcpSocket emits disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                            REQUIRE(pSocket->errorMessage().empty());

                            AND_WHEN("peer is deleted")
                            {
                                while (peerFailedSemaphore.tryAcquire()) {}
                                pPeerSocket.reset(nullptr);

                                THEN("TcpSocket not emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(pSocket->errorMessage().empty());
                                }
                            }

                            AND_WHEN("TcpSocket is deleted")
                            {
                                while (socketFailedSemaphore.tryAcquire()) {}
                                pSocket.reset(nullptr);

                                THEN("TcpSocket does not emit any error")
                                {
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }
                        }
                    }

                    AND_WHEN("peer is deleted")
                    {
                        while (peerFailedSemaphore.tryAcquire()) {}
                        pPeerSocket.reset(nullptr);

                        THEN("peer does not emit any error and TcpSocket emits disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                            REQUIRE(pSocket->errorMessage().empty());
                            REQUIRE(!peerFailedSemaphore.tryAcquire());
                        }
                    }

                    AND_WHEN("TcpSocket is deleted")
                    {
                        while (socketFailedSemaphore.tryAcquire()) {}
                        pSocket.reset(nullptr);
                        QCoreApplication::processEvents();

                        THEN("peer socket emits error and disconnected signal")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(pPeerSocket->error() == QAbstractSocket::SocketError::RemoteHostClosedError);
                            REQUIRE(!socketFailedSemaphore.tryAcquire());
                        }
                    }
                }
            }
        }
    }
}


SCENARIO("TcpSocket interacts with server peer by name")
{
    GIVEN("a listening server")
    {
        QTcpServer server;
        QTcpServer ipv6Server;
        bool connectedToIpv6Server = false;
        QSemaphore peerConnectedSemaphore;
        QSemaphore peerFailedSemaphore;
        QSemaphore peerDisconnectedSemaphore;
        QSemaphore peerReceivedDataFromTcpSocketSemaphore;
        QByteArray peerReceivedData;
        std::unique_ptr<QTcpSocket> pPeerSocket;
        QObject::connect(&ipv6Server, &QTcpServer::newConnection, [&]()
        {
            connectedToIpv6Server = true;
            REQUIRE(ipv6Server.hasPendingConnections());
            REQUIRE(!pPeerSocket);
            pPeerSocket.reset(ipv6Server.nextPendingConnection());
            REQUIRE(pPeerSocket.get() != nullptr);
            pPeerSocket->setParent(nullptr);
            REQUIRE(!ipv6Server.hasPendingConnections());
            QObject::connect(pPeerSocket.get(), &QTcpSocket::errorOccurred, [&](QAbstractSocket::SocketError error) {peerFailedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QTcpSocket::disconnected, [&](){peerDisconnectedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QTcpSocket::readyRead, [&]()
            {
                peerReceivedData.append(pPeerSocket->readAll());
                peerReceivedDataFromTcpSocketSemaphore.release();
            });
            peerConnectedSemaphore.release();
        });
        REQUIRE(ipv6Server.listen(QHostAddress("::1"), 0));
        const auto serverPort = ipv6Server.serverPort();
        REQUIRE(serverPort >= 1024);
        QObject::connect(&server, &QTcpServer::newConnection, [&]()
        {
            REQUIRE(server.hasPendingConnections());
            REQUIRE(!pPeerSocket);
            pPeerSocket.reset(server.nextPendingConnection());
            REQUIRE(pPeerSocket.get() != nullptr);
            pPeerSocket->setParent(nullptr);
            REQUIRE(!server.hasPendingConnections());
            QObject::connect(pPeerSocket.get(), &QTcpSocket::errorOccurred, [&](QAbstractSocket::SocketError error) {peerFailedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QTcpSocket::disconnected, [&](){peerDisconnectedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QTcpSocket::readyRead, [&]()
            {
                peerReceivedData.append(pPeerSocket->readAll());
                peerReceivedDataFromTcpSocketSemaphore.release();
            });
            peerConnectedSemaphore.release();
        });
        const auto serverAddress = GENERATE(AS(QHostAddress),
                                            QHostAddress("127.10.20.50"),
                                            QHostAddress("127.10.20.60"),
                                            QHostAddress("127.10.20.70"),
                                            QHostAddress("127.10.20.80"),
                                            QHostAddress("127.10.20.90"));
        REQUIRE(server.listen(serverAddress, serverPort));

        WHEN("TcpSocket connects to server")
        {
            QSemaphore socketConnectedSemaphore;
            QSemaphore socketFailedSemaphore;
            QSemaphore socketDisconnectedSemaphore;
            QSemaphore socketReceivedDataFromPeerSemaphore;
            QByteArray socketReceivedData;
            std::unique_ptr<TcpSocket> pSocket(new TcpSocket);
            Object::connect(pSocket.get(), &TcpSocket::error, [&]() {socketFailedSemaphore.release();});
            Object::connect(pSocket.get(), &TcpSocket::connected, [&](){socketConnectedSemaphore.release();});
            Object::connect(pSocket.get(), &TcpSocket::disconnected, [&](){socketDisconnectedSemaphore.release();});
            Object::connect(pSocket.get(), &TcpSocket::receivedData, [&]()
            {
                QByteArray readData;
                readData.resize(pSocket->dataAvailable());
                pSocket->read(readData.data(), readData.size());
                socketReceivedData.append(readData);
                socketReceivedDataFromPeerSemaphore.release();
            });
            const auto serverBindAddressAndPort = GENERATE(AS(std::pair<QHostAddress, bool>),
                                                           {QHostAddress::Any, false},
                                                           {QHostAddress("127.2.3.18"), true},
                                                           {QHostAddress("127.2.3.20"), false},
                                                           {QHostAddress("::1"), true},
                                                           {QHostAddress("::1"), false});
            bool isBound = false;
            QHostAddress bindAddress;
            bool isBindPortZero = false;
            quint16 bindPort = 0;
            if (serverBindAddressAndPort.first != QHostAddress::Any)
            {
                isBound = true;
                bindAddress = serverBindAddressAndPort.first;
                isBindPortZero = !serverBindAddressAndPort.second;
                if (!isBindPortZero)
                {
                    QTcpSocket socket;
                    REQUIRE(socket.bind(serverBindAddressAndPort.first));
                    bindPort = socket.localPort();
                    REQUIRE(bindPort > 1024 && bindPort <= 65535);
                    socket.abort();
                    pSocket->setBindAddressAndPort(bindAddress.toString().toStdString(), bindPort);
                }
                else
                    pSocket->setBindAddressAndPort(bindAddress.toString().toStdString());
            }
            pSocket->connect("test.onlocalhost.com", serverPort);

            THEN("peer emits newConnection with a connected socket")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                REQUIRE(pPeerSocket->state() == QAbstractSocket::ConnectedState);

                AND_THEN("TcpSocket emits connected")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                    REQUIRE(pPeerSocket->localAddress().toString().toStdString() == pSocket->peerAddress());
                    REQUIRE(pPeerSocket->localPort() == pSocket->peerPort());
                    REQUIRE(pPeerSocket->peerAddress().toString().toStdString() == pSocket->localAddress());
                    REQUIRE(pPeerSocket->peerPort() == pSocket->localPort());
                    if (isBound)
                    {
                        REQUIRE(pSocket->localAddress() == bindAddress.toString().toStdString());

                        if (!isBindPortZero)
                        {
                            REQUIRE(pSocket->localPort() == bindPort);
                        }
                    }
                    if (serverBindAddressAndPort.first != QHostAddress::Any)
                    {
                        if (serverBindAddressAndPort.first.protocol() == QAbstractSocket::IPv6Protocol)
                        {
                            REQUIRE(connectedToIpv6Server);
                        }
                        else
                        {
                            REQUIRE(!connectedToIpv6Server);
                        }
                    }
                    
                    AND_THEN("socket is constructed with LowDelay option set")
                    {
                        const auto defaultLowDelayOption = pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay);
                        REQUIRE(defaultLowDelayOption == 1);

                        AND_WHEN("LowDelay option is set to 0")
                        {
                            pSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, 0);

                            THEN("LowDelay option becomes unset")
                            {
                                const auto setLowDelayOption = pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay);
                                REQUIRE(setLowDelayOption == 0);

                                AND_WHEN("LowDelay option is enabled again")
                                {
                                    pSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, 1);

                                    THEN("LowDelay option becomes set")
                                    {
                                        const auto setLowDelayOption = pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay);
                                        REQUIRE(setLowDelayOption == 1);
                                    }
                                }

                                AND_WHEN("LowDelay option is set to a value other than 0 or 1")
                                {
                                    const auto value = GENERATE(AS(int), -3, 2, 5, 117);
                                    pSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, value);

                                    THEN("LowDelay option is set")
                                    {
                                        const auto setLowDelayOption = pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay);
                                        REQUIRE(setLowDelayOption == 1);
                                    }
                                }
                            }
                        }

                        AND_WHEN("LowDelay option is set to 1")
                        {
                            pSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, 1);

                            THEN("LowDelay option stays set")
                            {
                                const auto setLowDelayOption = pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay);
                                REQUIRE(setLowDelayOption == 1);
                            }
                        }

                        AND_WHEN("LowDelay option is set to a value other than 0 or 1")
                        {
                            const auto value = GENERATE(AS(int), -3, 2, 5, 117);
                            pSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, value);

                            THEN("LowDelay option stays set")
                            {
                                const auto setLowDelayOption = pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay);
                                REQUIRE(setLowDelayOption == 1);
                            }
                        }
                    }

                    AND_THEN("socket is constructed with KeepAlive option unset")
                    {
                        const auto defaultKeepAliveOption = pSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive);
                        REQUIRE(defaultKeepAliveOption == 0);

                        AND_WHEN("KeepAlive option is set to 1")
                        {
                            pSocket->setSocketOption(TcpSocket::SocketOption::KeepAlive, 1);

                            THEN("KeepAlive option becomes set")
                            {
                                const auto setKeepAliveOption = pSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive);
                                REQUIRE(setKeepAliveOption == 1);

                                AND_WHEN("KeepAlive option is set to 0")
                                {
                                    pSocket->setSocketOption(TcpSocket::SocketOption::KeepAlive, 0);

                                    THEN("KeepAlive option becomes unset")
                                    {
                                        const auto setKeepAliveOption = pSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive);
                                        REQUIRE(setKeepAliveOption == 0);
                                    }
                                }
                            }
                        }

                        AND_WHEN("KeepAlive option is set to 0")
                        {
                            pSocket->setSocketOption(TcpSocket::SocketOption::KeepAlive, 0);

                            THEN("KeepAlive option stays unset")
                            {
                                const auto setKeepAliveOption = pSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive);
                                REQUIRE(setKeepAliveOption == 0);
                            }
                        }

                        AND_WHEN("KeepAlive option is set to a value other than 0 or 1")
                        {
                            const auto value = GENERATE(AS(int), -3, 2, 5, 117);
                            pSocket->setSocketOption(TcpSocket::SocketOption::KeepAlive, value);

                            THEN("KeepAlive option is set")
                            {
                                const auto setKeepAliveOption = pSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive);
                                REQUIRE(setKeepAliveOption == 1);
                            }
                        }
                    }

                    AND_WHEN("SendBufferSize option is set to a valid value")
                    {
                        const auto value = GENERATE(AS(int),
                                                    wMemLimits.minValue,
                                                    wMemLimits.minValue + 18,
                                                    wMemLimits.minValue + 1024,
                                                    wMemLimits.defaultValue/2,
                                                    wMemLimits.defaultValue);
                        pSocket->setSocketOption(TcpSocket::SocketOption::SendBufferSize, value);

                        THEN("SendBufferSize option value is set to the double of the valid value")
                        {
                            const auto setOption = pSocket->getSocketOption(TcpSocket::SocketOption::SendBufferSize);
                            REQUIRE(setOption == (2 * value));
                        }
                    }

                    AND_WHEN("ReceiveBufferSize option is set to a valid value")
                    {
                        const auto value = GENERATE(AS(int),
                                                    rMemLimits.minValue,
                                                    rMemLimits.minValue + 18,
                                                    rMemLimits.minValue + 1024,
                                                    rMemLimits.defaultValue/2,
                                                    rMemLimits.defaultValue);
                        pSocket->setSocketOption(TcpSocket::SocketOption::ReceiveBufferSize, value);

                        THEN("ReceiveBufferSize option value is set to the double of the valid value")
                        {
                            const auto setOption = pSocket->getSocketOption(TcpSocket::SocketOption::ReceiveBufferSize);
                            REQUIRE(setOption == (2 * value));
                        }
                    }

                    THEN("connected peers can start exchanging data")
                    {
                        const auto dataToSend = GENERATE(AS(QByteArray),
                                                         QByteArray("a"),
                                                         QByteArray("abcdefgh"),
                                                         largeData);
                        const auto disableLowDelayOption = GENERATE(AS(bool), true, false);
                        const auto setKeepAliveOption = GENERATE(AS(bool), true, false);
                        const auto readBufferCapacity = GENERATE(AS(size_t), 0, 1024, 16384, 65536);
                        if (readBufferCapacity > 0)
                            pSocket->setReadBufferCapacity(readBufferCapacity);
                        if (disableLowDelayOption)
                            pSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, 0);
                        REQUIRE(pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay) == disableLowDelayOption ? 0 : 1);
                        if (setKeepAliveOption)
                        {
                            pPeerSocket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
                            pSocket->setSocketOption(TcpSocket::SocketOption::KeepAlive, 1);
                        }
                        REQUIRE(pPeerSocket->socketOption(QAbstractSocket::KeepAliveOption) == setKeepAliveOption ? 1 : 0);
                        REQUIRE(pSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive) == setKeepAliveOption ? 1 : 0);

                        AND_WHEN("peer sends data to TcpSocket")
                        {
                            pPeerSocket->write(dataToSend);

                            THEN("TcpSocket receives sent data")
                            {
                                const auto &sentData = socketReceivedData;
                                while (sentData != dataToSend)
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketReceivedDataFromPeerSemaphore, 1));
                                }

                                AND_WHEN("peer sends some more data to TcpSocket")
                                {
                                    socketReceivedData.clear();
                                    const QByteArray someMoreData("0123456789");
                                    pPeerSocket->write(someMoreData);

                                    THEN("TcpSocket receives sent data")
                                    {
                                        while (sentData != someMoreData)
                                        {
                                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketReceivedDataFromPeerSemaphore, 1));
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("peer closes connection after sending data to TcpSocket")
                        {
                            pPeerSocket->write(dataToSend);
                            pPeerSocket->disconnectFromHost();

                            THEN("TcpSocket receives sent data")
                            {
                                const auto &sentData = socketReceivedData;
                                while (sentData != dataToSend)
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketReceivedDataFromPeerSemaphore, 1));
                                }

                                AND_THEN("both peer and TcpSocket emit disconnected")
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                                    REQUIRE(pPeerSocket->error() == QAbstractSocket::UnknownSocketError);
                                    REQUIRE(pSocket->errorMessage().empty());

                                    AND_WHEN("peer is deleted")
                                    {
                                        while (peerFailedSemaphore.tryAcquire()) {}
                                        pPeerSocket.reset(nullptr);

                                        THEN("peer does not emit any error")
                                        {
                                            REQUIRE(!peerFailedSemaphore.tryAcquire());
                                            REQUIRE(pSocket->errorMessage().empty());
                                        }
                                    }

                                    AND_WHEN("TcpSocket is deleted")
                                    {
                                        while (socketFailedSemaphore.tryAcquire()) {}
                                        pSocket.reset(nullptr);

                                        THEN("neither peer or TcpSocket emit any error")
                                        {
                                            REQUIRE(!peerFailedSemaphore.tryAcquire());
                                            REQUIRE(!socketFailedSemaphore.tryAcquire());
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("peer aborts after writing data")
                        {
                            pPeerSocket->write(dataToSend);
                            pPeerSocket->abort();

                            THEN("both peer and TcpSocket emit disconnected")
                            {
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                                REQUIRE(pSocket->errorMessage().empty());
                            }
                        }

                        AND_WHEN("peer is deleted after writing data")
                        {
                            pPeerSocket->write(dataToSend);
                            pPeerSocket.reset(nullptr);

                            THEN("both peer and TcpSocket emit disconnected")
                            {
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                                REQUIRE(pSocket->errorMessage().empty());
                            }
                        }

                        AND_WHEN("TcpSocket sends data to peer")
                        {
                            pSocket->write(dataToSend.data(), dataToSend.size());

                            THEN("peer receives sent data")
                            {
                                const auto &sentData = peerReceivedData;
                                while (sentData != dataToSend)
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerReceivedDataFromTcpSocketSemaphore, 1));
                                }

                                AND_WHEN("TcpSocket sends some more data to peer")
                                {
                                    peerReceivedData.clear();
                                    const QByteArray someMoreData("0123456789");
                                    pSocket->write(someMoreData.data(), someMoreData.size());

                                    THEN("peer receives sent data")
                                    {
                                        while (sentData != someMoreData)
                                        {
                                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerReceivedDataFromTcpSocketSemaphore, 1));
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("TcpSocket closes connection after sending data to peer")
                        {
                            pSocket->write(dataToSend.data(), dataToSend.size());
                            pSocket->disconnectFromPeer();

                            THEN("peer receives sent data")
                            {
                                QByteArray sentData;
                                while (sentData != dataToSend)
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerReceivedDataFromTcpSocketSemaphore, 1));
                                    sentData = peerReceivedData;
                                }

                                AND_THEN("TcpSocket emits disconnected and peer emits RemoteHostClosedError before emiting disconnected")
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                                    REQUIRE(pPeerSocket->error() == QAbstractSocket::RemoteHostClosedError);
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                                    REQUIRE(pSocket->errorMessage().empty());

                                    AND_WHEN("TcpSocket is deleted")
                                    {
                                        while (socketFailedSemaphore.tryAcquire()) {}
                                        pSocket.reset(nullptr);

                                        THEN("TcpSocket does not emit any error")
                                        {
                                            REQUIRE(pPeerSocket->error() == QAbstractSocket::RemoteHostClosedError);
                                            REQUIRE(!socketFailedSemaphore.tryAcquire());
                                        }
                                    }

                                    AND_WHEN("Peer is deleted")
                                    {
                                        while (peerFailedSemaphore.tryAcquire()) {}
                                        pPeerSocket.reset(nullptr);

                                        THEN("neither peer or TcpSocket emit any error")
                                        {
                                            REQUIRE(!peerFailedSemaphore.tryAcquire());
                                            REQUIRE(!socketFailedSemaphore.tryAcquire());
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("TcpSocket aborts after writing data")
                        {
                            pSocket->write(dataToSend.data(), dataToSend.size());
                            pSocket->abort();

                            THEN("TcpSocket aborts and Peer emits RemoteHostClosedError before emiting disconnected")
                            {
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                                REQUIRE(pPeerSocket->error() == QAbstractSocket::RemoteHostClosedError);
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                                REQUIRE(pSocket->errorMessage().empty());
                            }
                        }

                        AND_WHEN("TcpSocket is deleted after writing data")
                        {
                            pSocket->write(dataToSend.data(), dataToSend.size());
                            pSocket.reset(nullptr);

                            THEN("TcpSocket aborts and Peer emits RemoteHostClosedError before emiting disconnected")
                            {
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                                REQUIRE(pPeerSocket->error() == QAbstractSocket::RemoteHostClosedError);
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            }
                        }
                    }

                    AND_WHEN("peer disconnects from TcpSocket")
                    {
                        pPeerSocket->disconnectFromHost();

                        THEN("peer emits disconnected and TcpSocket emits disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                            REQUIRE(pPeerSocket->error() == QAbstractSocket::UnknownSocketError);
                            REQUIRE(pSocket->errorMessage().empty());

                            AND_WHEN("peer is deleted")
                            {
                                while (peerFailedSemaphore.tryAcquire()) {}
                                pPeerSocket.reset(nullptr);

                                THEN("peer does not emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(pSocket->errorMessage().empty());
                                }
                            }

                            AND_WHEN("TcpSocket is deleted")
                            {
                                while (socketFailedSemaphore.tryAcquire()) {}
                                pSocket.reset(nullptr);

                                THEN("neither peer or TcpSocket emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }
                        }
                    }

                    AND_WHEN("peer aborts connection")
                    {
                        pPeerSocket->abort();

                        THEN("both peer and TcpSocket emit disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                            REQUIRE(pPeerSocket->error() == QAbstractSocket::UnknownSocketError);
                            REQUIRE(pSocket->errorMessage().empty());

                            AND_WHEN("peer is deleted")
                            {
                                while (peerFailedSemaphore.tryAcquire()) {}
                                pPeerSocket.reset(nullptr);

                                THEN("peer does not emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(pSocket->errorMessage().empty());
                                }
                            }

                            AND_WHEN("TcpSocket is deleted")
                            {
                                while (socketFailedSemaphore.tryAcquire()) {}
                                pSocket.reset(nullptr);

                                THEN("neither peer or TcpSocket emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }
                        }
                    }

                    AND_WHEN("TcpSocket disconnects from TcpSocket")
                    {
                        pSocket->disconnectFromPeer();

                        THEN("TcpSocket emits disconnected and peer emits RemoteHostClosedError before emiting disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                            REQUIRE(pPeerSocket->error() == QAbstractSocket::RemoteHostClosedError);
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(pSocket->errorMessage().empty());

                            AND_WHEN("peer is deleted")
                            {
                                while (peerFailedSemaphore.tryAcquire()) {}
                                pPeerSocket.reset(nullptr);

                                THEN("neither peer or TcpSocket emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }

                            AND_WHEN("TcpSocket is deleted")
                            {
                                pSocket.reset(nullptr);

                                THEN("TcpSocket does not emit any error")
                                {
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                    REQUIRE(pPeerSocket->error() == QAbstractSocket::RemoteHostClosedError);
                                }
                            }
                        }
                    }

                    AND_WHEN("TcpSocket aborts connection")
                    {
                        pSocket->abort();

                        THEN("Peer emits RemoteHostClosedError before emiting disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                            REQUIRE(pPeerSocket->error() == QAbstractSocket::RemoteHostClosedError);
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(pSocket->errorMessage().empty());

                            AND_WHEN("peer is deleted")
                            {
                                while (peerFailedSemaphore.tryAcquire()) {}
                                pPeerSocket.reset(nullptr);

                                THEN("neither peer or TcpSocket emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }

                            AND_WHEN("TcpSocket is deleted")
                            {
                                pSocket.reset(nullptr);

                                THEN("TcpSocket does not emit any error")
                                {
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                    REQUIRE(pPeerSocket->error() == QAbstractSocket::RemoteHostClosedError);
                                }
                            }
                        }
                    }

                    AND_WHEN("both peer and TcpSocket disconnects")
                    {
                        pPeerSocket->disconnectFromHost();
                        pSocket->disconnectFromPeer();

                        THEN("peer emits disconnected and TcpSocket emits disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                            REQUIRE(pSocket->errorMessage().empty());

                            AND_WHEN("peer is deleted")
                            {
                                while (peerFailedSemaphore.tryAcquire()) {}
                                pPeerSocket.reset(nullptr);

                                THEN("TcpSocket not emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(pSocket->errorMessage().empty());
                                }
                            }

                            AND_WHEN("TcpSocket is deleted")
                            {
                                while (socketFailedSemaphore.tryAcquire()) {}
                                pSocket.reset(nullptr);

                                THEN("TcpSocket does not emit any error")
                                {
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }
                        }
                    }

                    AND_WHEN("both TcpSocket and peer disconnects")
                    {
                        pSocket->disconnectFromPeer();
                        pPeerSocket->disconnectFromHost();

                        THEN("peer emits disconnected and TcpSocket emits disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                            REQUIRE(pSocket->errorMessage().empty());

                            AND_WHEN("peer is deleted")
                            {
                                while (peerFailedSemaphore.tryAcquire()) {}
                                pPeerSocket.reset(nullptr);

                                THEN("TcpSocket not emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(pSocket->errorMessage().empty());
                                }
                            }

                            AND_WHEN("TcpSocket is deleted")
                            {
                                while (socketFailedSemaphore.tryAcquire()) {}
                                pSocket.reset(nullptr);

                                THEN("TcpSocket does not emit any error")
                                {
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }
                        }
                    }

                    AND_WHEN("peer is deleted")
                    {
                        while (peerFailedSemaphore.tryAcquire()) {}
                        pPeerSocket.reset(nullptr);

                        THEN("peer does not emit any error and TcpSocket emits disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                            REQUIRE(pSocket->errorMessage().empty());
                            REQUIRE(!peerFailedSemaphore.tryAcquire());
                        }
                    }

                    AND_WHEN("TcpSocket is deleted")
                    {
                        while (socketFailedSemaphore.tryAcquire()) {}
                        pSocket.reset(nullptr);
                        QCoreApplication::processEvents();

                        THEN("peer socket emits error and disconnected signal")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(pPeerSocket->error() == QAbstractSocket::SocketError::RemoteHostClosedError);
                            REQUIRE(!socketFailedSemaphore.tryAcquire());
                        }
                    }
                }
            }
        }
    }
}


SCENARIO("TcpSocket interacts with client TcpSocket-based peer")
{
    GIVEN("a listening server")
    {
        TcpServer server;
        QSemaphore socketConnectedSemaphore;
        QSemaphore socketFailedSemaphore;
        QSemaphore socketDisconnectedSemaphore;
        QSemaphore socketReceivedDataFromPeerSemaphore;
        QByteArray socketReceivedData;
        std::unique_ptr<TcpSocket> pSocket;
        Object::connect(&server, &TcpServer::newConnection, [&](TcpSocket *pNewSocket)
        {
            pSocket.reset(pNewSocket);
            Object::connect(pSocket.get(), &TcpSocket::error, [&]() {socketFailedSemaphore.release();});
            Object::connect(pSocket.get(), &TcpSocket::disconnected, [&socketDisconnectedSemaphore](){socketDisconnectedSemaphore.release();});
            Object::connect(pSocket.get(), &TcpSocket::receivedData, [&]()
            {
                QByteArray readData;
                readData.resize(pSocket->dataAvailable());
                pSocket->read(readData.data(), readData.size());
                socketReceivedData.append(readData);
                socketReceivedDataFromPeerSemaphore.release();
            });
            socketConnectedSemaphore.release();
        });
        const auto serverAddress = GENERATE(AS(QHostAddress), QHostAddress("127.10.10.25"), QHostAddress("::1"));
        REQUIRE(server.listen(serverAddress, 0));
        const auto serverPort = server.serverPort();
        REQUIRE(serverPort >= 1024);

        WHEN("peer connects to host")
        {
            QSemaphore peerConnectedSemaphore;
            QSemaphore peerFailedSemaphore;
            QSemaphore peerDisconnectedSemaphore;
            QSemaphore peerReceivedDataFromTcpSocketSemaphore;
            QByteArray peerReceivedData;
            std::unique_ptr<TcpSocket> pPeerSocket(new TcpSocket);
            Object::connect(pPeerSocket.get(), &TcpSocket::error, [&]() {peerFailedSemaphore.release();});
            Object::connect(pPeerSocket.get(), &TcpSocket::connected, [&peerConnectedSemaphore](){peerConnectedSemaphore.release();});
            Object::connect(pPeerSocket.get(), &TcpSocket::disconnected, [&peerDisconnectedSemaphore](){peerDisconnectedSemaphore.release();});
            Object::connect(pPeerSocket.get(), &TcpSocket::receivedData, [&]()
            {
                peerReceivedData.append(pPeerSocket->readAll());
                peerReceivedDataFromTcpSocketSemaphore.release();
            });
            pPeerSocket->connect(serverAddress.toString().toStdString(), serverPort);

            THEN("server emits newConnection with a connected socket")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                REQUIRE(pSocket->state() == TcpSocket::State::Connected);

                AND_THEN("connecting peer socket emits connected")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                    REQUIRE(pPeerSocket->localAddress() == pSocket->peerAddress());
                    REQUIRE(pPeerSocket->localPort() == pSocket->peerPort());
                    REQUIRE(pPeerSocket->peerAddress() == pSocket->localAddress());
                    REQUIRE(pPeerSocket->peerPort() == pSocket->localPort());

                    AND_THEN("socket is constructed with LowDelay option set")
                    {
                        const auto defaultLowDelayOption = pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay);
                        REQUIRE(defaultLowDelayOption == 1);

                        AND_WHEN("LowDelay option is set to 0")
                        {
                            pSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, 0);

                            THEN("LowDelay option becomes unset")
                            {
                                const auto setLowDelayOption = pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay);
                                REQUIRE(setLowDelayOption == 0);

                                AND_WHEN("LowDelay option is enabled again")
                                {
                                    pSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, 1);

                                    THEN("LowDelay option becomes set")
                                    {
                                        const auto setLowDelayOption = pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay);
                                        REQUIRE(setLowDelayOption == 1);
                                    }
                                }

                                AND_WHEN("LowDelay option is set to a value other than 0 or 1")
                                {
                                    const auto value = GENERATE(AS(int), -3, 2, 5, 117);
                                    pSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, value);

                                    THEN("LowDelay option is set")
                                    {
                                        const auto setLowDelayOption = pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay);
                                        REQUIRE(setLowDelayOption == 1);
                                    }
                                }
                            }
                        }

                        AND_WHEN("LowDelay option is set to 1")
                        {
                            pSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, 1);

                            THEN("LowDelay option stays set")
                            {
                                const auto setLowDelayOption = pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay);
                                REQUIRE(setLowDelayOption == 1);
                            }
                        }

                        AND_WHEN("LowDelay option is set to a value other than 0 or 1")
                        {
                            const auto value = GENERATE(AS(int), -3, 2, 5, 117);
                            pSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, value);

                            THEN("LowDelay option stays set")
                            {
                                const auto setLowDelayOption = pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay);
                                REQUIRE(setLowDelayOption == 1);
                            }
                        }
                    }

                    AND_THEN("socket is constructed with KeepAlive option unset")
                    {
                        const auto defaultKeepAliveOption = pSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive);
                        REQUIRE(defaultKeepAliveOption == 0);

                        AND_WHEN("KeepAlive option is set to 1")
                        {
                            pSocket->setSocketOption(TcpSocket::SocketOption::KeepAlive, 1);

                            THEN("KeepAlive option becomes set")
                            {
                                const auto setKeepAliveOption = pSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive);
                                REQUIRE(setKeepAliveOption == 1);

                                AND_WHEN("KeepAlive option is set to 0")
                                {
                                    pSocket->setSocketOption(TcpSocket::SocketOption::KeepAlive, 0);

                                    THEN("KeepAlive option becomes unset")
                                    {
                                        const auto setKeepAliveOption = pSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive);
                                        REQUIRE(setKeepAliveOption == 0);
                                    }
                                }
                            }
                        }

                        AND_WHEN("KeepAlive option is set to 0")
                        {
                            pSocket->setSocketOption(TcpSocket::SocketOption::KeepAlive, 0);

                            THEN("KeepAlive option stays unset")
                            {
                                const auto setKeepAliveOption = pSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive);
                                REQUIRE(setKeepAliveOption == 0);
                            }
                        }

                        AND_WHEN("KeepAlive option is set to a value other than 0 or 1")
                        {
                            const auto value = GENERATE(AS(int), -3, 2, 5, 117);
                            pSocket->setSocketOption(TcpSocket::SocketOption::KeepAlive, value);

                            THEN("KeepAlive option is set")
                            {
                                const auto setKeepAliveOption = pSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive);
                                REQUIRE(setKeepAliveOption == 1);
                            }
                        }
                    }

                    AND_WHEN("SendBufferSize option is set to a valid value")
                    {
                        const auto value = GENERATE(AS(int),
                                                    wMemLimits.minValue,
                                                    wMemLimits.minValue + 18,
                                                    wMemLimits.minValue + 1024,
                                                    wMemLimits.defaultValue/2,
                                                    wMemLimits.defaultValue);
                        pSocket->setSocketOption(TcpSocket::SocketOption::SendBufferSize, value);

                        THEN("SendBufferSize option value is set to the double of the valid value")
                        {
                            const auto setOption = pSocket->getSocketOption(TcpSocket::SocketOption::SendBufferSize);
                            REQUIRE(setOption == (2 * value));
                        }
                    }

                    AND_WHEN("ReceiveBufferSize option is set to a valid value")
                    {
                        const auto value = GENERATE(AS(int),
                                                    rMemLimits.minValue,
                                                    rMemLimits.minValue + 18,
                                                    rMemLimits.minValue + 1024,
                                                    rMemLimits.defaultValue/2,
                                                    rMemLimits.defaultValue);
                        pSocket->setSocketOption(TcpSocket::SocketOption::ReceiveBufferSize, value);

                        THEN("ReceiveBufferSize option value is set to the double of the valid value")
                        {
                            const auto setOption = pSocket->getSocketOption(TcpSocket::SocketOption::ReceiveBufferSize);
                            REQUIRE(setOption == (2 * value));
                        }
                    }

                    THEN("connected peers can start exchanging data")
                    {
                        const auto dataToSend = GENERATE(AS(QByteArray),
                                                         QByteArray("a"),
                                                         QByteArray("abcdefgh"),
                                                         largeData);
                        const auto disableLowDelayOption = GENERATE(AS(bool), true, false);
                        const auto setKeepAliveOption = GENERATE(AS(bool), true, false);
                        const auto readBufferCapacity = GENERATE(AS(size_t), 0, 1024, 16384, 65536);
                        if (readBufferCapacity > 0)
                            pSocket->setReadBufferCapacity(readBufferCapacity);
                        if (disableLowDelayOption)
                            pSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, 0);
                        REQUIRE(pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay) == disableLowDelayOption ? 0 : 1);
                        if (setKeepAliveOption)
                        {
                            pPeerSocket->setSocketOption(TcpSocket::SocketOption::KeepAlive, 1);
                            pSocket->setSocketOption(TcpSocket::SocketOption::KeepAlive, 1);
                        }
                        REQUIRE(pPeerSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive) == setKeepAliveOption ? 1 : 0);
                        REQUIRE(pSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive) == setKeepAliveOption ? 1 : 0);

                        AND_WHEN("peer sends data to TcpSocket")
                        {
                            pPeerSocket->write(dataToSend.data(), dataToSend.size());

                            THEN("TcpSocket receives sent data")
                            {
                                const auto &sentData = socketReceivedData;
                                while (sentData != dataToSend)
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketReceivedDataFromPeerSemaphore, 1));
                                }

                                AND_WHEN("peer sends some more data to TcpSocket")
                                {
                                    socketReceivedData.clear();
                                    const QByteArray someMoreData("0123456789");
                                    pPeerSocket->write(someMoreData.data(), someMoreData.size());

                                    THEN("TcpSocket receives sent data")
                                    {
                                        while (sentData != someMoreData)
                                        {
                                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketReceivedDataFromPeerSemaphore, 1));
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("peer closes connection after sending data to TcpSocket")
                        {
                            pPeerSocket->write(dataToSend.data(), dataToSend.size());
                            pPeerSocket->disconnectFromPeer();

                            THEN("TcpSocket receives sent data")
                            {
                                const auto &sentData = socketReceivedData;
                                while (sentData != dataToSend)
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketReceivedDataFromPeerSemaphore, 1));
                                }

                                AND_THEN("both peer and TcpSocket emit disconnected")
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                                    REQUIRE(pPeerSocket->errorMessage().empty());
                                    REQUIRE(pSocket->errorMessage().empty());

                                    AND_WHEN("peer is deleted")
                                    {
                                        while (peerFailedSemaphore.tryAcquire()) {}
                                        pPeerSocket.reset(nullptr);

                                        THEN("peer does not emit any error")
                                        {
                                            REQUIRE(!peerFailedSemaphore.tryAcquire());
                                            REQUIRE(pSocket->errorMessage().empty());
                                        }
                                    }

                                    AND_WHEN("TcpSocket is deleted")
                                    {
                                        while (socketFailedSemaphore.tryAcquire()) {}
                                        pSocket.reset(nullptr);

                                        THEN("neither peer or TcpSocket emit any error")
                                        {
                                            REQUIRE(!peerFailedSemaphore.tryAcquire());
                                            REQUIRE(!socketFailedSemaphore.tryAcquire());
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("peer aborts after writing data")
                        {
                            pPeerSocket->write(dataToSend.data(), dataToSend.size());
                            pPeerSocket->abort();

                            THEN("TcpSocket emits disconnected")
                            {
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                                REQUIRE(pSocket->errorMessage().empty());
                                REQUIRE(pPeerSocket->errorMessage().empty());
                            }
                        }

                        AND_WHEN("peer is deleted after writing data")
                        {
                            pPeerSocket->write(dataToSend.data(), dataToSend.size());
                            pPeerSocket.reset(nullptr);

                            THEN("TcpSocket emits disconnected")
                            {
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                                REQUIRE(pSocket->errorMessage().empty());
                            }
                        }

                        AND_WHEN("TcpSocket sends data to peer")
                        {
                            pSocket->write(dataToSend.data(), dataToSend.size());

                            THEN("peer receives sent data")
                            {
                                const auto &sentData = peerReceivedData;
                                while (sentData != dataToSend)
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerReceivedDataFromTcpSocketSemaphore, 1));
                                }

                                AND_WHEN("TcpSocket sends some more data to peer")
                                {
                                    peerReceivedData.clear();
                                    const QByteArray someMoreData("0123456789");
                                    pSocket->write(someMoreData.data(), someMoreData.size());

                                    THEN("peer receives sent data")
                                    {
                                        while (sentData != someMoreData)
                                        {
                                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerReceivedDataFromTcpSocketSemaphore, 1));
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("TcpSocket closes connection after sending data to peer")
                        {
                            pSocket->write(dataToSend.data(), dataToSend.size());
                            pSocket->disconnectFromPeer();

                            THEN("peer receives sent data")
                            {
                                QByteArray sentData;
                                while (sentData != dataToSend)
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerReceivedDataFromTcpSocketSemaphore, 1));
                                    sentData = peerReceivedData;
                                }

                                AND_THEN("both sockets emit disconnected")
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                                    REQUIRE(pPeerSocket->errorMessage().empty());
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                                    REQUIRE(pSocket->errorMessage().empty());

                                    AND_WHEN("TcpSocket is deleted")
                                    {
                                        while (socketFailedSemaphore.tryAcquire()) {}
                                        pSocket.reset(nullptr);

                                        THEN("TcpSocket does not emit any error")
                                        {
                                            REQUIRE(pPeerSocket->errorMessage().empty());
                                            REQUIRE(!socketFailedSemaphore.tryAcquire());
                                        }
                                    }

                                    AND_WHEN("Peer is deleted")
                                    {
                                        while (peerFailedSemaphore.tryAcquire()) {}
                                        pPeerSocket.reset(nullptr);

                                        THEN("neither peer or TcpSocket emit any error")
                                        {
                                            REQUIRE(!peerFailedSemaphore.tryAcquire());
                                            REQUIRE(!socketFailedSemaphore.tryAcquire());
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("TcpSocket aborts after writing data")
                        {
                            pSocket->write(dataToSend.data(), dataToSend.size());
                            pSocket->abort();

                            THEN("TcpSocket aborts and Peer emits disconnected")
                            {
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                                REQUIRE(pSocket->errorMessage().empty());
                                REQUIRE(pPeerSocket->errorMessage().empty());
                            }
                        }

                        AND_WHEN("TcpSocket is deleted after writing data")
                        {
                            pSocket->write(dataToSend.data(), dataToSend.size());
                            pSocket.reset(nullptr);

                            THEN("TcpSocket aborts and Peer emits disconnected")
                            {
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                                REQUIRE(pPeerSocket->errorMessage().empty());
                            }
                        }
                    }

                    AND_WHEN("peer disconnects from TcpSocket")
                    {
                        pPeerSocket->disconnectFromPeer();

                        THEN("peer emits disconnected and TcpSocket emits disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                            REQUIRE(pPeerSocket->errorMessage().empty());
                            REQUIRE(pSocket->errorMessage().empty());

                            AND_WHEN("peer is deleted")
                            {
                                while (peerFailedSemaphore.tryAcquire()) {}
                                pPeerSocket.reset(nullptr);

                                THEN("peer does not emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(pSocket->errorMessage().empty());
                                }
                            }

                            AND_WHEN("TcpSocket is deleted")
                            {
                                while (socketFailedSemaphore.tryAcquire()) {}
                                pSocket.reset(nullptr);

                                THEN("neither peer or TcpSocket emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }
                        }
                    }

                    AND_WHEN("peer aborts connection")
                    {
                        pPeerSocket->abort();

                        THEN("TcpSocket emits disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                            REQUIRE(pPeerSocket->errorMessage().empty());
                            REQUIRE(pSocket->errorMessage().empty());

                            AND_WHEN("peer is deleted")
                            {
                                while (peerFailedSemaphore.tryAcquire()) {}
                                pPeerSocket.reset(nullptr);

                                THEN("peer does not emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(pSocket->errorMessage().empty());
                                }
                            }

                            AND_WHEN("TcpSocket is deleted")
                            {
                                while (socketFailedSemaphore.tryAcquire()) {}
                                pSocket.reset(nullptr);

                                THEN("neither peer or TcpSocket emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }
                        }
                    }

                    AND_WHEN("TcpSocket disconnects from TcpSocket")
                    {
                        pSocket->disconnectFromPeer();

                        THEN("both sockets emit disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(pPeerSocket->errorMessage().empty());
                            REQUIRE(pSocket->errorMessage().empty());

                            AND_WHEN("peer is deleted")
                            {
                                while (peerFailedSemaphore.tryAcquire()) {}
                                pPeerSocket.reset(nullptr);

                                THEN("neither peer or TcpSocket emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }

                            AND_WHEN("TcpSocket is deleted")
                            {
                                pSocket.reset(nullptr);

                                THEN("TcpSocket does not emit any error")
                                {
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                    REQUIRE(pPeerSocket->errorMessage().empty());
                                }
                            }
                        }
                    }

                    AND_WHEN("TcpSocket aborts connection")
                    {
                        pSocket->abort();

                        THEN("Peer emits disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(pPeerSocket->errorMessage().empty());
                            REQUIRE(pSocket->errorMessage().empty());

                            AND_WHEN("peer is deleted")
                            {
                                while (peerFailedSemaphore.tryAcquire()) {}
                                pPeerSocket.reset(nullptr);

                                THEN("neither peer or TcpSocket emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }

                            AND_WHEN("TcpSocket is deleted")
                            {
                                pSocket.reset(nullptr);

                                THEN("TcpSocket does not emit any error")
                                {
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                    REQUIRE(pPeerSocket->errorMessage().empty());
                                }
                            }
                        }
                    }

                    AND_WHEN("both peer and TcpSocket disconnects")
                    {
                        pPeerSocket->disconnectFromPeer();
                        pSocket->disconnectFromPeer();

                        THEN("both sockets emit disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                            REQUIRE(pPeerSocket->errorMessage().empty());
                            REQUIRE(pSocket->errorMessage().empty());

                            AND_WHEN("peer is deleted")
                            {
                                while (peerFailedSemaphore.tryAcquire()) {}
                                pPeerSocket.reset(nullptr);

                                THEN("TcpSocket not emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(pSocket->errorMessage().empty());
                                }
                            }

                            AND_WHEN("TcpSocket is deleted")
                            {
                                while (socketFailedSemaphore.tryAcquire()) {}
                                pSocket.reset(nullptr);

                                THEN("TcpSocket does not emit any error")
                                {
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }
                        }
                    }

                    AND_WHEN("both TcpSocket and peer disconnects")
                    {
                        pSocket->disconnectFromPeer();
                        pPeerSocket->disconnectFromPeer();

                        THEN("both sockets emit disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                            REQUIRE(pPeerSocket->errorMessage().empty());
                            REQUIRE(pSocket->errorMessage().empty());

                            AND_WHEN("peer is deleted")
                            {
                                while (peerFailedSemaphore.tryAcquire()) {}
                                pPeerSocket.reset(nullptr);

                                THEN("TcpSocket not emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(pSocket->errorMessage().empty());
                                }
                            }

                            AND_WHEN("TcpSocket is deleted")
                            {
                                while (socketFailedSemaphore.tryAcquire()) {}
                                pSocket.reset(nullptr);

                                THEN("TcpSocket does not emit any error")
                                {
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }
                        }
                    }

                    AND_WHEN("peer is deleted")
                    {
                        while (peerFailedSemaphore.tryAcquire()) {}
                        pPeerSocket.reset(nullptr);
                        QCoreApplication::processEvents();

                        THEN("peer does not emit any error and TcpSocket emits disconnected")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                            REQUIRE(pSocket->errorMessage().empty());
                            REQUIRE(!peerFailedSemaphore.tryAcquire());
                        }
                    }

                    AND_WHEN("TcpSocket is deleted")
                    {
                        while (socketFailedSemaphore.tryAcquire()) {}
                        pSocket.reset(nullptr);
                        QCoreApplication::processEvents();

                        THEN("peer socket emits error and disconnected signal")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(pPeerSocket->errorMessage().empty());
                            REQUIRE(!socketFailedSemaphore.tryAcquire());
                        }
                    }
                }
            }
        }
    }
}


SCENARIO("TcpSocket fails as expected")
{
    GIVEN("no server running on any IP related to test.onlocalhost.com")
    {
        WHEN("TcpSocket is connected to test.onlocalhost.com")
        {
            TcpSocket socket;
            QSemaphore socketFailedSemaphore;
            Object::connect(&socket, &TcpSocket::error, [&](){socketFailedSemaphore.release();});
            socket.connect("test.onlocalhost.com", 5000);

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketFailedSemaphore, 10));
                REQUIRE(socket.state() == TcpSocket::State::Unconnected);
                REQUIRE(socket.errorMessage().starts_with("Failed to connect to test.onlocalhost.com at"));
            }
        }
    }

    GIVEN("a non-existent domain")
    {
        std::string_view nonExistentDomain("nonexistentdomain.thisdomaindoesnotexist");

        WHEN("TcpSocket is connected to the non-existent domain")
        {
            TcpSocket socket;
            QSemaphore socketFailedSemaphore;
            Object::connect(&socket, &TcpSocket::error, [&](){socketFailedSemaphore.release();});
            socket.connect(nonExistentDomain, 5000);

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketFailedSemaphore, 10));
                REQUIRE(socket.state() == TcpSocket::State::Unconnected);
                REQUIRE(socket.errorMessage() == "Failed to connect to nonexistentdomain.thisdomaindoesnotexist. Could not fetch any address for domain.");
            }
        }
    }

    GIVEN("a server running on IPV6 localhost")
    {
        QTcpServer server;
        REQUIRE(server.listen(QHostAddress::LocalHostIPv6));
        QObject::connect(&server, &QTcpServer::newConnection, [](){FAIL("This code is supposed to be unreachable.");});

        WHEN("a TcpSocket bounded to a IPV4 address is connected to the IPV6 server")
        {
            TcpSocket socket;
            QSemaphore socketFailedSemaphore;
            Object::connect(&socket, &TcpSocket::error, [&](){socketFailedSemaphore.release();});
            socket.setBindAddressAndPort("127.2.2.5");
            socket.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketFailedSemaphore, 10));
                REQUIRE(socket.state() == TcpSocket::State::Unconnected);
                REQUIRE(socket.errorMessage().starts_with("Failed to connect to [::1]:"));
            }
        }

        WHEN("TcpSocket bounded to a privileged port on IPV6 is connected to the server")
        {
            TcpSocket socket;
            QSemaphore socketFailedSemaphore;
            Object::connect(&socket, &TcpSocket::error, [&](){socketFailedSemaphore.release();});
            socket.setBindAddressAndPort("::1", 443);
            socket.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketFailedSemaphore, 10));
                REQUIRE(socket.state() == TcpSocket::State::Unconnected);
                REQUIRE(socket.errorMessage() == "Failed to bind socket to [::1]:443. POSIX error EACCES(13): Permission denied.");
            }
        }
    }

    GIVEN("a server running on IPV4 localhost")
    {
        QTcpServer server;
        REQUIRE(server.listen(QHostAddress("127.18.28.38")));
        size_t connectionCount = 0;
        QObject::connect(&server, &QTcpServer::newConnection, [&](){while (server.nextPendingConnection() != nullptr) {++connectionCount;}});

        WHEN("a TcpSocket bounded to a IPV6 address is connected to the IPV4 server")
        {
            TcpSocket socket;
            QSemaphore socketFailedSemaphore;
            Object::connect(&socket, &TcpSocket::error, [&](){socketFailedSemaphore.release();});
            socket.setBindAddressAndPort("::1");
            socket.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketFailedSemaphore, 10));
                REQUIRE(socket.state() == TcpSocket::State::Unconnected);
                REQUIRE(socket.errorMessage().starts_with("Failed to connect to 127.18.28.38:"));
                REQUIRE(connectionCount == 0);
            }
        }

        WHEN("TcpSocket bound to a privileged port on IPV4 is connected to server")
        {
            TcpSocket socket;
            QSemaphore socketFailedSemaphore;
            Object::connect(&socket, &TcpSocket::error, [&](){socketFailedSemaphore.release();});
            socket.setBindAddressAndPort("127.0.0.1", 443);
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
            QTcpSocket previouslyConnectedSocket;
            QSemaphore previouslyConnectedSocketSemaphore;
            QObject::connect(&previouslyConnectedSocket, &QTcpSocket::connected, [&](){previouslyConnectedSocketSemaphore.release();});
            previouslyConnectedSocket.connectToHost(server.serverAddress(), server.serverPort());
            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(previouslyConnectedSocketSemaphore, 10));
            TcpSocket socket;
            QSemaphore socketFailedSemaphore;
            Object::connect(&socket, &TcpSocket::error, [&](){socketFailedSemaphore.release();});
            socket.setBindAddressAndPort(previouslyConnectedSocket.localAddress().toString().toStdString(), previouslyConnectedSocket.localPort());
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

            THEN("socket is created as unconnected")
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

            THEN("socket is created as unconnected")
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

            THEN("socket is created as unconnected")
            {
                REQUIRE(socket.state() == TcpSocket::State::Unconnected);
            }
        }
    }

    GIVEN("a server that does not accept new connections")
    {
        QTcpServer server;
        static constexpr int backlogSize = 128;
        server.setListenBacklogSize(backlogSize);
        QObject::connect(&server, &QTcpServer::newConnection, [](){FAIL("This code is supposed to be unreachable.");});
        REQUIRE(server.listen(QHostAddress("127.10.20.82")));
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
            isServerAcceptingConnections = SemaphoreAwaiter::signalSlotAwareWait(connectedSemaphore, 1);
        }
        while (isServerAcceptingConnections);
        sockets.front()->abort();

        WHEN("a TcpSocket tries to connect to server")
        {
            TcpSocket socket;
            QSemaphore socketFailedSemaphore;
            Object::connect(&socket, &TcpSocket::error, [&]() {socketFailedSemaphore.release();});
            socket.connect("127.10.20.82", server.serverPort());

            THEN("TcpSocket times out while trying to connect to server")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketFailedSemaphore, 70));
                REQUIRE(socket.state() == TcpSocket::State::Unconnected);
                std::string expectedErrorMessage("Failed to connect to 127.10.20.82:");
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
        QTcpServer server;
        std::unique_ptr<QTcpSocket> pPeerSocket;
        QObject::connect(&server, &QTcpServer::newConnection, [&]()
        {
            REQUIRE(server.hasPendingConnections());
            REQUIRE(!pPeerSocket);
            pPeerSocket.reset(server.nextPendingConnection());
            REQUIRE(pPeerSocket.get() != nullptr);
            pPeerSocket->setParent(nullptr);
            REQUIRE(!server.hasPendingConnections());
            QObject::connect(pPeerSocket.get(), &QTcpSocket::errorOccurred, [&]() {peerFailedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QTcpSocket::disconnected, [&](){peerDisconnectedSemaphore.release();});
            peerConnectedSemaphore.release();
        });
        REQUIRE(server.listen(QHostAddress("127.11.22.44")));
        QSemaphore socketConnectedSemaphore;
        QSemaphore socketDisconnectedSemaphore;
        QSemaphore socketFailedSemaphore;
        std::unique_ptr<TcpSocket> pSocket(new TcpSocket);
        Object::connect(pSocket.get(), &TcpSocket::connected, [&]{socketConnectedSemaphore.release();});
        Object::connect(pSocket.get(), &TcpSocket::disconnected, [&]{socketDisconnectedSemaphore.release();});
        Object::connect(pSocket.get(), &TcpSocket::error, [&]{socketFailedSemaphore.release();});

        WHEN("TcpSocket connects to server and disconnects while handling the connected signal")
        {
            Object::connect(pSocket.get(), &TcpSocket::connected, [&](){pSocket->disconnectFromPeer();});
            pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("TcpSocket disconnects from peer")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
            }
        }

        WHEN("TcpSocket connects to server and aborts connection while handling the connected signal")
        {
            Object::connect(pSocket.get(), &TcpSocket::connected, [&](){pSocket->abort();});
            pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("TcpSocket disconnects from peer")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                REQUIRE(pSocket->state() == TcpSocket::State::Unconnected);
            }
        }

        WHEN("TcpSocket connects to server and is destroyed while handling the connected signal")
        {
            Object::connect(pSocket.get(), &TcpSocket::connected, [&](){pSocket.release()->scheduleForDeletion();});
            pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("TcpSocket disconnects from peer")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
            }
        }

        WHEN("TcpSocket connects to server and connects again while handling the connected signal")
        {
            auto *pCtxObject = new Object;
            Object::connect(pSocket.get(), &TcpSocket::connected, pCtxObject, [&]()
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                QObject::connect(pPeerSocket.get(), &QTcpSocket::disconnected, pPeerSocket.get(), &QObject::deleteLater);
                pPeerSocket.release();
                pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());
                delete pCtxObject;
            });
            pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("TcpSocket connects, aborts and then reconnects to peer")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
            }
        }

        WHEN("TcpSocket connects to server and connects to a non-existent server address while handling the connected signal")
        {
            Object::connect(pSocket.get(), &TcpSocket::connected, [&]()
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                QObject::connect(pPeerSocket.get(), &QTcpSocket::disconnected, pPeerSocket.get(), &QObject::deleteLater);
                pPeerSocket.release();
                pSocket->abort();
                auto const serverAddress = QHostAddress("127.1.2.3");
                QTcpSocket socket;
                REQUIRE(socket.bind(serverAddress));
                const auto unusedPortForNow = socket.localPort();
                socket.abort();
                pSocket->connect(serverAddress.toString().toStdString(), unusedPortForNow);
            });
            pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("TcpSocket connects, aborts and fails to connect to the non-existent server")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketFailedSemaphore, 10));
                REQUIRE(pSocket->errorMessage().starts_with("Failed to connect to 127.1.2.3:"));
            }
        }

        WHEN("TcpSocket connects to server")
        {
            pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());
            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
            REQUIRE(!socketDisconnectedSemaphore.tryAcquire());
            REQUIRE(!socketFailedSemaphore.tryAcquire());

            THEN("connected peers can start exchanging data")
            {
                AND_WHEN("connected peer sends some data to TcpSocket")
                {
                    pPeerSocket->write("abcdefgh");

                    AND_WHEN("TcpSocket disconnects while handling the receivedData signal")
                    {
                        Object::connect(pSocket.get(), &TcpSocket::receivedData, [&](){pSocket->disconnectFromPeer();});

                        THEN("TcpSocket disconnects from peer")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                        }
                    }


                    AND_WHEN("TcpSocket aborts connection while handling the receivedData signal")
                    {
                        REQUIRE(!peerDisconnectedSemaphore.tryAcquire());
                        Object::connect(pSocket.get(), &TcpSocket::receivedData, [&](){pSocket->abort();});

                        THEN("TcpSocket disconnects from peer")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(pSocket->state() == TcpSocket::State::Unconnected);
                        }
                    }

                    AND_WHEN("TcpSocket is destroyed while handling the receivedData signal")
                    {
                        REQUIRE(!peerDisconnectedSemaphore.tryAcquire());
                        Object::connect(pSocket.get(), &TcpSocket::receivedData, [&](){pSocket.release()->scheduleForDeletion();});

                        THEN("TcpSocket disconnects from peer")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                        }
                    }

                    AND_WHEN("TcpSocket is reconnected while handling the receivedData signal")
                    {
                        QObject::connect(pPeerSocket.get(), &QTcpSocket::disconnected, pPeerSocket.get(), &QObject::deleteLater);
                        pPeerSocket.release();

                        Object::connect(pSocket.get(), &TcpSocket::receivedData, [&]()
                        {
                            pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());
                        });

                        THEN("TcpSocket aborts and then reconnects")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                        }
                    }
                }

                AND_WHEN("TcpSocket sends more data than the socket's send buffer can store")
                {
                    const auto socketSendBufferSize = pSocket->getSocketOption(TcpSocket::SocketOption::SendBufferSize);
                    REQUIRE(socketSendBufferSize > 1);
                    QByteArray dataToSend(3 * socketSendBufferSize, ' ');
                    for (auto &ch : dataToSend)
                        ch = QRandomGenerator64::global()->bounded(0, 256);
                    pSocket->write(dataToSend.data(), dataToSend.size());

                    AND_WHEN("TcpSocket disconnects while handling the sentData signal with data still to be written")
                    {
                        Object::connect(pSocket.get(), &TcpSocket::sentData, [&]()
                        {
                            if (pSocket->dataToWrite())
                                pSocket->disconnectFromPeer();
                        });

                        THEN("TcpSocket disconnects from peer")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                        }
                    }

                    AND_WHEN("TcpSocket disconnects while handling the sentData signal with no more data still to be written")
                    {
                        Object::connect(pSocket.get(), &TcpSocket::sentData, [&]()
                        {
                            if (!pSocket->dataToWrite())
                                pSocket->disconnectFromPeer();
                        });

                        THEN("TcpSocket disconnects from peer")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                        }
                    }

                    AND_WHEN("TcpSocket aborts connection while handling the sentData signal with data still to be written")
                    {
                        Object::connect(pSocket.get(), &TcpSocket::sentData, [&]()
                        {
                            if (pSocket->dataToWrite())
                                pSocket->abort();
                        });

                        THEN("TcpSocket disconnects from peer")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(pSocket->state() == TcpSocket::State::Unconnected);
                        }
                    }

                    AND_WHEN("TcpSocket aborts connection while handling the sentData signal with no more data data still to be written")
                    {
                        Object::connect(pSocket.get(), &TcpSocket::sentData, [&]()
                        {
                            if (!pSocket->dataToWrite())
                                pSocket->abort();
                        });

                        THEN("TcpSocket disconnects from peer")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(pSocket->state() == TcpSocket::State::Unconnected);
                        }
                    }

                    AND_WHEN("TcpSocket is destroyed while handling the sentData signal with data still to be written")
                    {
                        Object::connect(pSocket.get(), &TcpSocket::sentData, [&]()
                        {
                            if (pSocket->dataToWrite())
                                pSocket.release()->scheduleForDeletion();
                        });

                        THEN("TcpSocket disconnects from peer")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                        }
                    }

                    AND_WHEN("TcpSocket is destroyed while handling the sentData signal with no more data still to be written")
                    {
                        Object::connect(pSocket.get(), &TcpSocket::sentData, [&]()
                        {
                            if (!pSocket->dataToWrite())
                                pSocket.release()->scheduleForDeletion();
                        });

                        THEN("TcpSocket disconnects from peer")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                        }
                    }

                    AND_WHEN("TcpSocket is reconnected while handling the sentData signal with data still to be written")
                    {
                        QObject::connect(pPeerSocket.get(), &QTcpSocket::disconnected, pPeerSocket.get(), &QObject::deleteLater);
                        pPeerSocket.release();

                        Object::connect(pSocket.get(), &TcpSocket::sentData, [&]()
                        {
                            if (pSocket->dataToWrite())
                            {
                                pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());
                            }
                        });

                        THEN("TcpSocket reconnects after disconnecting")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));

                        }
                    }

                    AND_WHEN("TcpSocket is reconnected while handling the sentData signal with no more data still to be written")
                    {
                        QObject::connect(pPeerSocket.get(), &QTcpSocket::disconnected, pPeerSocket.get(), &QObject::deleteLater);
                        pPeerSocket.release();

                        Object::connect(pSocket.get(), &TcpSocket::sentData, [&]()
                        {
                            if (!pSocket->dataToWrite())
                            {
                                pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());
                            }
                        });

                        THEN("TcpSocket reconnects after disconnecting")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                        }
                    }
                }
            }

            AND_WHEN("connected peer disconnects")
            {
                pPeerSocket->disconnectFromHost();
                QSemaphore socketDisconnectedFromPeerSemaphore;

                AND_WHEN("TcpSocket is disconnected while handling the disconnected signal")
                {
                    Object::connect(pSocket.get(), &TcpSocket::disconnected, [&]()
                    {
                        pSocket->disconnectFromPeer();
                        socketDisconnectedFromPeerSemaphore.release();
                    });

                    THEN("no exception is thrown")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedFromPeerSemaphore, 10));
                    }
                }

                AND_WHEN("TcpSocket aborts connection while handling the disconnected signal")
                {
                    Object::connect(pSocket.get(), &TcpSocket::disconnected, [&]()
                    {
                        pSocket->abort();
                        socketDisconnectedFromPeerSemaphore.release();
                    });

                    THEN("no exception is thrown")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedFromPeerSemaphore, 10));
                    }
                }

                AND_WHEN("TcpSocket is destroyed while handling the disconnected signal")
                {
                    Object::connect(pSocket.get(), &TcpSocket::disconnected, [&]()
                    {
                        pSocket.release()->scheduleForDeletion();
                        socketDisconnectedFromPeerSemaphore.release();
                    });

                    THEN("no exception is thrown")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedFromPeerSemaphore, 10));
                    }
                }

                AND_WHEN("TcpSocket is reconnected while handling the disconnected signal")
                {
                    pPeerSocket.release()->deleteLater();
                    Object::connect(pSocket.get(), &TcpSocket::disconnected, [&]()
                    {
                        socketDisconnectedFromPeerSemaphore.release();
                        pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());
                    });

                    THEN("TcpSocket disconnects and then reconnects")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedFromPeerSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                    }
                }
            }
        }

        WHEN("TcpSocket tries to connect to a non-existent server by address")
        {
            auto const serverAddress = QHostAddress("127.1.2.3");
            QTcpSocket socket;
            REQUIRE(socket.bind(serverAddress));
            const auto unusedPortForNow = socket.localPort();
            socket.abort();
            pSocket->connect(serverAddress.toString().toStdString(), unusedPortForNow);
            QSemaphore socketHandledErrorSemaphore;

            AND_WHEN("TcpSocket is disconnected while handling the error signal")
            {
                Object::connect(pSocket.get(), &TcpSocket::error, [&]()
                {
                    REQUIRE(!pSocket->errorMessage().empty());
                    pSocket->disconnectFromPeer();
                    socketHandledErrorSemaphore.release();
                });

                THEN("no exception is thrown")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                }
            }

            AND_WHEN("TcpSocket aborts connection while handling the error signal")
            {
                Object::connect(pSocket.get(), &TcpSocket::error, [&]()
                {
                    REQUIRE(!pSocket->errorMessage().empty());
                    pSocket->abort();
                    socketHandledErrorSemaphore.release();
                });

                THEN("no exception is thrown")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                }
            }

            AND_WHEN("TcpSocket is destroyed while handling the error signal")
            {
                Object::connect(pSocket.get(), &TcpSocket::error, [&]()
                {
                    REQUIRE(!pSocket->errorMessage().empty());
                    pSocket.release()->scheduleForDeletion();
                    socketHandledErrorSemaphore.release();
                });

                THEN("no exception is thrown")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                }
            }

            AND_WHEN("TcpSocket is reconnected to the running server while handling the error signal")
            {
                Object::connect(pSocket.get(), &TcpSocket::error, [&]()
                {
                    REQUIRE(!pSocket->errorMessage().empty());
                    REQUIRE(!socketConnectedSemaphore.tryAcquire());
                    pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());
                    socketHandledErrorSemaphore.release();
                });

                THEN("TcpSocket reconnects after disconnecting")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                }
            }
        }

        WHEN("TcpSocket tries to connect to a non-existent server by name")
        {
            pSocket->connect("This.domain.name.does.not.exist.for.sure", 3008);
            QSemaphore socketHandledErrorSemaphore;

            AND_WHEN("TcpSocket is disconnected while handling the error signal")
            {
                Object::connect(pSocket.get(), &TcpSocket::error, [&]()
                {
                    REQUIRE(!pSocket->errorMessage().empty());
                    pSocket->disconnectFromPeer();
                    socketHandledErrorSemaphore.release();
                });

                THEN("no exception is thrown")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                }
            }

            AND_WHEN("TcpSocket aborts connection while handling the error signal")
            {
                Object::connect(pSocket.get(), &TcpSocket::error, [&]()
                {
                    REQUIRE(!pSocket->errorMessage().empty());
                    pSocket->abort();
                    socketHandledErrorSemaphore.release();
                });

                THEN("no exception is thrown")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                }
            }

            AND_WHEN("TcpSocket is destroyed while handling the error signal")
            {
                Object::connect(pSocket.get(), &TcpSocket::error, [&]()
                {
                    REQUIRE(!pSocket->errorMessage().empty());
                    pSocket.release()->scheduleForDeletion();
                    socketHandledErrorSemaphore.release();
                });

                THEN("no exception is thrown")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                }
            }

            AND_WHEN("TcpSocket is reconnected to the running server while handling the error signal")
            {
                Object::connect(pSocket.get(), &TcpSocket::error, [&]()
                {
                    REQUIRE(!pSocket->errorMessage().empty());
                    REQUIRE(!socketConnectedSemaphore.tryAcquire());
                    pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());
                    socketHandledErrorSemaphore.release();
                });

                THEN("TcpSocket reconnects after aborting")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                }
            }
        }

        WHEN("TcpSocket tries to connect to test.onlocalhost.com without any server running")
        {
            pSocket->connect("test.onlocalhost.com", 3008);
            QSemaphore socketHandledErrorSemaphore;

            AND_WHEN("TcpSocket is disconnected while handling the error signal")
            {
                Object::connect(pSocket.get(), &TcpSocket::error, [&]()
                {
                    REQUIRE(!pSocket->errorMessage().empty());
                    pSocket->disconnectFromPeer();
                    socketHandledErrorSemaphore.release();
                });

                THEN("no exception is thrown")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                }
            }

            AND_WHEN("TcpSocket aborts connection while handling the error signal")
            {
                Object::connect(pSocket.get(), &TcpSocket::error, [&]()
                {
                    REQUIRE(!pSocket->errorMessage().empty());
                    pSocket->abort();
                    socketHandledErrorSemaphore.release();
                });

                THEN("no exception is thrown")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                }
            }

            AND_WHEN("TcpSocket is destroyed while handling the error signal")
            {
                Object::connect(pSocket.get(), &TcpSocket::error, [&]()
                {
                    REQUIRE(!pSocket->errorMessage().empty());
                    pSocket.release()->scheduleForDeletion();
                    socketHandledErrorSemaphore.release();
                });

                THEN("no exception is thrown")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                }
            }

            AND_WHEN("TcpSocket is reconnected to the running server while handling the error signal")
            {
                Object::connect(pSocket.get(), &TcpSocket::error, [&]()
                {
                    REQUIRE(!pSocket->errorMessage().empty());
                    REQUIRE(!socketConnectedSemaphore.tryAcquire());
                    pSocket->connect(server.serverAddress().toString().toStdString(), server.serverPort());
                    socketHandledErrorSemaphore.release();
                });

                THEN("TcpSocket reconnects after aborting")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                }
            }
        }
    }
}


SCENARIO("TcpSockets can be reused")
{
    GIVEN("a QTcpServer listening for connections")
    {
        QTcpServer server;
        QSemaphore peerConnectedSemaphore;
        QSemaphore peerFailedSemaphore;
        QSemaphore peerDisconnectedSemaphore;
        QSemaphore peerReceivedDataFromTcpSocketSemaphore;
        QObject::connect(&server, &QTcpServer::newConnection, &server, [&]()
        {
            REQUIRE(server.hasPendingConnections());
            auto *pPeerSocket = server.nextPendingConnection();
            REQUIRE(pPeerSocket != nullptr);
            REQUIRE(!server.hasPendingConnections());
            QObject::connect(pPeerSocket, &QTcpSocket::errorOccurred, [&](QAbstractSocket::SocketError error) {peerFailedSemaphore.release();});
            QObject::connect(pPeerSocket, &QTcpSocket::disconnected, [&](){peerDisconnectedSemaphore.release();});
            QObject::connect(pPeerSocket, &QTcpSocket::readyRead, [pPeerSocket, &peerReceivedDataFromTcpSocketSemaphore]()
            {
                if (pPeerSocket->bytesAvailable() != 6)
                    return;
                const auto receivedData = pPeerSocket->readAll();
                if (receivedData == "PING\r\n")
                    pPeerSocket->write("PONG\r\n");
                else if (receivedData == "QUIT\r\n")
                    pPeerSocket->disconnectFromHost();
                else
                {
                    FAIL("This code is supposed to be unreachable");
                }
                peerReceivedDataFromTcpSocketSemaphore.release();
            });
            peerConnectedSemaphore.release();
        });
        REQUIRE(server.listen(QHostAddress::LocalHost));
        const auto serverPort = server.serverPort();
        REQUIRE(serverPort >= 1024);

        WHEN("TcpSocket connects to server and play ping pong game three times")
        {
            static constexpr int repCount = 3;
            static constexpr int pingCount = 31;
            QSemaphore socketConnectedSemaphore;
            QSemaphore socketFailedSemaphore;
            QSemaphore socketDisconnectedSemaphore;
            QSemaphore socketReceivedDataFromPeerSemaphore;
            int currentPingCount = 0;
            std::unique_ptr<TcpSocket> pSocket(new TcpSocket);
            Object::connect(pSocket.get(), &TcpSocket::error, [&]() {socketFailedSemaphore.release();});
            Object::connect(pSocket.get(), &TcpSocket::connected, [&]()
            {
                ++currentPingCount;
                pSocket->write("PING\r\n");
                socketConnectedSemaphore.release();
            });
            Object::connect(pSocket.get(), &TcpSocket::disconnected, [&]()
            {
                currentPingCount = 0;
                socketDisconnectedSemaphore.release();
            });
            Object::connect(pSocket.get(), &TcpSocket::receivedData, [&]()
            {
                std::string_view expectedData("PONG\r\n");
                if (pSocket->dataAvailable() != expectedData.size())
                    return;
                REQUIRE(pSocket->readAll() == expectedData);
                if (++currentPingCount <= pingCount)
                    pSocket->write("PING\r\n");
                else
                    pSocket->write("QUIT\r\n");
                socketReceivedDataFromPeerSemaphore.release();
            });
            for (auto i = 0; i < repCount; ++i)
            {
                pSocket->connect(server.serverAddress().toString().toStdString(), serverPort);
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
            }

            THEN("sockets exchange messages as expected")
            {
                const int tcpSocketReceivedDataSemaphoreReleaseCount = repCount * pingCount;
                REQUIRE(socketReceivedDataFromPeerSemaphore.tryAcquire(tcpSocketReceivedDataSemaphoreReleaseCount));
                REQUIRE(!socketReceivedDataFromPeerSemaphore.tryAcquire(1));
            }
        }
    }

    GIVEN("a TcpServer listening for connections")
    {
        TcpServer server;
        QSemaphore peerConnectedSemaphore;
        QSemaphore peerDisconnectedSemaphore;
        QSemaphore peerReceivedDataFromTcpSocketSemaphore;
        std::unique_ptr<TcpSocket> pPeerSocket;
        Object::connect(&server, &TcpServer::newConnection, [&](TcpSocket *pNewSocket)
        {
            pPeerSocket.reset(pNewSocket);
            Object::connect(pPeerSocket.get(), &TcpSocket::receivedData, [&]()
            {
                if (pPeerSocket->dataAvailable() != 6)
                    return;
                const auto receivedData = pPeerSocket->readAll();
                if (receivedData == "PING\r\n")
                    pPeerSocket->write("PONG\r\n");
                else if (receivedData == "QUIT\r\n")
                    pPeerSocket->disconnectFromPeer();
                else
                {
                    FAIL("This code is supposed to be unreachable");
                }
                peerReceivedDataFromTcpSocketSemaphore.release();
            });
            Object::connect(pPeerSocket.get(), &TcpSocket::disconnected, [&](){peerDisconnectedSemaphore.release();});
            Object::connect(pPeerSocket.get(), &TcpSocket::error, [](){FAIL("This code is supposed to be unreachable.");});
            peerConnectedSemaphore.release();
        });
        REQUIRE(server.listen(QHostAddress("127.0.0.1"), 0));
        const auto serverPort = server.serverPort();
        REQUIRE(serverPort >= 1024);

        WHEN("TcpSocket connects to server and play ping pong game three times")
        {
            static constexpr int repCount = 3;
            static constexpr int pingCount = 31;
            QSemaphore socketConnectedSemaphore;
            QSemaphore socketDisconnectedSemaphore;
            QSemaphore socketReceivedDataFromPeerSemaphore;
            int currentPingCount = 0;
            std::unique_ptr<TcpSocket> pSocket(new TcpSocket);
            Object::connect(pSocket.get(), &TcpSocket::error, [&]() {FAIL("This code is supposed to be unreachable.");});
            Object::connect(pSocket.get(), &TcpSocket::connected, [&]()
            {
                ++currentPingCount;
                pSocket->write("PING\r\n");
                socketConnectedSemaphore.release();
            });
            Object::connect(pSocket.get(), &TcpSocket::disconnected, [&]()
            {
                currentPingCount = 0;
                socketDisconnectedSemaphore.release();
            });
            Object::connect(pSocket.get(), &TcpSocket::receivedData, [&]()
            {
                std::string_view expectedData("PONG\r\n");
                if (pSocket->dataAvailable() != expectedData.size())
                    return;
                REQUIRE(pSocket->readAll() == expectedData);
                if (++currentPingCount <= pingCount)
                    pSocket->write("PING\r\n");
                else
                    pSocket->write("QUIT\r\n");
                socketReceivedDataFromPeerSemaphore.release();
            });
            for (auto i = 0; i < repCount; ++i)
            {
                pSocket->connect(server.serverAddress().toString().toStdString(), serverPort);
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
            }

            THEN("sockets exchange messages as expected")
            {
                const int tcpSocketReceivedDataSemaphoreReleaseCount = repCount * pingCount;
                REQUIRE(socketReceivedDataFromPeerSemaphore.tryAcquire(tcpSocketReceivedDataSemaphoreReleaseCount));
                REQUIRE(!socketReceivedDataFromPeerSemaphore.tryAcquire(1));
            }
        }

        WHEN("TcpSocket connects and then disconnects from server")
        {
            QSemaphore socketConnectedSemaphore;
            QSemaphore socketDisconnectedSemaphore;
            QSemaphore socketReceivedDataFromPeerSemaphore;
            TcpSocket socket;
            Object::connect(&socket, &TcpSocket::error, [&]() {FAIL("This code is supposed to be unreachable.");});
            Object::connect(&socket, &TcpSocket::connected, [&](){socketConnectedSemaphore.release(); socket.disconnectFromPeer();});
            Object::connect(&socket, &TcpSocket::disconnected, [&](){socketDisconnectedSemaphore.release();});
            socket.connect(server.serverAddress().toString().toStdString(), serverPort);

            THEN("socket connects and then disconnects")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));

                AND_WHEN("we use server TcpSocket as client to connect to server and play ping pong game three times")
                {
                    static constexpr int repCount = 3;
                    static constexpr int pingCount = 31;
                    int currentPingCount = 0;
                    std::unique_ptr<TcpSocket> pSocket(pPeerSocket.release());
                    REQUIRE(pSocket.get() != nullptr);
                    Object::connect(pSocket.get(), &TcpSocket::error, [&]() {FAIL("This code is supposed to be unreachable.");});
                    Object::connect(pSocket.get(), &TcpSocket::connected, [&]()
                    {
                        ++currentPingCount;
                        pSocket->write("PING\r\n");
                        socketConnectedSemaphore.release();
                    });
                    Object::connect(pSocket.get(), &TcpSocket::disconnected, [&]()
                    {
                        currentPingCount = 0;
                        socketDisconnectedSemaphore.release();
                    });
                    Object::connect(pSocket.get(), &TcpSocket::receivedData, [&]()
                    {
                        std::string_view expectedData("PONG\r\n");
                        if (pSocket->dataAvailable() != expectedData.size())
                            return;
                        REQUIRE(pSocket->readAll() == expectedData);
                        if (++currentPingCount <= pingCount)
                            pSocket->write("PING\r\n");
                        else
                            pSocket->write("QUIT\r\n");
                        socketReceivedDataFromPeerSemaphore.release();
                    });
                    for (auto i = 0; i < repCount; ++i)
                    {
                        pSocket->connect(server.serverAddress().toString().toStdString(), serverPort);
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                    }

                    THEN("sockets exchange messages as expected")
                    {
                        const int tcpSocketReceivedDataSemaphoreReleaseCount = repCount * pingCount;
                        REQUIRE(socketReceivedDataFromPeerSemaphore.tryAcquire(tcpSocketReceivedDataSemaphoreReleaseCount));
                        REQUIRE(!socketReceivedDataFromPeerSemaphore.tryAcquire(1));
                    }
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
        m_sockets.resize(m_totalConnections);
        for (auto *&pSocket : m_sockets)
            pSocket = new TcpSocket;
    }
    ~ClientTcpSockets() override = default;

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
            pSocket->disconnectFromPeer();
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
            Object::connect(pSocket, &TcpSocket::connected, [this]()
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
            Object::connect(pSocket, &TcpSocket::receivedData, [this, pSocket]()
            {
                if (pSocket->dataAvailable() != (m_requestsPerWorkingConnection * sizeof(int)))
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
            Object::connect(pSocket, &TcpSocket::disconnected, [this, pSocket]()
            {
                REQUIRE(m_hasConnectedAllClients);
                pSocket->scheduleForDeletion();
                if (++m_disconnectionCount == m_totalConnections)
                    emit disconnectedFromServer();
            });
            Object::connect(pSocket, &TcpSocket::error, [this, pSocket]()
            {
                REQUIRE(!m_hasConnectedAllClients);
                // binding failed
                REQUIRE(m_currentBindPort < 65534);
                pSocket->setBindAddressAndPort(m_bindAddress, ++m_currentBindPort);
                pSocket->connect(m_serverAddress, m_serverPort);
            });
            REQUIRE(m_currentBindPort < 65534);
            pSocket->setBindAddressAndPort(m_bindAddress, ++m_currentBindPort);
            pSocket->connect(m_serverAddress, m_serverPort);
        }
    }

private:
    size_t m_connectionCount = 0;
    size_t m_responseCount = 0;
    size_t m_disconnectionCount = 0;
    std::vector<TcpSocket*> m_sockets;
    size_t m_currentConnectIndex = 0;
    size_t m_batchConnectionCount = 0;
    const size_t m_connectionsPerBatch = 250;
    std::string m_serverAddress;
    std::string m_bindAddress;
    uint16_t m_currentBindPort = 1024;
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
                if (pSocket->dataAvailable() != (2 * m_requestsPerWorkingConnection * sizeof(int)))
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


SCENARIO("TcpSocket benchmarks")
{
    static constexpr std::string_view serverAddress("127.25.24.20");
    static constexpr size_t totalConnectionsPerThread = 15000;
    static constexpr size_t workingConnectionsPerThread = 10000;
    static constexpr size_t clientThreadCount = 5;
    static constexpr size_t totalConnections = totalConnectionsPerThread * clientThreadCount;
    static constexpr size_t requestsPerWorkingConnection = 1000;
    static constexpr int a = 5;
    static constexpr int b = 3;
    size_t memoryConsumedAfterCreatingClientSockets = 0;
    size_t memoryConsumedAfterConnecting = 0;
    size_t memoryConsumedAfterResponses = 0;
    size_t memoryConsumedAfterDisconnecting = 0;
    QElapsedTimer elapsedTimer;
    double connectionsPerSecond = 0;
    double requestsPerSecond = 0;
    double disconnectionsPerSecond = 0;
    std::atomic_size_t connectedClientCount = 0;
    std::atomic_size_t receivedResponseCount = 0;
    std::atomic_size_t disconnectedClientCount = 0;
    QSemaphore clientSocketsDisconnectedSemaphore;
    QSemaphore serverSocketsConnectedSemaphore;
    QSemaphore serverSocketsDisconnectedSemaphore;
    std::unique_ptr<AsyncQObject<ServerTcpSockets, std::string_view, size_t, size_t>> server(new AsyncQObject<ServerTcpSockets, std::string_view, size_t, size_t>(serverAddress, totalConnections, requestsPerWorkingConnection));
    const auto serverPort = server->get()->serverPort();
    QObject::connect(server->get(), &ServerTcpSockets::connectedToClients, [&](){serverSocketsConnectedSemaphore.release();});
    QObject::connect(server->get(), &ServerTcpSockets::disconnectedFromClients, [&](){serverSocketsDisconnectedSemaphore.release();});
    std::vector<std::unique_ptr<AsyncQObject<ClientTcpSockets, std::string_view, uint16_t, std::string_view, size_t, size_t, size_t, int, int>>> clients(clientThreadCount);
    size_t counter = 0;
    for (auto &client : clients)
    {
        std::string currentBindAddress("127.25.2.");
        currentBindAddress.append(std::to_string(++counter));
        client.reset(new AsyncQObject<ClientTcpSockets, std::string_view, uint16_t, std::string_view, size_t, size_t, size_t, int, int>(serverAddress, serverPort, currentBindAddress, totalConnectionsPerThread, workingConnectionsPerThread, requestsPerWorkingConnection, a, b));
    }
    memoryConsumedAfterCreatingClientSockets = getUsedMemory();
    QObject ctxObject;
    for (auto &client : clients)
    {
        QObject::connect(client->get(), &ClientTcpSockets::connectedToServer, &ctxObject, [&]()
        {
            if (++connectedClientCount == clientThreadCount)
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverSocketsConnectedSemaphore, 10000));
                connectionsPerSecond = (1000.0 * totalConnections)/elapsedTimer.elapsed();
                memoryConsumedAfterConnecting = getUsedMemory();
                elapsedTimer.start();
                for (auto &client : clients)
                    QMetaObject::invokeMethod(client->get(), "sendRequests", Qt::QueuedConnection);
            }
        });
        QObject::connect(client->get(), &ClientTcpSockets::receivedResponses, &ctxObject, [&]()
        {
            if (++receivedResponseCount == clientThreadCount)
            {
                requestsPerSecond = (1000.0 * clientThreadCount * workingConnectionsPerThread * requestsPerWorkingConnection)/elapsedTimer.elapsed();
                memoryConsumedAfterResponses = getUsedMemory();
                elapsedTimer.start();
                for (auto &client : clients)
                    QMetaObject::invokeMethod(client->get(), "disconnectFromServer", Qt::QueuedConnection);
            }
        });
        QObject::connect(client->get(), &ClientTcpSockets::disconnectedFromServer, &ctxObject, [&]()
        {
            if (++disconnectedClientCount == clientThreadCount)
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverSocketsDisconnectedSemaphore, 10000));
                disconnectionsPerSecond = (1000.0 * totalConnections)/elapsedTimer.elapsed();
                memoryConsumedAfterDisconnecting = getUsedMemory();
                clientSocketsDisconnectedSemaphore.release();
            }
        });
    }
    elapsedTimer.start();
    for (auto &client : clients)
        QMetaObject::invokeMethod(client->get(), "connectToServer", Qt::QueuedConnection);
    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientSocketsDisconnectedSemaphore, 1000));
    WARN(QByteArray("Memory consumed after creating client sockets: ").append(QByteArray::number(memoryConsumedAfterCreatingClientSockets)));
    WARN(QByteArray("Memory consumed after connecting: ").append(QByteArray::number(memoryConsumedAfterConnecting)));
    WARN(QByteArray("Memory consumed after responses: ").append(QByteArray::number(memoryConsumedAfterResponses)));
    WARN(QByteArray("Memory consumed after disconnecting: ").append(QByteArray::number(memoryConsumedAfterDisconnecting)));
    WARN(QByteArray("Connections per second: ").append(QByteArray::number(connectionsPerSecond)));
    WARN(QByteArray("Requests per second: ").append(QByteArray::number(requestsPerSecond)));
    WARN(QByteArray("Disconnections per second: ").append(QByteArray::number(disconnectionsPerSecond)));
}


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


SCENARIO("QTcpSocket benchmarks")
{
    static constexpr std::string_view serverAddress("127.25.24.25");
    static constexpr size_t totalConnectionsPerThread = 15000;
    static constexpr size_t workingConnectionsPerThread = 10000;
    static constexpr size_t clientThreadCount = 5;
    static constexpr size_t totalConnections = totalConnectionsPerThread * clientThreadCount;
    static constexpr size_t requestsPerWorkingConnection = 1000;
    static constexpr int a = 5;
    static constexpr int b = 3;
    size_t memoryConsumedAfterCreatingClientSockets = 0;
    size_t memoryConsumedAfterConnecting = 0;
    size_t memoryConsumedAfterResponses = 0;
    size_t memoryConsumedAfterDisconnecting = 0;
    QElapsedTimer elapsedTimer;
    double connectionsPerSecond = 0;
    double requestsPerSecond = 0;
    double disconnectionsPerSecond = 0;
    std::atomic_size_t connectedClientCount = 0;
    std::atomic_size_t receivedResponseCount = 0;
    std::atomic_size_t disconnectedClientCount = 0;
    QSemaphore clientSocketsDisconnectedSemaphore;
    QSemaphore serverSocketsConnectedSemaphore;
    QSemaphore serverSocketsDisconnectedSemaphore;
    std::unique_ptr<AsyncQObject<ServerQTcpSockets, std::string_view, size_t, size_t>> server(new AsyncQObject<ServerQTcpSockets, std::string_view, size_t, size_t>(serverAddress, totalConnections, requestsPerWorkingConnection));
    const auto serverPort = server->get()->serverPort();
    QObject::connect(server->get(), &ServerQTcpSockets::connectedToClients, [&](){serverSocketsConnectedSemaphore.release();});
    QObject::connect(server->get(), &ServerQTcpSockets::disconnectedFromClients, [&](){serverSocketsDisconnectedSemaphore.release();});
    std::vector<std::unique_ptr<AsyncQObject<ClientQTcpSockets, std::string_view, uint16_t, std::string_view, size_t, size_t, size_t, int, int>>> clients(clientThreadCount);
    size_t counter = 0;
    for (auto &client : clients)
    {
        std::string currentBindAddress("127.35.21.");
        currentBindAddress.append(std::to_string(++counter));
        client.reset(new AsyncQObject<ClientQTcpSockets, std::string_view, uint16_t, std::string_view, size_t, size_t, size_t, int, int>(serverAddress, serverPort, currentBindAddress, totalConnectionsPerThread, workingConnectionsPerThread, requestsPerWorkingConnection, a, b));
    }
    memoryConsumedAfterCreatingClientSockets = getUsedMemory();
    QObject ctxObject;
    for (auto &client : clients)
    {
        QObject::connect(client->get(), &ClientQTcpSockets::connectedToServer, &ctxObject, [&]()
        {
            if (++connectedClientCount == clientThreadCount)
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverSocketsConnectedSemaphore, 10000));
                connectionsPerSecond = (1000.0 * totalConnections)/elapsedTimer.elapsed();
                memoryConsumedAfterConnecting = getUsedMemory();
                elapsedTimer.start();
                for (auto &client : clients)
                    QMetaObject::invokeMethod(client->get(), "sendRequests", Qt::QueuedConnection);
            }
        });
        QObject::connect(client->get(), &ClientQTcpSockets::receivedResponses, &ctxObject, [&]()
        {
            if (++receivedResponseCount == clientThreadCount)
            {
                requestsPerSecond = (1000.0 * clientThreadCount * workingConnectionsPerThread * requestsPerWorkingConnection)/elapsedTimer.elapsed();
                memoryConsumedAfterResponses = getUsedMemory();
                elapsedTimer.start();
                for (auto &client : clients)
                    QMetaObject::invokeMethod(client->get(), "disconnectFromServer", Qt::QueuedConnection);
            }
        });
        QObject::connect(client->get(), &ClientQTcpSockets::disconnectedFromServer, &ctxObject, [&]()
        {
            if (++disconnectedClientCount == clientThreadCount)
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverSocketsDisconnectedSemaphore, 10000));
                disconnectionsPerSecond = (1000.0 * totalConnections)/elapsedTimer.elapsed();
                memoryConsumedAfterDisconnecting = getUsedMemory();
                clientSocketsDisconnectedSemaphore.release();
            }
        });
    }
    elapsedTimer.start();
    for (auto &client : clients)
        QMetaObject::invokeMethod(client->get(), "connectToServer", Qt::QueuedConnection);
    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientSocketsDisconnectedSemaphore, 10000));
    WARN(QByteArray("Memory consumed after creating client sockets: ").append(QByteArray::number(memoryConsumedAfterCreatingClientSockets)));
    WARN(QByteArray("Memory consumed after connecting: ").append(QByteArray::number(memoryConsumedAfterConnecting)));
    WARN(QByteArray("Memory consumed after responses: ").append(QByteArray::number(memoryConsumedAfterResponses)));
    WARN(QByteArray("Memory consumed after disconnecting: ").append(QByteArray::number(memoryConsumedAfterDisconnecting)));
    WARN(QByteArray("Connections per second: ").append(QByteArray::number(connectionsPerSecond)));
    WARN(QByteArray("Requests per second: ").append(QByteArray::number(requestsPerSecond)));
    WARN(QByteArray("Disconnections per second: ").append(QByteArray::number(disconnectionsPerSecond)));
}

#include "TcpSocket.spec.moc"
