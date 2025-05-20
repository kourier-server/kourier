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
#include "AsyncQObject.h"
#include <Tests/Resources/TlsServer.h>
#include <Tests/Resources/TlsTestCertificates.h>
#include <QSslSocket>
#include <QSslServer>
#include <QSslKey>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QList>
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

using Kourier::TlsServer;
using Kourier::TcpSocket;
using Kourier::TlsSocket;
using Kourier::TlsConfiguration;
using Kourier::Object;
using Kourier::AsyncQObject;
using Spectator::SemaphoreAwaiter;
using Kourier::TestResources::TlsTestCertificateInfo;
using Kourier::TestResources::TlsTestCertificates;


namespace TlsSocketTests
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

using namespace TlsSocketTests;


SCENARIO("TlsSocket interacts with client peer")
{
    GIVEN("a listening server")
    {
        const auto certificateType = GENERATE(AS(TlsTestCertificates::CertificateType),
                                              TlsTestCertificates::CertificateType::RSA_2048,
                                              TlsTestCertificates::CertificateType::RSA_2048_CHAIN,
                                              TlsTestCertificates::CertificateType::RSA_2048_EncryptedPrivateKey,
                                              TlsTestCertificates::CertificateType::RSA_2048_CHAIN_EncryptedPrivateKey,
                                              TlsTestCertificates::CertificateType::ECDSA,
                                              TlsTestCertificates::CertificateType::ECDSA_CHAIN,
                                              TlsTestCertificates::CertificateType::ECDSA_EncryptedPrivateKey,
                                              TlsTestCertificates::CertificateType::ECDSA_CHAIN_EncryptedPrivateKey);
        std::string certificateFile;
        std::string privateKeyFile;
        std::string caCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
        std::string certificateContents;
        std::string privateKeyContents;
        std::string privateKeyPassword;
        std::string caCertificateContents;
        TlsTestCertificates::getContentsFromCertificateType(certificateType, certificateContents, privateKeyContents, privateKeyPassword, caCertificateContents);
        auto sslCaCert = QSslCertificate::fromPath(QString::fromStdString(caCertificateFile));
        REQUIRE(!sslCaCert.isEmpty());
        QSslConfiguration clientTlsConfiguration;
        clientTlsConfiguration.setCaCertificates({sslCaCert});
        TlsConfiguration serverTlsConfiguration;
        serverTlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile, privateKeyPassword);
        serverTlsConfiguration.addCaCertificate(caCertificateFile);
        TlsServer server(serverTlsConfiguration);
        QSemaphore socketConnectedSemaphore;
        QSemaphore socketCompletedHandshakeSemaphore;
        QSemaphore socketFailedSemaphore;
        QSemaphore socketDisconnectedSemaphore;
        QSemaphore socketReceivedDataFromPeerSemaphore;
        QByteArray socketReceivedData;
        std::unique_ptr<TlsSocket> pSocket;
        Object::connect(&server, &TlsServer::newConnection, [&](TlsSocket *pNewSocket)
        {
            REQUIRE(!pSocket);
            pSocket.reset(pNewSocket);
            REQUIRE(!pSocket->isEncrypted());
            Object::connect(pSocket.get(), &TlsSocket::connected, []() {FAIL("This code is supposed to be unreachable.");});
            Object::connect(pSocket.get(), &TlsSocket::encrypted, [&]()
            {
                REQUIRE(pSocket->isEncrypted());
                socketCompletedHandshakeSemaphore.release();
            });
            Object::connect(pSocket.get(), &TlsSocket::error, [&]() {socketFailedSemaphore.release();});
            Object::connect(pSocket.get(), &TlsSocket::disconnected, [&socketDisconnectedSemaphore]() {socketDisconnectedSemaphore.release();});
            Object::connect(pSocket.get(), &TlsSocket::receivedData, [&]()
            {
                QByteArray readData;
                readData.resize(pSocket->dataAvailable());
                pSocket->read(readData.data(), readData.size());
                socketReceivedData.append(readData);
                socketReceivedDataFromPeerSemaphore.release();
            });
        });
        const auto serverAddress = GENERATE(AS(QHostAddress),
                                            QHostAddress("127.10.20.50"),
                                            QHostAddress("127.10.20.60"),
                                            QHostAddress("127.10.20.70"),
                                            QHostAddress("127.10.20.80"),
                                            QHostAddress("127.10.20.90"),
                                            QHostAddress("::1"));
        REQUIRE(server.listen(serverAddress, 0));
        const auto serverPort = server.serverPort();
        REQUIRE(serverPort >= 1024);

        WHEN("peer connects to host")
        {
            QSemaphore peerConnectedSemaphore;
            QSemaphore peerCompletedHandshakeSemaphore;
            QSemaphore peerFailedSemaphore;
            QSemaphore peerDisconnectedSemaphore;
            QSemaphore peerReceivedDataFromSocketSemaphore;
            QByteArray peerReceivedData;
            std::unique_ptr<QSslSocket> pPeerSocket(new QSslSocket);
            QObject::connect(pPeerSocket.get(), &QSslSocket::connected, [&]() {pPeerSocket->setSocketOption(QAbstractSocket::SocketOption::LowDelayOption, 1); peerConnectedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::errorOccurred, [&](QAbstractSocket::SocketError error) {peerFailedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::encrypted, [&peerCompletedHandshakeSemaphore](){peerCompletedHandshakeSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::disconnected, [&peerDisconnectedSemaphore](){peerDisconnectedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::readyRead, [&]()
            {
                peerReceivedData.append(pPeerSocket->readAll());
                peerReceivedDataFromSocketSemaphore.release();
            });
            pPeerSocket->setSslConfiguration(clientTlsConfiguration);
            pPeerSocket->connectToHostEncrypted("test.onlocalhost.com", serverPort);

            THEN("server emits newConnection with a connected socket that does not emit connected but emits encrypted after completing tls handshake")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketCompletedHandshakeSemaphore, 10));
                REQUIRE(pSocket->state() == TcpSocket::State::Connected);
                REQUIRE(!socketConnectedSemaphore.tryAcquire());

                AND_THEN("connecting peer socket emits connect and encrypted after completing tls handshake")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerCompletedHandshakeSemaphore, 10));
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
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerReceivedDataFromSocketSemaphore, 1));
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
                                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerReceivedDataFromSocketSemaphore, 1));
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
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerReceivedDataFromSocketSemaphore, 1));
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


SCENARIO("TlsSocket interacts with server peer by name")
{
    GIVEN("a listening server")
    {
        const auto certificateType = GENERATE(AS(TlsTestCertificates::CertificateType),
                                              TlsTestCertificates::CertificateType::RSA_2048,
                                              TlsTestCertificates::CertificateType::RSA_2048_CHAIN,
                                              TlsTestCertificates::CertificateType::ECDSA,
                                              TlsTestCertificates::CertificateType::ECDSA_CHAIN);
        std::string certificateFile;
        std::string privateKeyFile;
        std::string caCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
        auto certChain = QSslCertificate::fromPath(QString::fromStdString(certificateFile));
        REQUIRE(!certChain.isEmpty());
        auto sslCert = QSslCertificate::fromPath(QString::fromStdString(caCertificateFile));
        REQUIRE(!sslCert.isEmpty());
        QSslConfiguration serverTlsConfiguration;
        serverTlsConfiguration.setLocalCertificateChain(certChain);
        QFile file(QString::fromStdString(privateKeyFile));
        REQUIRE(file.open(QIODevice::ReadOnly));
        const auto keyContents = file.readAll();
        REQUIRE(!keyContents.isEmpty());
        QSsl::KeyAlgorithm sslKeyAlgorithm = QSsl::Opaque;
        switch (certificateType)
        {
            case Kourier::TestResources::TlsTestCertificates::RSA_2048:
            case Kourier::TestResources::TlsTestCertificates::RSA_2048_CHAIN:
                sslKeyAlgorithm = QSsl::Rsa;
                break;
            case Kourier::TestResources::TlsTestCertificates::ECDSA:
            case Kourier::TestResources::TlsTestCertificates::ECDSA_CHAIN:
                sslKeyAlgorithm = QSsl::Ec;
                break;
            default:
                qFatal("This code is supposed to be unreachable.");
                break;
        }
        QSslKey sslKey(keyContents, sslKeyAlgorithm);
        REQUIRE(!sslKey.isNull());
        serverTlsConfiguration.setPrivateKey(sslKey);
        serverTlsConfiguration.addCaCertificates(sslCert);
        REQUIRE(!serverTlsConfiguration.isNull());
        TlsConfiguration clientTlsConfiguration;
        clientTlsConfiguration.addCaCertificate(caCertificateFile);
        QSslServer server;
        QSslServer ipv6Server;
        server.setSslConfiguration(serverTlsConfiguration);
        ipv6Server.setSslConfiguration(serverTlsConfiguration);
        bool connectedToIpv6Server = false;
        QSemaphore peerConnectedSemaphore;
        QSemaphore peerFailedSemaphore;
        QSemaphore peerDisconnectedSemaphore;
        QSemaphore peerReceivedDataFromSocketSemaphore;
        QByteArray peerReceivedData;
        std::unique_ptr<QSslSocket> pPeerSocket;
        QObject::connect(&ipv6Server, &QSslServer::pendingConnectionAvailable, [&]()
        {
            connectedToIpv6Server = true;
            REQUIRE(ipv6Server.hasPendingConnections());
            REQUIRE(!pPeerSocket);
            pPeerSocket.reset(qobject_cast<QSslSocket*>(ipv6Server.nextPendingConnection()));
            REQUIRE(pPeerSocket.get() != nullptr);
            pPeerSocket->setParent(nullptr);
            pPeerSocket->setSocketOption(QAbstractSocket::SocketOption::LowDelayOption, 1);
            REQUIRE(pPeerSocket->isEncrypted());
            REQUIRE(!ipv6Server.hasPendingConnections());
            QObject::connect(pPeerSocket.get(), &QSslSocket::errorOccurred, [&](QAbstractSocket::SocketError error) {peerFailedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::disconnected, [&](){peerDisconnectedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::readyRead, [&]()
            {
                peerReceivedData.append(pPeerSocket->readAll());
                peerReceivedDataFromSocketSemaphore.release();
            });
            peerConnectedSemaphore.release();
        });
        REQUIRE(ipv6Server.listen(QHostAddress("::1"), 0));
        const auto serverPort = ipv6Server.serverPort();
        REQUIRE(serverPort >= 1024);
        QObject::connect(&server, &QSslServer::pendingConnectionAvailable, [&]()
        {
            REQUIRE(server.hasPendingConnections());
            REQUIRE(!pPeerSocket);
            pPeerSocket.reset(qobject_cast<QSslSocket*>(server.nextPendingConnection()));
            REQUIRE(pPeerSocket.get() != nullptr);
            pPeerSocket->setParent(nullptr);
            pPeerSocket->setSocketOption(QAbstractSocket::SocketOption::LowDelayOption, 1);
            REQUIRE(pPeerSocket->isEncrypted());
            REQUIRE(!server.hasPendingConnections());
            QObject::connect(pPeerSocket.get(), &QSslSocket::errorOccurred, [&](QAbstractSocket::SocketError error) {peerFailedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::disconnected, [&](){peerDisconnectedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::readyRead, [&]()
            {
                peerReceivedData.append(pPeerSocket->readAll());
                peerReceivedDataFromSocketSemaphore.release();
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

        WHEN("TlsSocket connects to server")
        {
            QSemaphore socketConnectedSemaphore;
            QSemaphore socketCompletedHandshakeSemaphore;
            QSemaphore socketFailedSemaphore;
            QSemaphore socketDisconnectedSemaphore;
            QSemaphore socketReceivedDataFromPeerSemaphore;
            QByteArray socketReceivedData;
            std::unique_ptr<TlsSocket> pSocket(new TlsSocket(clientTlsConfiguration));
            Object::connect(pSocket.get(), &TlsSocket::error, [&]() {socketFailedSemaphore.release();});
            Object::connect(pSocket.get(), &TlsSocket::connected, [&]()
            {
                REQUIRE(!pSocket->isEncrypted());
                socketConnectedSemaphore.release();
            });
            Object::connect(pSocket.get(), &TlsSocket::encrypted, [&]()
            {
                REQUIRE(pSocket->isEncrypted());
                socketCompletedHandshakeSemaphore.release();
            });
            Object::connect(pSocket.get(), &TlsSocket::disconnected, [&](){socketDisconnectedSemaphore.release();});
            Object::connect(pSocket.get(), &TlsSocket::receivedData, [&]()
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

            THEN("server emits pendingConnectionAvailable with a socket that has already completed tls handshake")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                REQUIRE(pPeerSocket.get() != nullptr);
                REQUIRE(pPeerSocket->state() == QAbstractSocket::ConnectedState);
                REQUIRE(pPeerSocket->isEncrypted());

                AND_THEN("TlsSocket emits connected and then emits encrypted after completing tls handshake")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketCompletedHandshakeSemaphore, 10));
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
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerReceivedDataFromSocketSemaphore, 1));
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
                                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerReceivedDataFromSocketSemaphore, 1));
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
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerReceivedDataFromSocketSemaphore, 1));
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


SCENARIO("TlsSocket interacts with TlsSocket-based server peer by name")
{
    GIVEN("a listening server")
    {
        const auto certificateType = GENERATE(AS(TlsTestCertificates::CertificateType),
                                              TlsTestCertificates::CertificateType::RSA_2048,
                                              TlsTestCertificates::CertificateType::RSA_2048_CHAIN,
                                              TlsTestCertificates::CertificateType::ECDSA,
                                              TlsTestCertificates::CertificateType::ECDSA_CHAIN);
        std::string certificateFile;
        std::string privateKeyFile;
        std::string caCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
        TlsConfiguration serverTlsConfiguration;
        serverTlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile);
        serverTlsConfiguration.addCaCertificate(caCertificateFile);
        TlsConfiguration clientTlsConfiguration;
        clientTlsConfiguration.addCaCertificate(caCertificateFile);
        TlsServer server(serverTlsConfiguration);
        TlsServer ipv6Server(serverTlsConfiguration);
        bool connectedToIpv6Server = false;
        QSemaphore peerConnectedSemaphore;
        QSemaphore peerCompletedHandshakeSemaphore;
        QSemaphore peerFailedSemaphore;
        QSemaphore peerDisconnectedSemaphore;
        QSemaphore peerReceivedDataFromSocketSemaphore;
        QByteArray peerReceivedData;
        std::unique_ptr<TlsSocket> pPeerSocket;
        Object::connect(&ipv6Server, &TlsServer::newConnection, [&](TlsSocket *pSocket)
        {
            connectedToIpv6Server = true;
            REQUIRE(!pPeerSocket);
            pPeerSocket.reset(pSocket);
            REQUIRE(!pPeerSocket->isEncrypted());
            Object::connect(pPeerSocket.get(), &TlsSocket::error, [&]() {peerFailedSemaphore.release();});
            Object::connect(pPeerSocket.get(), &TlsSocket::connected, [&]() {FAIL("This code is supposed to be unreachable.");});
            Object::connect(pPeerSocket.get(), &TlsSocket::encrypted, [&]()
            {
                REQUIRE(pPeerSocket->isEncrypted());
                peerCompletedHandshakeSemaphore.release();
            });
            Object::connect(pPeerSocket.get(), &TlsSocket::disconnected, [&](){peerDisconnectedSemaphore.release();});
            Object::connect(pPeerSocket.get(), &TlsSocket::receivedData, [&]()
            {
                peerReceivedData.append(pPeerSocket->readAll());
                peerReceivedDataFromSocketSemaphore.release();
            });
            peerConnectedSemaphore.release();
        });
        REQUIRE(ipv6Server.listen(QHostAddress("::1"), 0));
        const auto serverPort = ipv6Server.serverPort();
        REQUIRE(serverPort >= 1024);
        Object::connect(&server, &TlsServer::newConnection, [&](TlsSocket *pSocket)
        {
            REQUIRE(!pPeerSocket);
            pPeerSocket.reset(pSocket);
            REQUIRE(!pPeerSocket->isEncrypted());
            Object::connect(pPeerSocket.get(), &TlsSocket::error, [&]() {peerFailedSemaphore.release();});
            Object::connect(pPeerSocket.get(), &TlsSocket::connected, [&]() {FAIL("This code is supposed to be unreachable.");});
            Object::connect(pPeerSocket.get(), &TlsSocket::encrypted, [&]()
            {
                REQUIRE(pPeerSocket->isEncrypted());
                peerCompletedHandshakeSemaphore.release();
            });
            Object::connect(pPeerSocket.get(), &TlsSocket::disconnected, [&](){peerDisconnectedSemaphore.release();});
            Object::connect(pPeerSocket.get(), &TlsSocket::receivedData, [&]()
            {
                peerReceivedData.append(pPeerSocket->readAll());
                peerReceivedDataFromSocketSemaphore.release();
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

        WHEN("TlsSocket connects to server")
        {
            QSemaphore socketConnectedSemaphore;
            QSemaphore socketCompletedHandshakeSemaphore;
            QSemaphore socketFailedSemaphore;
            QSemaphore socketDisconnectedSemaphore;
            QSemaphore socketReceivedDataFromPeerSemaphore;
            QByteArray socketReceivedData;
            std::unique_ptr<TlsSocket> pSocket(new TlsSocket(clientTlsConfiguration));
            Object::connect(pSocket.get(), &TlsSocket::error, [&]() {socketFailedSemaphore.release();});
            Object::connect(pSocket.get(), &TlsSocket::connected, [&]()
            {
                REQUIRE(!pSocket->isEncrypted());
                socketConnectedSemaphore.release();
            });
            Object::connect(pSocket.get(), &TlsSocket::encrypted, [&]()
            {
                REQUIRE(pSocket->isEncrypted());
                socketCompletedHandshakeSemaphore.release();
            });
            Object::connect(pSocket.get(), &TlsSocket::disconnected, [&](){socketDisconnectedSemaphore.release();});
            Object::connect(pSocket.get(), &TlsSocket::receivedData, [&]()
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

            THEN("server emits newConnection with a connected socket that does not emit connected but emits encrypted after completing tls handshake")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerCompletedHandshakeSemaphore, 10));
                REQUIRE(pPeerSocket->state() == TcpSocket::State::Connected);

                AND_THEN("TlsSocket emits connected and then emits encrypted after completing tls handshake")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketCompletedHandshakeSemaphore, 10));
                    REQUIRE(pPeerSocket->localAddress() == pSocket->peerAddress());
                    REQUIRE(pPeerSocket->localPort() == pSocket->peerPort());
                    REQUIRE(pPeerSocket->peerAddress() == pSocket->localAddress());
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
                        {
                            pSocket->setReadBufferCapacity(readBufferCapacity);
                            pPeerSocket->setReadBufferCapacity(readBufferCapacity);
                        }
                        if (disableLowDelayOption)
                        {
                            pSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, 0);
                            pPeerSocket->setSocketOption(TcpSocket::SocketOption::LowDelay, 0);
                        }
                        REQUIRE(pSocket->getSocketOption(TcpSocket::SocketOption::LowDelay) == disableLowDelayOption ? 0 : 1);
                        REQUIRE(pPeerSocket->getSocketOption(TcpSocket::SocketOption::LowDelay) == disableLowDelayOption ? 0 : 1);
                        if (setKeepAliveOption)
                        {
                            pPeerSocket->setSocketOption(TcpSocket::SocketOption::KeepAlive, 1);
                            pSocket->setSocketOption(TcpSocket::SocketOption::KeepAlive, 1);
                        }
                        REQUIRE(pPeerSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive) == setKeepAliveOption ? 1 : 0);
                        REQUIRE(pSocket->getSocketOption(TcpSocket::SocketOption::KeepAlive) == setKeepAliveOption ? 1 : 0);

                        AND_WHEN("peer sends data to TlsSocket")
                        {
                            pPeerSocket->write(dataToSend.data(), dataToSend.size());

                            THEN("TlsSocket receives sent data")
                            {
                                const auto &sentData = socketReceivedData;
                                while (sentData != dataToSend)
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketReceivedDataFromPeerSemaphore, 1));
                                }

                                AND_WHEN("peer sends some more data to TlsSocket")
                                {
                                    socketReceivedData.clear();
                                    const QByteArray someMoreData("0123456789");
                                    pPeerSocket->write(someMoreData.data(), someMoreData.size());

                                    THEN("TlsSocket receives sent data")
                                    {
                                        while (sentData != someMoreData)
                                        {
                                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketReceivedDataFromPeerSemaphore, 1));
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("peer closes connection after sending data to TlsSocket")
                        {
                            pPeerSocket->write(dataToSend.data(), dataToSend.size());
                            pPeerSocket->disconnectFromPeer();

                            THEN("TlsSocket receives sent data")
                            {
                                const auto &sentData = socketReceivedData;
                                while (sentData != dataToSend)
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketReceivedDataFromPeerSemaphore, 1));
                                }

                                AND_THEN("both peer and TlsSocket emit disconnected")
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

                                    AND_WHEN("TlsSocket is deleted")
                                    {
                                        while (socketFailedSemaphore.tryAcquire()) {}
                                        pSocket.reset(nullptr);

                                        THEN("neither peer or TlsSocket emit any error")
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

                            THEN("peer aborts and socket emits disconnected")
                            {
                                REQUIRE(pPeerSocket->state() == TcpSocket::State::Unconnected);
                                REQUIRE(!pPeerSocket->isEncrypted());
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                                REQUIRE(!peerDisconnectedSemaphore.tryAcquire());
                                REQUIRE(pSocket->errorMessage().empty());
                                REQUIRE(pPeerSocket->errorMessage().empty());
                            }
                        }

                        AND_WHEN("peer is deleted after writing data")
                        {
                            pPeerSocket->write(dataToSend.data(), dataToSend.size());
                            pPeerSocket.reset(nullptr);

                            THEN("TlsSocket emits disconnected")
                            {
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                                REQUIRE(pSocket->errorMessage().empty());
                            }
                        }

                        AND_WHEN("TlsSocket sends data to peer")
                        {
                            pSocket->write(dataToSend.data(), dataToSend.size());

                            THEN("peer receives sent data")
                            {
                                const auto &sentData = peerReceivedData;
                                while (sentData != dataToSend)
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerReceivedDataFromSocketSemaphore, 1));
                                }

                                AND_WHEN("TlsSocket sends some more data to peer")
                                {
                                    peerReceivedData.clear();
                                    const QByteArray someMoreData("0123456789");
                                    pSocket->write(someMoreData.data(), someMoreData.size());

                                    THEN("peer receives sent data")
                                    {
                                        while (sentData != someMoreData)
                                        {
                                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerReceivedDataFromSocketSemaphore, 1));
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("TlsSocket closes connection after sending data to peer")
                        {
                            pSocket->write(dataToSend.data(), dataToSend.size());
                            pSocket->disconnectFromPeer();

                            THEN("peer receives sent data")
                            {
                                QByteArray sentData;
                                while (sentData != dataToSend)
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerReceivedDataFromSocketSemaphore, 1));
                                    sentData = peerReceivedData;
                                }

                                AND_THEN("both sockets emit disconnected")
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));
                                    REQUIRE(pPeerSocket->errorMessage().empty());
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                                    REQUIRE(pSocket->errorMessage().empty());

                                    AND_WHEN("TlsSocket is deleted")
                                    {
                                        while (socketFailedSemaphore.tryAcquire()) {}
                                        pSocket.reset(nullptr);

                                        THEN("TlsSocket does not emit any error")
                                        {
                                            REQUIRE(pPeerSocket->errorMessage().empty());
                                            REQUIRE(!socketFailedSemaphore.tryAcquire());
                                        }
                                    }

                                    AND_WHEN("Peer is deleted")
                                    {
                                        while (peerFailedSemaphore.tryAcquire()) {}
                                        pPeerSocket.reset(nullptr);

                                        THEN("neither peer or TlsSocket emit any error")
                                        {
                                            REQUIRE(!peerFailedSemaphore.tryAcquire());
                                            REQUIRE(!socketFailedSemaphore.tryAcquire());
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("socket aborts after writing data")
                        {
                            pSocket->write(dataToSend.data(), dataToSend.size());
                            pSocket->abort();

                            THEN("socket aborts and peer emits disconnected")
                            {
                                REQUIRE(pSocket->state() == TcpSocket::State::Unconnected);
                                REQUIRE(!pSocket->isEncrypted());
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                                REQUIRE(!socketDisconnectedSemaphore.tryAcquire());
                                REQUIRE(pSocket->errorMessage().empty());
                                REQUIRE(pPeerSocket->errorMessage().empty());
                            }
                        }

                        AND_WHEN("TlsSocket is deleted after sending data to peer")
                        {
                            pSocket->write(dataToSend.data(), dataToSend.size());
                            pSocket.reset(nullptr);

                            THEN("TlsSocket aborts and Peer emits disconnected")
                            {
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                                REQUIRE(pPeerSocket->errorMessage().empty());
                            }
                        }
                    }

                    AND_WHEN("peer disconnects from TlsSocket")
                    {
                        pPeerSocket->disconnectFromPeer();

                        THEN("peer emits disconnected and TlsSocket emits disconnected")
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

                            AND_WHEN("TlsSocket is deleted")
                            {
                                while (socketFailedSemaphore.tryAcquire()) {}
                                pSocket.reset(nullptr);

                                THEN("neither peer or TlsSocket emit any error")
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

                        THEN("TlsSocket emits disconnected")
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

                            AND_WHEN("TlsSocket is deleted")
                            {
                                while (socketFailedSemaphore.tryAcquire()) {}
                                pSocket.reset(nullptr);

                                THEN("neither peer or TlsSocket emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }
                        }
                    }

                    AND_WHEN("TlsSocket disconnects from TlsSocket")
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

                                THEN("neither peer or TlsSocket emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }

                            AND_WHEN("TlsSocket is deleted")
                            {
                                pSocket.reset(nullptr);

                                THEN("TlsSocket does not emit any error")
                                {
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                    REQUIRE(pPeerSocket->errorMessage().empty());
                                }
                            }
                        }
                    }

                    AND_WHEN("TlsSocket aborts connection")
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

                                THEN("neither peer or TlsSocket emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }

                            AND_WHEN("TlsSocket is deleted")
                            {
                                pSocket.reset(nullptr);

                                THEN("TlsSocket does not emit any error")
                                {
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                    REQUIRE(pPeerSocket->errorMessage().empty());
                                }
                            }
                        }
                    }

                    AND_WHEN("both peer and TlsSocket disconnects")
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

                                THEN("TlsSocket not emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(pSocket->errorMessage().empty());
                                }
                            }

                            AND_WHEN("TlsSocket is deleted")
                            {
                                while (socketFailedSemaphore.tryAcquire()) {}
                                pSocket.reset(nullptr);

                                THEN("TlsSocket does not emit any error")
                                {
                                    REQUIRE(!socketFailedSemaphore.tryAcquire());
                                }
                            }
                        }
                    }

                    AND_WHEN("both TlsSocket and peer disconnects")
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

                                THEN("TlsSocket not emit any error")
                                {
                                    REQUIRE(!peerFailedSemaphore.tryAcquire());
                                    REQUIRE(pSocket->errorMessage().empty());
                                }
                            }

                            AND_WHEN("TlsSocket is deleted")
                            {
                                while (socketFailedSemaphore.tryAcquire()) {}
                                pSocket.reset(nullptr);

                                THEN("TlsSocket does not emit any error")
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

                        THEN("peer socket emits disconnected signal")
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


SCENARIO("TlsSocket supports client authentication (two-way SSL)")
{
    GIVEN("a TlsSocket that has to authenticate client peer")
    {
        const auto clientCertificateType = TlsTestCertificates::CertificateType::ECDSA;
        std::string clientCertificateFile;
        std::string clientPrivateKeyFile;
        std::string clientCaCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(clientCertificateType, clientCertificateFile, clientPrivateKeyFile, clientCaCertificateFile);
        std::string clientCertificateContents;
        std::string clientPrivateKeyContents;
        std::string clientPrivateKeyPassword;
        std::string clientCaCertificateContents;
        TlsTestCertificates::getContentsFromCertificateType(clientCertificateType, clientCertificateContents, clientPrivateKeyContents, clientPrivateKeyPassword, clientCaCertificateContents);
        const auto serverCertificateType = TlsTestCertificates::CertificateType::RSA_2048;
        std::string serverCertificateFile;
        std::string serverPrivateKeyFile;
        std::string serverCaCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(serverCertificateType, serverCertificateFile, serverPrivateKeyFile, serverCaCertificateFile);
        std::string serverCertificateContents;
        std::string serverPrivateKeyContents;
        std::string serverPrivateKeyPassword;
        std::string serverCaCertificateContents;
        TlsTestCertificates::getContentsFromCertificateType(serverCertificateType, serverCertificateContents, serverPrivateKeyContents, serverPrivateKeyPassword, serverCaCertificateContents);
        auto sslCaCert = QSslCertificate::fromPath(QString::fromStdString(serverCaCertificateFile));
        REQUIRE(!sslCaCert.isEmpty());
        QSslConfiguration clientTlsConfiguration;
        clientTlsConfiguration.setCaCertificates({sslCaCert});
        auto certChain = QSslCertificate::fromPath(QString::fromStdString(clientCertificateFile));
        REQUIRE(!certChain.isEmpty());
        auto sslCert = QSslCertificate::fromPath(QString::fromStdString(clientCaCertificateFile));
        REQUIRE(!sslCert.isEmpty());
        clientTlsConfiguration.setLocalCertificateChain(certChain);
        QSslKey sslKey(QByteArray(clientPrivateKeyContents.data(), clientPrivateKeyContents.size()), QSsl::Ec);
        REQUIRE(!sslKey.isNull());
        clientTlsConfiguration.setPrivateKey(sslKey);
        TlsConfiguration serverTlsConfiguration;
        serverTlsConfiguration.setPeerVerifyMode(TlsConfiguration::PeerVerifyMode::On);
        serverTlsConfiguration.setCertificateKeyPair(serverCertificateFile, serverPrivateKeyFile, serverPrivateKeyPassword);
        serverTlsConfiguration.addCaCertificate(serverCaCertificateFile);
        serverTlsConfiguration.addCaCertificate(clientCaCertificateFile);
        TlsServer server(serverTlsConfiguration);
        QSemaphore socketCompletedHandshakeSemaphore;
        QSemaphore socketFailedSemaphore;
        QSemaphore socketDisconnectedSemaphore;
        QSemaphore socketReceivedDataFromPeerSemaphore;
        QByteArray socketReceivedData;
        std::unique_ptr<TlsSocket> pSocket;
        Object::connect(&server, &TlsServer::newConnection, [&](TlsSocket *pNewSocket)
        {
            pSocket.reset(pNewSocket);
            REQUIRE(pSocket->state() == TcpSocket::State::Connected);
            REQUIRE(!pSocket->isEncrypted());
            Object::connect(pSocket.get(), &TlsSocket::connected, [&](){FAIL("This code is supposed to be unreachable.");});
            Object::connect(pSocket.get(), &TlsSocket::encrypted, [&]()
            {
                REQUIRE(pSocket->isEncrypted());
                socketCompletedHandshakeSemaphore.release();
            });
            Object::connect(pSocket.get(), &TlsSocket::error, [&]() {socketFailedSemaphore.release();});
            Object::connect(pSocket.get(), &TlsSocket::disconnected, [&socketDisconnectedSemaphore]() {socketDisconnectedSemaphore.release();});
            Object::connect(pSocket.get(), &TlsSocket::receivedData, [&]()
            {
                QByteArray readData;
                readData.resize(pSocket->dataAvailable());
                pSocket->read(readData.data(), readData.size());
                socketReceivedData.append(readData);
                socketReceivedDataFromPeerSemaphore.release();
            });
        });
        const auto serverAddress = QHostAddress("127.10.20.90");
        REQUIRE(server.listen(serverAddress, 0));
        const auto serverPort = server.serverPort();
        REQUIRE(serverPort >= 1024);

        WHEN("peer connects to host")
        {
            QSemaphore peerConnectedSemaphore;
            QSemaphore peerCompletedHandshakeSemaphore;
            QSemaphore peerFailedSemaphore;
            QSemaphore peerDisconnectedSemaphore;
            QSemaphore peerReceivedDataFromTcpSocketSemaphore;
            QByteArray peerReceivedData;
            std::unique_ptr<QSslSocket> pPeerSocket(new QSslSocket);
            QObject::connect(pPeerSocket.get(), &QSslSocket::connected, [&]() {pPeerSocket->setSocketOption(QAbstractSocket::SocketOption::LowDelayOption, 1); peerConnectedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::errorOccurred, [&](QAbstractSocket::SocketError error) {peerFailedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::encrypted, [&](){peerCompletedHandshakeSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::disconnected, [&peerDisconnectedSemaphore](){peerDisconnectedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::readyRead, [&]()
            {
                peerReceivedData.append(pPeerSocket->readAll());
                peerReceivedDataFromTcpSocketSemaphore.release();
            });
            pPeerSocket->setSslConfiguration(clientTlsConfiguration);
            pPeerSocket->connectToHostEncrypted("test.onlocalhost.com", serverPort);

            THEN("server emits newConnection with a connected socket that does not emit connected but emits encrypted when tls handshake completes")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketCompletedHandshakeSemaphore, 10));

                AND_THEN("connecting peer socket emits connected and encrypted when tls hanshake completes")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerCompletedHandshakeSemaphore, 10));
                    REQUIRE(pPeerSocket->localAddress() == QHostAddress(QString::fromStdString(std::string(pSocket->peerAddress()))));
                    REQUIRE(pPeerSocket->localPort() == pSocket->peerPort());
                    REQUIRE(pPeerSocket->peerAddress() == QHostAddress(QString::fromStdString(std::string(pSocket->localAddress()))));
                    REQUIRE(pPeerSocket->peerPort() == pSocket->localPort());

                    AND_WHEN("client peer sends data to server peer")
                    {
                        const QByteArray dataToSend("Some data");
                        pPeerSocket->write(dataToSend);

                        THEN("server peer receives sent data")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketReceivedDataFromPeerSemaphore, 10));
                            REQUIRE(socketReceivedData == dataToSend);
                        }
                    }
                }
            }
        }
    }

    GIVEN("a TlsSocket client peer that has to be authenticated by server peer")
    {
        const auto clientCertificateType = TlsTestCertificates::CertificateType::ECDSA;
        std::string clientCertificateFile;
        std::string clientPrivateKeyFile;
        std::string clientCaCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(clientCertificateType, clientCertificateFile, clientPrivateKeyFile, clientCaCertificateFile);
        std::string clientCertificateContents;
        std::string clientPrivateKeyContents;
        std::string clientPrivateKeyPassword;
        std::string clientCaCertificateContents;
        TlsTestCertificates::getContentsFromCertificateType(clientCertificateType, clientCertificateContents, clientPrivateKeyContents, clientPrivateKeyPassword, clientCaCertificateContents);
        const auto serverCertificateType = TlsTestCertificates::CertificateType::RSA_2048;
        std::string serverCertificateFile;
        std::string serverPrivateKeyFile;
        std::string serverCaCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(serverCertificateType, serverCertificateFile, serverPrivateKeyFile, serverCaCertificateFile);
        std::string serverCertificateContents;
        std::string serverPrivateKeyContents;
        std::string serverPrivateKeyPassword;
        std::string serverCaCertificateContents;
        TlsTestCertificates::getContentsFromCertificateType(serverCertificateType, serverCertificateContents, serverPrivateKeyContents, serverPrivateKeyPassword, serverCaCertificateContents);
        auto certChain = QSslCertificate::fromPath(QString::fromStdString(serverCertificateFile));
        REQUIRE(!certChain.isEmpty());
        auto sslCert = QSslCertificate::fromPath(QString::fromStdString(clientCaCertificateFile));
        REQUIRE(!sslCert.isEmpty());
        QSslConfiguration serverTlsConfiguration;
        serverTlsConfiguration.setPeerVerifyMode(QSslSocket::PeerVerifyMode::VerifyPeer);
        serverTlsConfiguration.setLocalCertificateChain(certChain);
        QSslKey sslKey(QByteArray(serverPrivateKeyContents.data(), serverPrivateKeyContents.size()), QSsl::Rsa);
        REQUIRE(!sslKey.isNull());
        serverTlsConfiguration.setPrivateKey(sslKey);
        serverTlsConfiguration.addCaCertificates(sslCert);
        REQUIRE(!serverTlsConfiguration.isNull());
        TlsConfiguration clientTlsConfiguration;
        clientTlsConfiguration.addCaCertificate(clientCaCertificateFile);
        clientTlsConfiguration.addCaCertificate(serverCaCertificateFile);
        clientTlsConfiguration.setCertificateKeyPair(clientCertificateFile, clientPrivateKeyFile);
        QSslServer server;
        server.setSslConfiguration(serverTlsConfiguration);
        QSemaphore peerConnectedSemaphore;
        QSemaphore peerFailedSemaphore;
        QSemaphore peerDisconnectedSemaphore;
        QSemaphore peerReceivedDataFromSocketSemaphore;
        QByteArray peerReceivedData;
        std::unique_ptr<QSslSocket> pPeerSocket;
        QObject::connect(&server, &QSslServer::pendingConnectionAvailable, [&]()
        {
            REQUIRE(server.hasPendingConnections());
            REQUIRE(!pPeerSocket);
            pPeerSocket.reset(qobject_cast<QSslSocket*>(server.nextPendingConnection()));
            REQUIRE(pPeerSocket.get() != nullptr);
            pPeerSocket->setParent(nullptr);
            pPeerSocket->setSocketOption(QAbstractSocket::SocketOption::LowDelayOption, 1);
            REQUIRE(pPeerSocket->state() == QAbstractSocket::ConnectedState);
            REQUIRE(pPeerSocket->isEncrypted());
            REQUIRE(!server.hasPendingConnections());
            QObject::connect(pPeerSocket.get(), &QSslSocket::errorOccurred, [&](QAbstractSocket::SocketError error) {peerFailedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::disconnected, [&](){peerDisconnectedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::readyRead, [&]()
            {
                peerReceivedData.append(pPeerSocket->readAll());
                peerReceivedDataFromSocketSemaphore.release();
            });
            peerConnectedSemaphore.release();
        });
        const auto serverAddress = QHostAddress("127.10.20.50");
        REQUIRE(server.listen(serverAddress, 0));
        const auto serverPort = server.serverPort();
        REQUIRE(serverPort >= 1024);

        WHEN("TlsSocket connects to server")
        {
            QSemaphore socketConnectedSemaphore;
            QSemaphore socketCompletedHandshakeSemaphore;
            QSemaphore socketFailedSemaphore;
            QSemaphore socketDisconnectedSemaphore;
            QSemaphore socketReceivedDataFromPeerSemaphore;
            QByteArray socketReceivedData;
            std::unique_ptr<TlsSocket> pSocket(new TlsSocket(clientTlsConfiguration));
            Object::connect(pSocket.get(), &TlsSocket::error, [&]() {socketFailedSemaphore.release();});
            Object::connect(pSocket.get(), &TlsSocket::connected, [&](){socketConnectedSemaphore.release();});
            Object::connect(pSocket.get(), &TlsSocket::encrypted, [&](){socketCompletedHandshakeSemaphore.release();});
            Object::connect(pSocket.get(), &TlsSocket::disconnected, [&](){socketDisconnectedSemaphore.release();});
            Object::connect(pSocket.get(), &TlsSocket::receivedData, [&]()
            {
                QByteArray readData;
                readData.resize(pSocket->dataAvailable());
                pSocket->read(readData.data(), readData.size());
                socketReceivedData.append(readData);
                socketReceivedDataFromPeerSemaphore.release();
            });
            pSocket->connect("test.onlocalhost.com", serverPort);

            THEN("peer emits pendingConnectionAvailable with a connected socket that has already completed tls handshake")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));

                AND_THEN("TlsSocket emits connected and then encrypted after completing tls handshake")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketConnectedSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketCompletedHandshakeSemaphore, 10));
                    REQUIRE(pPeerSocket->localAddress().toString().toStdString() == pSocket->peerAddress());
                    REQUIRE(pPeerSocket->localPort() == pSocket->peerPort());
                    REQUIRE(pPeerSocket->peerAddress().toString().toStdString() == pSocket->localAddress());
                    REQUIRE(pPeerSocket->peerPort() == pSocket->localPort());

                    AND_WHEN("client peer sends data to server peer")
                    {
                        const QByteArray dataToSend("Some data");
                        pPeerSocket->write(dataToSend);

                        THEN("server peer receives sent data")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketReceivedDataFromPeerSemaphore, 10));
                            REQUIRE(socketReceivedData == dataToSend);
                        }
                    }
                }
            }
        }
    }
}


SCENARIO("TlsSocket fails as expected")
{
    GIVEN("no server running on any IP related to test.onlocalhost.com")
    {
        WHEN("TlsSocket with valid tls configuration is connected to test.onlocalhost.com")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration tlsConfiguration;
            tlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile);
            tlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket tlsSocket(tlsConfiguration);
            QSemaphore tlsSocketFailedSemaphore;
            Object::connect(&tlsSocket, &TlsSocket::error, [&](){tlsSocketFailedSemaphore.release();});
            tlsSocket.connect("test.onlocalhost.com", 5000);

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketFailedSemaphore, 10));
                REQUIRE(tlsSocket.state() == TcpSocket::State::Unconnected);
                REQUIRE(tlsSocket.errorMessage().starts_with("Failed to connect to test.onlocalhost.com at"));
            }
        }
    }

    GIVEN("a non-existent domain")
    {
        std::string_view nonExistentDomain("nonexistentdomain.thisdomaindoesnotexist");

        WHEN("TlsSocket with valid tls configuration is connected to the non-existent domain")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration tlsConfiguration;
            tlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile);
            tlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket tlsSocket(tlsConfiguration);
            QSemaphore tlsSocketFailedSemaphore;
            Object::connect(&tlsSocket, &TlsSocket::error, [&](){tlsSocketFailedSemaphore.release();});
            tlsSocket.connect(nonExistentDomain, 5000);

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketFailedSemaphore, 10));
                REQUIRE(tlsSocket.state() == TcpSocket::State::Unconnected);
                REQUIRE(tlsSocket.errorMessage() == "Failed to connect to nonexistentdomain.thisdomaindoesnotexist. Could not fetch any address for domain.");
            }
        }
    }

    GIVEN("a server running on IPV6 localhost")
    {
        QTcpServer server;
        REQUIRE(server.listen(QHostAddress::LocalHostIPv6));
        QObject::connect(&server, &QTcpServer::newConnection, [](){FAIL("This code is supposed to be unreachable.");});
        QObject::connect(&server, &QTcpServer::pendingConnectionAvailable, [](){FAIL("This code is supposed to be unreachable.");});

        WHEN("a TlsSocket with valid tls configuration and bounded to an IPV4 address is connected to the IPV6 server")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration tlsConfiguration;
            tlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile);
            tlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket tlsSocket(tlsConfiguration);
            QSemaphore tlsSocketFailedSemaphore;
            Object::connect(&tlsSocket, &TlsSocket::error, [&](){tlsSocketFailedSemaphore.release();});
            tlsSocket.setBindAddressAndPort("127.2.2.5");
            tlsSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketFailedSemaphore, 10));
                REQUIRE(tlsSocket.state() == TcpSocket::State::Unconnected);
                REQUIRE(tlsSocket.errorMessage().starts_with("Failed to connect to [::1]:"));
            }
        }

        WHEN("TlsSocket with valid tls configuration and bounded to a privileged port on IPV6 is connected to the server")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration tlsConfiguration;
            tlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile);
            tlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket tlsSocket(tlsConfiguration);
            QSemaphore tlsSocketFailedSemaphore;
            Object::connect(&tlsSocket, &TlsSocket::error, [&](){tlsSocketFailedSemaphore.release();});
            tlsSocket.setBindAddressAndPort("::1", 443);
            tlsSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketFailedSemaphore, 10));
                REQUIRE(tlsSocket.state() == TcpSocket::State::Unconnected);
                REQUIRE(tlsSocket.errorMessage() == "Failed to bind socket to [::1]:443. POSIX error EACCES(13): Permission denied.");
            }
        }
    }

    GIVEN("a server running on IPV4 localhost")
    {
        QTcpServer server;
        REQUIRE(server.listen(QHostAddress("127.8.8.8")));
        size_t connectionCount = 0;
        QObject::connect(&server, &QTcpServer::newConnection, [&](){while (server.nextPendingConnection() != nullptr) {++connectionCount;}});

        WHEN("a TlsSocket with valid tls configuration and bounded to a IPV6 address is connected to the IPV4 server")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration tlsConfiguration;
            tlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile);
            tlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket tlsSocket(tlsConfiguration);
            QSemaphore tlsSocketFailedSemaphore;
            Object::connect(&tlsSocket, &TlsSocket::error, [&](){tlsSocketFailedSemaphore.release();});
            tlsSocket.setBindAddressAndPort("::1");
            tlsSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketFailedSemaphore, 10));
                REQUIRE(tlsSocket.state() == TcpSocket::State::Unconnected);
                REQUIRE(tlsSocket.errorMessage().starts_with("Failed to connect to 127.8.8.8:"));
                REQUIRE(connectionCount == 0);
            }
        }

        WHEN("TlsSocket with valid tls configuration and bounded to a privileged port on IPV4 is connected to the server")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration tlsConfiguration;
            tlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile);
            tlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket tlsSocket(tlsConfiguration);
            QSemaphore tlsSocketFailedSemaphore;
            Object::connect(&tlsSocket, &TlsSocket::error, [&](){tlsSocketFailedSemaphore.release();});
            tlsSocket.setBindAddressAndPort("127.0.0.1", 443);
            tlsSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketFailedSemaphore, 10));
                REQUIRE(tlsSocket.state() == TcpSocket::State::Unconnected);
                REQUIRE(tlsSocket.errorMessage() == "Failed to bind socket to 127.0.0.1:443. POSIX error EACCES(13): Permission denied.");
                REQUIRE(connectionCount == 0);
            }
        }

        WHEN("TlsSocket bound to an already used address/port pair is connected to server")
        {
            QTcpSocket previouslyConnectedSocket;
            QSemaphore previouslyConnectedSocketSemaphore;
            QObject::connect(&previouslyConnectedSocket, &QTcpSocket::connected, [&](){previouslyConnectedSocketSemaphore.release();});
            previouslyConnectedSocket.connectToHost(server.serverAddress(), server.serverPort());
            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(previouslyConnectedSocketSemaphore, 10));
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration tlsConfiguration;
            tlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile);
            tlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket tlsSocket(tlsConfiguration);
            QSemaphore tlsSocketFailedSemaphore;
            Object::connect(&tlsSocket, &TlsSocket::error, [&](){tlsSocketFailedSemaphore.release();});
            tlsSocket.setBindAddressAndPort(previouslyConnectedSocket.localAddress().toString().toStdString(), previouslyConnectedSocket.localPort());
            tlsSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketFailedSemaphore, 10));
                REQUIRE(tlsSocket.state() == TcpSocket::State::Unconnected);
                REQUIRE(tlsSocket.errorMessage() == std::string("Failed to bind socket to 127.0.0.1:").append(std::to_string(previouslyConnectedSocket.localPort())).append(". POSIX error EADDRINUSE(98): Address already in use."));
                REQUIRE(connectionCount == 1);
            }
        }
    }

    GIVEN("a descriptor that does not represent a socket")
    {
        const auto fileDescriptor = ::memfd_create("Kourier_tls_socket_spec_a_descriptor_that_does_not_represent_a_socket", 0);
        REQUIRE(fileDescriptor >= 0);

        WHEN("a TlsSocket with valid tls configuration is created with the given descriptor")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration tlsConfiguration;
            tlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile);
            tlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket socket(fileDescriptor, tlsConfiguration);

            THEN("socket is created as unconnected")
            {
                REQUIRE(socket.state() == TcpSocket::State::Unconnected);
            }
        }
    }

    GIVEN("an invalid descriptor")
    {
        const int invalidDescriptor = std::numeric_limits<int>::max();

        WHEN("a TlsSocket with valid tls configuration is created with the given descritor")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration tlsConfiguration;
            tlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile);
            tlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket socket(invalidDescriptor, tlsConfiguration);

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

        WHEN("a TlsSocket with valid tls configuration is created with the given descritor")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration tlsConfiguration;
            tlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile);
            tlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket socket(socketDescriptor, tlsConfiguration);

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
        REQUIRE(server.listen(QHostAddress("127.10.20.90")));
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

        WHEN("a TlsSocket with valid tls configuration tries to connect to server")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration tlsConfiguration;
            tlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile);
            tlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket tlsSocket(tlsConfiguration);
            QSemaphore tlsSocketFailedSemaphore;
            Object::connect(&tlsSocket, &TlsSocket::error, [&]() {tlsSocketFailedSemaphore.release();});
            tlsSocket.connect("test.onlocalhost.com", server.serverPort());

            THEN("TlsSocket times out while trying to connect to server")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketFailedSemaphore, 70));
                REQUIRE(tlsSocket.state() == TcpSocket::State::Unconnected);
                std::string expectedErrorMessage("Failed to connect to test.onlocalhost.com at 127.10.20.90:");
                expectedErrorMessage.append(std::to_string(server.serverPort()));
                expectedErrorMessage.append(".");
                REQUIRE(tlsSocket.errorMessage() == expectedErrorMessage);
            }
        }
    }

    GIVEN("a server that does not do tls hanshakes")
    {
        QTcpServer server;
        QObject::connect(&server, &QTcpServer::newConnection, [&]()
        {
            static bool firstRun = true;
            REQUIRE(firstRun);
            firstRun = false;
            REQUIRE(server.nextPendingConnection() != nullptr); // socket has server as parent and will be destroyed by it
            REQUIRE(server.nextPendingConnection() == nullptr);
        });
        REQUIRE(server.listen(QHostAddress("127.10.20.90")));

        WHEN("a TlsSocket with valid tls configuration tries to connect to server")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration tlsConfiguration;
            tlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile);
            tlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket tlsSocket(tlsConfiguration);
            QSemaphore tlsSocketFailedSemaphore;
            Object::connect(&tlsSocket, &TlsSocket::error, [&]() {tlsSocketFailedSemaphore.release();});
            tlsSocket.connect("test.onlocalhost.com", server.serverPort());

            THEN("TlsSocket handshake times out")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketFailedSemaphore, 70));
                REQUIRE(tlsSocket.state() == TcpSocket::State::Unconnected);
                std::string expectedErrorMessage("Failed to connect to test.onlocalhost.com at 127.10.20.90:");
                expectedErrorMessage.append(std::to_string(server.serverPort()));
                expectedErrorMessage.append(". TLS handshake timed out.");
                REQUIRE(tlsSocket.errorMessage() == expectedErrorMessage);
            }
        }
    }

    GIVEN("a client that does not do tls hanshakes")
    {
        const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
        std::string certificateFile;
        std::string privateKeyFile;
        std::string caCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
        TlsConfiguration serverTlsConfiguration;
        serverTlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile);
        serverTlsConfiguration.addCaCertificate(caCertificateFile);
        TlsServer server(serverTlsConfiguration);
        QSemaphore tlsSocketFailedSemaphore;
        std::unique_ptr<TlsSocket> pSocket;
        Object::connect(&server, &TlsServer::newConnection, [&](TlsSocket *pNewSocket)
        {
            REQUIRE(!pSocket);
            pSocket.reset(pNewSocket);
            Object::connect(pSocket.get(), &TlsSocket::error, [&]() {tlsSocketFailedSemaphore.release();});
        });
        const auto serverAddress = QHostAddress("127.10.20.90");
        REQUIRE(server.listen(serverAddress, 0));
        const auto serverPort = server.serverPort();
        REQUIRE(serverPort >= 1024);

        WHEN("client connects to server")
        {
            TcpSocket tcpSocket;
            QSemaphore tcpSocketConnectedSemaphore;
            QSemaphore tcpSocketDisconnectedSemaphore;
            std::string localAddress;
            uint16_t localPort = 0;
            Object::connect(&tcpSocket, &TcpSocket::connected, [&]()
            {
                localAddress = tcpSocket.localAddress();
                localPort = tcpSocket.localPort();
                tcpSocketConnectedSemaphore.release();
            });
            Object::connect(&tcpSocket, &TcpSocket::disconnected, [&]() {tcpSocketDisconnectedSemaphore.release();});
            tcpSocket.connect("127.10.20.90", server.serverPort());

            THEN("TcpSocket connects")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tcpSocketConnectedSemaphore, 1));

                AND_THEN("TlsSocket server peer handshake times out")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketFailedSemaphore, 70));
                    REQUIRE(pSocket->state() == TcpSocket::State::Unconnected);
                    std::string expectedErrorMessage("Failed to connect to ");
                    expectedErrorMessage.append(localAddress);
                    expectedErrorMessage.append(":");
                    expectedErrorMessage.append(std::to_string(localPort));
                    expectedErrorMessage.append(". TLS handshake timed out.");
                    REQUIRE(pSocket->errorMessage() == expectedErrorMessage);

                    AND_THEN("TcpSocket disconnects")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tcpSocketDisconnectedSemaphore, 1));
                        REQUIRE(tcpSocket.errorMessage().empty());
                    }
                }
            }
        }
    }

    GIVEN("a client peer that does not trust the certificate sent from server")
    {
        TlsConfiguration clientTlsConfiguration;
        TlsSocket clientPeer(clientTlsConfiguration);
        QSemaphore clientPeerConnectedSemaphore;
        QSemaphore clientPeerFailedSemaphore;
        Object::connect(&clientPeer, &TlsSocket::connected, [&](){clientPeerConnectedSemaphore.release();});
        Object::connect(&clientPeer, &TlsSocket::encrypted, [](){FAIL("This code is supposed to be unreachable.");});
        Object::connect(&clientPeer, &TlsSocket::error, [&]() {clientPeerFailedSemaphore.release();});
        Object::connect(&clientPeer, &TlsSocket::disconnected, []() {FAIL("This code is supposed to be unreachable.");});
        Object::connect(&clientPeer, &TlsSocket::receivedData, []() {FAIL("This code is supposed to be unreachable.");});
        const auto serverCertificateType = TlsTestCertificates::CertificateType::ECDSA;
        std::string serverCertificateFile;
        std::string serverPrivateKeyFile;
        std::string serverCaCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(serverCertificateType, serverCertificateFile, serverPrivateKeyFile, serverCaCertificateFile);
        TlsConfiguration serverTlsConfiguration;
        serverTlsConfiguration.setCertificateKeyPair(serverCertificateFile, serverPrivateKeyFile);
        serverTlsConfiguration.addCaCertificate(serverCaCertificateFile);
        TlsServer server(serverTlsConfiguration);
        QSemaphore serverPeerDisconnectedSemaphore;
        std::unique_ptr<TlsSocket> pSocket;
        Object::connect(&server, &TlsServer::newConnection, [&](TlsSocket *pNewSocket)
        {
            REQUIRE(!pSocket);
            pSocket.reset(pNewSocket);
            Object::connect(pSocket.get(), &TlsSocket::connected, []() {FAIL("This code is supposed to be unreachable.");});
            Object::connect(pSocket.get(), &TlsSocket::encrypted, []() {FAIL("This code is supposed to be unreachable.");});
            Object::connect(pSocket.get(), &TlsSocket::error, []() {FAIL("This code is supposed to be unreachable.");});
            Object::connect(pSocket.get(), &TlsSocket::disconnected, [&]() {serverPeerDisconnectedSemaphore.release();});
            Object::connect(pSocket.get(), &TlsSocket::receivedData, []() {FAIL("This code is supposed to be unreachable.");});
        });
        const auto serverAddress = QHostAddress("127.10.20.50");
        REQUIRE(server.listen(serverAddress, 0));
        const auto serverPort = server.serverPort();
        REQUIRE(serverPort >= 1024);

        WHEN("client peer tries to connect to server")
        {
            clientPeer.connect("test.onlocalhost.com", serverPort);

            THEN("client peer fails and disconnects from server")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerFailedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerDisconnectedSemaphore, 10));
            }
        }
    }

    GIVEN("a server peer that does not trust the certificate sent from client")
    {
        const auto clientCertificateType = TlsTestCertificates::CertificateType::ECDSA;
        std::string clientCertificateFile;
        std::string clientPrivateKeyFile;
        std::string clientCaCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(clientCertificateType, clientCertificateFile, clientPrivateKeyFile, clientCaCertificateFile);
        std::string clientCertificateContents;
        std::string clientPrivateKeyContents;
        std::string clientPrivateKeyPassword;
        std::string clientCaCertificateContents;
        TlsTestCertificates::getContentsFromCertificateType(clientCertificateType, clientCertificateContents, clientPrivateKeyContents, clientPrivateKeyPassword, clientCaCertificateContents);
        const auto serverCertificateType = TlsTestCertificates::CertificateType::RSA_2048;
        std::string serverCertificateFile;
        std::string serverPrivateKeyFile;
        std::string serverCaCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(serverCertificateType, serverCertificateFile, serverPrivateKeyFile, serverCaCertificateFile);
        std::string serverCertificateContents;
        std::string serverPrivateKeyContents;
        std::string serverPrivateKeyPassword;
        std::string serverCaCertificateContents;
        TlsTestCertificates::getContentsFromCertificateType(serverCertificateType, serverCertificateContents, serverPrivateKeyContents, serverPrivateKeyPassword, serverCaCertificateContents);
        auto sslCaCert = QSslCertificate::fromPath(QString::fromStdString(serverCaCertificateFile));
        REQUIRE(!sslCaCert.isEmpty());
        QSslConfiguration clientTlsConfiguration;
        clientTlsConfiguration.setCaCertificates({sslCaCert});
        auto certChain = QSslCertificate::fromPath(QString::fromStdString(clientCertificateFile));
        REQUIRE(!certChain.isEmpty());
        auto sslCert = QSslCertificate::fromPath(QString::fromStdString(clientCaCertificateFile));
        REQUIRE(!sslCert.isEmpty());
        clientTlsConfiguration.setLocalCertificateChain(certChain);
        QSslKey sslKey(QByteArray(clientPrivateKeyContents.data(), clientPrivateKeyContents.size()), QSsl::Ec);
        REQUIRE(!sslKey.isNull());
        clientTlsConfiguration.setPrivateKey(sslKey);
        TlsConfiguration serverTlsConfiguration;
        serverTlsConfiguration.setPeerVerifyMode(TlsConfiguration::PeerVerifyMode::On);
        serverTlsConfiguration.setCertificateKeyPair(serverCertificateFile, serverPrivateKeyFile, serverPrivateKeyPassword);
        serverTlsConfiguration.addCaCertificate(serverCaCertificateFile);
        TlsServer server(serverTlsConfiguration);
        QSemaphore socketConnectedSemaphore;
        QSemaphore socketFailedSemaphore;
        QSemaphore socketDisconnectedSemaphore;
        QSemaphore socketReceivedDataFromPeerSemaphore;
        QByteArray socketReceivedData;
        std::unique_ptr<TlsSocket> pSocket;
        Object::connect(&server, &TlsServer::newConnection, [&](TlsSocket *pNewSocket)
        {
            pSocket.reset(pNewSocket);
            Object::connect(pSocket.get(), &TlsSocket::encrypted, [&](){FAIL("This code is supposed to be unreachable.");});
            Object::connect(pSocket.get(), &TlsSocket::error, [&]() {socketFailedSemaphore.release();});
            Object::connect(pSocket.get(), &TlsSocket::disconnected, [&socketDisconnectedSemaphore]() {socketDisconnectedSemaphore.release();});
            Object::connect(pSocket.get(), &TlsSocket::receivedData, [&]()
            {
                QByteArray readData;
                readData.resize(pSocket->dataAvailable());
                pSocket->read(readData.data(), readData.size());
                socketReceivedData.append(readData);
                socketReceivedDataFromPeerSemaphore.release();
            });
            socketConnectedSemaphore.release();
        });
        const auto serverAddress = QHostAddress("127.10.20.90");
        REQUIRE(server.listen(serverAddress, 0));
        const auto serverPort = server.serverPort();
        REQUIRE(serverPort >= 1024);

        WHEN("client peer tries to connect to server")
        {
            QSemaphore peerConnectedSemaphore;
            QSemaphore peerCompletedHandshakeSemaphore;
            QSemaphore peerFailedSemaphore;
            QSemaphore peerDisconnectedSemaphore;
            QSemaphore peerReceivedDataFromTcpSocketSemaphore;
            QByteArray peerReceivedData;
            std::unique_ptr<QSslSocket> pPeerSocket(new QSslSocket);
            QObject::connect(pPeerSocket.get(), &QSslSocket::connected, [&]() {pPeerSocket->setSocketOption(QAbstractSocket::SocketOption::LowDelayOption, 1); peerConnectedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::errorOccurred, [&](QAbstractSocket::SocketError error) {peerFailedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::encrypted, [&](){peerCompletedHandshakeSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::disconnected, [&peerDisconnectedSemaphore](){peerDisconnectedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::readyRead, [&]()
            {
                peerReceivedData.append(pPeerSocket->readAll());
                peerReceivedDataFromTcpSocketSemaphore.release();
            });
            pPeerSocket->setSslConfiguration(clientTlsConfiguration);
            pPeerSocket->connectToHostEncrypted("test.onlocalhost.com", serverPort);

            THEN("client peer successfully connects and consider its tls handshake as completed, while server peer fails and closes connection")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerCompletedHandshakeSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketFailedSemaphore, 10));
                REQUIRE(!pSocket->errorMessage().empty())
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                REQUIRE(pPeerSocket->error() == QAbstractSocket::SocketError::RemoteHostClosedError);
            }
        }
    }

    GIVEN("a client peer that does not send any certificate to server that requires client authentication")
    {
        const auto clientCertificateType = TlsTestCertificates::CertificateType::ECDSA;
        std::string clientCertificateFile;
        std::string clientPrivateKeyFile;
        std::string clientCaCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(clientCertificateType, clientCertificateFile, clientPrivateKeyFile, clientCaCertificateFile);
        std::string clientCertificateContents;
        std::string clientPrivateKeyContents;
        std::string clientPrivateKeyPassword;
        std::string clientCaCertificateContents;
        TlsTestCertificates::getContentsFromCertificateType(clientCertificateType, clientCertificateContents, clientPrivateKeyContents, clientPrivateKeyPassword, clientCaCertificateContents);
        const auto serverCertificateType = TlsTestCertificates::CertificateType::RSA_2048;
        std::string serverCertificateFile;
        std::string serverPrivateKeyFile;
        std::string serverCaCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(serverCertificateType, serverCertificateFile, serverPrivateKeyFile, serverCaCertificateFile);
        std::string serverCertificateContents;
        std::string serverPrivateKeyContents;
        std::string serverPrivateKeyPassword;
        std::string serverCaCertificateContents;
        TlsTestCertificates::getContentsFromCertificateType(serverCertificateType, serverCertificateContents, serverPrivateKeyContents, serverPrivateKeyPassword, serverCaCertificateContents);
        auto sslCaCert = QSslCertificate::fromPath(QString::fromStdString(serverCaCertificateFile));
        REQUIRE(!sslCaCert.isEmpty());
        QSslConfiguration clientTlsConfiguration;
        clientTlsConfiguration.setCaCertificates({sslCaCert});
        TlsConfiguration serverTlsConfiguration;
        serverTlsConfiguration.setPeerVerifyMode(TlsConfiguration::PeerVerifyMode::On);
        serverTlsConfiguration.setCertificateKeyPair(serverCertificateFile, serverPrivateKeyFile, serverPrivateKeyPassword);
        serverTlsConfiguration.addCaCertificate(serverCaCertificateFile);
        serverTlsConfiguration.addCaCertificate(clientCaCertificateFile);
        TlsServer server(serverTlsConfiguration);
        QSemaphore socketConnectedSemaphore;
        QSemaphore socketFailedSemaphore;
        QSemaphore socketDisconnectedSemaphore;
        QSemaphore socketReceivedDataFromPeerSemaphore;
        QByteArray socketReceivedData;
        std::unique_ptr<TlsSocket> pSocket;
        Object::connect(&server, &TlsServer::newConnection, [&](TlsSocket *pNewSocket)
        {
            pSocket.reset(pNewSocket);
            Object::connect(pSocket.get(), &TlsSocket::encrypted, [&](){FAIL("This code is supposed to be unreachable.");});
            Object::connect(pSocket.get(), &TlsSocket::error, [&]() {socketFailedSemaphore.release();});
            Object::connect(pSocket.get(), &TlsSocket::disconnected, [&socketDisconnectedSemaphore]() {socketDisconnectedSemaphore.release();});
            Object::connect(pSocket.get(), &TlsSocket::receivedData, [&]()
            {
                QByteArray readData;
                readData.resize(pSocket->dataAvailable());
                pSocket->read(readData.data(), readData.size());
                socketReceivedData.append(readData);
                socketReceivedDataFromPeerSemaphore.release();
            });
            socketConnectedSemaphore.release();
        });
        const auto serverAddress = QHostAddress("127.10.20.90");
        REQUIRE(server.listen(serverAddress, 0));
        const auto serverPort = server.serverPort();
        REQUIRE(serverPort >= 1024);

        WHEN("peer connects to host")
        {
            QSemaphore peerConnectedSemaphore;
            QSemaphore peerCompletedHandshakeSemaphore;
            QSemaphore peerFailedSemaphore;
            QSemaphore peerDisconnectedSemaphore;
            QSemaphore peerReceivedDataFromTcpSocketSemaphore;
            QByteArray peerReceivedData;
            std::unique_ptr<QSslSocket> pPeerSocket(new QSslSocket);
            QObject::connect(pPeerSocket.get(), &QSslSocket::connected, [&]() {pPeerSocket->setSocketOption(QAbstractSocket::SocketOption::LowDelayOption, 1); peerConnectedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::errorOccurred, [&](QAbstractSocket::SocketError error) {peerFailedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::encrypted, [&](){peerCompletedHandshakeSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::disconnected, [&peerDisconnectedSemaphore](){peerDisconnectedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::readyRead, [&]()
            {
                peerReceivedData.append(pPeerSocket->readAll());
                peerReceivedDataFromTcpSocketSemaphore.release();
            });
            pPeerSocket->setSslConfiguration(clientTlsConfiguration);
            pPeerSocket->connectToHostEncrypted("test.onlocalhost.com", serverPort);

            THEN("client peer successfully connects and consider its tls handshake as completed, while server peer fails and closes connection")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerCompletedHandshakeSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketFailedSemaphore, 10));
                REQUIRE(!pSocket->errorMessage().empty())
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                REQUIRE(pPeerSocket->error() == QAbstractSocket::SocketError::RemoteHostClosedError);
            }
        }
    }

    GIVEN("a client peer that sends an invalid TLS record")
    {
        const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
        std::string certificateFile;
        std::string privateKeyFile;
        std::string caCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
        std::string certificateContents;
        std::string privateKeyContents;
        std::string privateKeyPassword;
        std::string caCertificateContents;
        TlsTestCertificates::getContentsFromCertificateType(certificateType, certificateContents, privateKeyContents, privateKeyPassword, caCertificateContents);
        auto sslCaCert = QSslCertificate::fromPath(QString::fromStdString(caCertificateFile));
        REQUIRE(!sslCaCert.isEmpty());
        QSslConfiguration clientTlsConfiguration;
        clientTlsConfiguration.setCaCertificates({sslCaCert});
        TlsConfiguration serverTlsConfiguration;
        serverTlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile, privateKeyPassword);
        serverTlsConfiguration.addCaCertificate(caCertificateFile);
        TlsServer server(serverTlsConfiguration);
        QSemaphore socketCompletedHandshakeSemaphore;
        QSemaphore socketFailedSemaphore;
        QSemaphore socketDisconnectedSemaphore;
        QSemaphore socketReceivedDataFromPeerSemaphore;
        QByteArray socketReceivedData;
        std::unique_ptr<TlsSocket> pSocket;
        Object::connect(&server, &TlsServer::newConnection, [&](TlsSocket *pNewSocket)
        {
            pSocket.reset(pNewSocket);
            Object::connect(pSocket.get(), &TlsSocket::connected, [](){FAIL("This code is supposed to be unreachable.");});
            Object::connect(pSocket.get(), &TlsSocket::encrypted, [&](){socketCompletedHandshakeSemaphore.release();});
            Object::connect(pSocket.get(), &TlsSocket::error, [&]() {socketFailedSemaphore.release();});
            Object::connect(pSocket.get(), &TlsSocket::disconnected, [&socketDisconnectedSemaphore]() {socketDisconnectedSemaphore.release();});
            Object::connect(pSocket.get(), &TlsSocket::receivedData, [&]()
            {
                QByteArray readData;
                readData.resize(pSocket->dataAvailable());
                pSocket->read(readData.data(), readData.size());
                socketReceivedData.append(readData);
                socketReceivedDataFromPeerSemaphore.release();
            });
        });
        const auto serverAddress = QHostAddress("127.10.20.50");
        REQUIRE(server.listen(serverAddress, 0));
        const auto serverPort = server.serverPort();
        REQUIRE(serverPort >= 1024);

        WHEN("peer connects to host")
        {
            QSemaphore peerConnectedSemaphore;
            QSemaphore peerCompletedHandshakeSemaphore;
            QSemaphore peerFailedSemaphore;
            QSemaphore peerDisconnectedSemaphore;
            QSemaphore peerReceivedDataFromTcpSocketSemaphore;
            QByteArray peerReceivedData;
            std::unique_ptr<QSslSocket> pPeerSocket(new QSslSocket);
            QObject::connect(pPeerSocket.get(), &QSslSocket::connected, [&]() {pPeerSocket->setSocketOption(QAbstractSocket::SocketOption::LowDelayOption, 1); peerConnectedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::errorOccurred, [&](QAbstractSocket::SocketError error) {peerFailedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::encrypted, [&peerCompletedHandshakeSemaphore](){peerCompletedHandshakeSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::disconnected, [&peerDisconnectedSemaphore](){peerDisconnectedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::readyRead, [&]()
            {
                peerReceivedData.append(pPeerSocket->readAll());
                peerReceivedDataFromTcpSocketSemaphore.release();
            });
            pPeerSocket->setSslConfiguration(clientTlsConfiguration);
            pPeerSocket->connectToHostEncrypted("test.onlocalhost.com", serverPort);

            THEN("server emits newConnection with a connected socket that does not emit connected but emits encrypted when tls handshake completes")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketCompletedHandshakeSemaphore, 10));
                REQUIRE(pSocket->state() == TcpSocket::State::Connected);
                REQUIRE(pSocket->isEncrypted());

                AND_THEN("connecting peer socket emits connected and encrypted when tls handshake completes")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerCompletedHandshakeSemaphore, 10));
                    REQUIRE(pPeerSocket->localAddress() == QHostAddress(QString::fromStdString(std::string(pSocket->peerAddress()))));
                    REQUIRE(pPeerSocket->localPort() == pSocket->peerPort());
                    REQUIRE(pPeerSocket->peerAddress() == QHostAddress(QString::fromStdString(std::string(pSocket->localAddress()))));
                    REQUIRE(pPeerSocket->peerPort() == pSocket->localPort());

                    AND_WHEN("client peer sends data to server peer")
                    {
                        const QByteArray dataToSend("This is some data that will be sent in a valid TLS record");
                        pPeerSocket->write(dataToSend);

                        THEN("server peer receives sent data")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketReceivedDataFromPeerSemaphore, 10));
                            REQUIRE(socketReceivedData == dataToSend);

                            AND_WHEN("client peer sends an invalid TLS record to server peer")
                            {
                                const QByteArray invalidTlsRecord("This is an invalid TLS record for sure.");
                                const auto clientPeerSocketDescriptor = pPeerSocket->socketDescriptor();
                                REQUIRE(clientPeerSocketDescriptor >= 0);
                                size_t bytesSent = 0;
                                while (bytesSent != invalidTlsRecord.size())
                                {
                                    const auto currentBytesSent = ::write(clientPeerSocketDescriptor, invalidTlsRecord.data() + bytesSent, invalidTlsRecord.size() - bytesSent);
                                    if (currentBytesSent > 0)
                                    {
                                        bytesSent += currentBytesSent;
                                        if (bytesSent == invalidTlsRecord.size())
                                            break;
                                    }
                                }

                                THEN("server fails and disconnects from client peer")
                                {
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketFailedSemaphore, 10));
                                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                                }
                            }
                        }
                    }

                    AND_WHEN("client peer sends an invalid TLS record to server peer")
                    {
                        const QByteArray invalidTlsRecord("This is an invalid TLS record for sure.");
                        const auto clientPeerSocketDescriptor = pPeerSocket->socketDescriptor();
                        REQUIRE(clientPeerSocketDescriptor >= 0);
                        size_t bytesSent = 0;
                        while (bytesSent != invalidTlsRecord.size())
                        {
                            const auto currentBytesSent = ::write(clientPeerSocketDescriptor, invalidTlsRecord.data() + bytesSent, invalidTlsRecord.size() - bytesSent);
                            if (currentBytesSent > 0)
                            {
                                bytesSent += currentBytesSent;
                                if (bytesSent == invalidTlsRecord.size())
                                    break;
                            }
                        }

                        THEN("server fails and disconnects from client peer")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketFailedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                        }
                    }
                }
            }
        }
    }
}

namespace TlsSocketTests
{

class TestServer : public QTcpServer
{
Q_OBJECT
public:
    TestServer() = default;
    ~TestServer() override = default;
signals:
    void newIncomingConnection(qintptr socketDescriptor);

private:
    void incomingConnection(qintptr socketDescriptor) override
    {
        emit newIncomingConnection(socketDescriptor);
    }

};

}

using namespace TlsSocketTests;


SCENARIO("TlsSocket allows connected slots to take any action")
{
    GIVEN("a TlsSocket and a running server")
    {
        const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
        std::string certificateFile;
        std::string privateKeyFile;
        std::string caCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
        auto certChain = QSslCertificate::fromPath(QString::fromStdString(certificateFile));
        REQUIRE(!certChain.isEmpty());
        auto sslCert = QSslCertificate::fromPath(QString::fromStdString(caCertificateFile));
        REQUIRE(!sslCert.isEmpty());
        QSslConfiguration serverTlsConfiguration;
        serverTlsConfiguration.setLocalCertificateChain(certChain);
        QFile file(QString::fromStdString(privateKeyFile));
        REQUIRE(file.open(QIODevice::ReadOnly));
        const auto keyContents = file.readAll();
        REQUIRE(!keyContents.isEmpty());
        QSslKey sslKey(keyContents, QSsl::Ec);
        REQUIRE(!sslKey.isNull());
        serverTlsConfiguration.setPrivateKey(sslKey);
        serverTlsConfiguration.addCaCertificates(sslCert);
        REQUIRE(!serverTlsConfiguration.isNull());
        TlsConfiguration clientTlsConfiguration;
        clientTlsConfiguration.addCaCertificate(caCertificateFile);
        TestServer server;
        QSemaphore peerConnectedSemaphore;
        QSemaphore peerCompletedHandshakeSemaphore;
        QSemaphore peerFailedSemaphore;
        QSemaphore peerDisconnectedSemaphore;
        QSemaphore peerReceivedDataFromSocketSemaphore;
        QByteArray peerReceivedData;
        std::unique_ptr<QSslSocket> pPeerSocket;
        QObject::connect(&server, &TestServer::newIncomingConnection, [&](qintptr socketDescriptor)
        {
            REQUIRE(!server.hasPendingConnections());
            REQUIRE(!pPeerSocket);
            pPeerSocket.reset(new QSslSocket);
            pPeerSocket->setSslConfiguration(serverTlsConfiguration);
            REQUIRE(pPeerSocket->setSocketDescriptor(socketDescriptor));
            REQUIRE(pPeerSocket->state() == QAbstractSocket::SocketState::ConnectedState);
            REQUIRE(!pPeerSocket->isEncrypted());
            pPeerSocket->startServerEncryption();
            pPeerSocket->setParent(nullptr);
            pPeerSocket->setSocketOption(QAbstractSocket::SocketOption::LowDelayOption, 1);
            QObject::connect(pPeerSocket.get(), &QSslSocket::errorOccurred, [&](QAbstractSocket::SocketError error) {peerFailedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::encrypted, [&](){peerCompletedHandshakeSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::disconnected, [&](){peerDisconnectedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::readyRead, [&]()
            {
                if (pPeerSocket)
                    peerReceivedData.append(pPeerSocket->readAll());
                peerReceivedDataFromSocketSemaphore.release();
            });
            peerConnectedSemaphore.release();
        });
        const auto serverAddress = QHostAddress("127.10.20.50");
        REQUIRE(server.listen(serverAddress, 0));
        const auto serverPort = server.serverPort();
        REQUIRE(serverPort >= 1024);
        QSemaphore tlsSocketConnectedSemaphore;
        QSemaphore tlsSocketCompletedHandshakeSemaphore;
        QSemaphore tlsSocketFailedSemaphore;
        QSemaphore tlsSocketDisconnectedSemaphore;
        std::unique_ptr<TlsSocket> pTlsSocket(new TlsSocket(clientTlsConfiguration));
        Object::connect(pTlsSocket.get(), &TlsSocket::error, [&]() {tlsSocketFailedSemaphore.release();});
        Object::connect(pTlsSocket.get(), &TlsSocket::connected, [&](){tlsSocketConnectedSemaphore.release();});
        Object::connect(pTlsSocket.get(), &TlsSocket::encrypted, [&](){tlsSocketCompletedHandshakeSemaphore.release();});
        Object::connect(pTlsSocket.get(), &TlsSocket::disconnected, [&](){tlsSocketDisconnectedSemaphore.release();});

        WHEN("TcpSocket connects to server and disconnects while handling the connected signal")
        {
            Object::connect(pTlsSocket.get(), &TlsSocket::connected, [&]()
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                pTlsSocket->disconnectFromPeer();
            });
            pTlsSocket->connect("test.onlocalhost.com", serverPort);

            THEN("TcpSocket aborts connection and disconnects from peer")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                REQUIRE(!tlsSocketDisconnectedSemaphore.tryAcquire());
                REQUIRE(pPeerSocket->error() == QAbstractSocket::SocketError::RemoteHostClosedError);
            }
        }

        WHEN("TcpSocket connects to server and aborts connection while handling the connected signal")
        {
            Object::connect(pTlsSocket.get(), &TlsSocket::connected, [&]()
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                pTlsSocket->abort();
            });
            pTlsSocket->connect("test.onlocalhost.com", serverPort);

            THEN("TcpSocket connects and then aborts connection")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketConnectedSemaphore, 10));
                REQUIRE(pTlsSocket->state() == TcpSocket::State::Unconnected);
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                REQUIRE(pPeerSocket->error() == QAbstractSocket::SocketError::RemoteHostClosedError);
            }
        }

        WHEN("TcpSocket connects to server and is destroyed while handling the connected signal")
        {
            Object::connect(pTlsSocket.get(), &TlsSocket::connected, [&]()
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                pTlsSocket.release()->scheduleForDeletion();
            });
            pTlsSocket->connect("test.onlocalhost.com", serverPort);

            THEN("TcpSocket connects and then aborts connection")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                REQUIRE(pPeerSocket->error() == QAbstractSocket::SocketError::RemoteHostClosedError);
            }
        }

        WHEN("TcpSocket connects to server and connects again while handling the connected signal")
        {
            auto *pCtxObject = new Object;
            Object::connect(pTlsSocket.get(), &TlsSocket::connected, pCtxObject, [&]()
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                QObject::connect(pPeerSocket.get(), &QSslSocket::disconnected, pPeerSocket.get(), &QObject::deleteLater);
                pPeerSocket.release();
                pTlsSocket->connect("test.onlocalhost.com", serverPort);
                delete pCtxObject;
            });
            pTlsSocket->connect("test.onlocalhost.com", serverPort);

            THEN("TcpSocket connects, aborts and then reconnects to peer")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketConnectedSemaphore, 10));
            }
        }

        WHEN("TcpSocket connects to server and connects to a non-existent server address while handling the connected signal")
        {
            Object::connect(pTlsSocket.get(), &TlsSocket::connected, [&]()
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                QObject::connect(pPeerSocket.get(), &QTcpSocket::disconnected, pPeerSocket.get(), &QObject::deleteLater);
                pPeerSocket.release();
                server.close();
                pTlsSocket->connect("test.onlocalhost.com", serverPort);
            });
            pTlsSocket->connect("test.onlocalhost.com", serverPort);

            THEN("TcpSocket connects, aborts and fails to connect to the non-existent server")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketFailedSemaphore, 10));
                REQUIRE(pTlsSocket->errorMessage().starts_with("Failed to connect to test.onlocalhost.com at"));
            }
        }

        WHEN("TcpSocket connects to server and disconnects while handling the encrypted signal")
        {
            Object::connect(pTlsSocket.get(), &TlsSocket::encrypted, [&]()
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerCompletedHandshakeSemaphore, 10));
                pTlsSocket->disconnectFromPeer();
            });
            pTlsSocket->connect("test.onlocalhost.com", serverPort);

            THEN("TcpSocket connects, completes tls handshake and then disconnects from peer")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketCompletedHandshakeSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                REQUIRE(pPeerSocket->error() == QAbstractSocket::SocketError::RemoteHostClosedError);
            }
        }

        WHEN("TcpSocket connects to server and aborts connection while handling the encrypted signal")
        {
            Object::connect(pTlsSocket.get(), &TlsSocket::encrypted, [&]()
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerCompletedHandshakeSemaphore, 10));
                pTlsSocket->abort();
            });
            pTlsSocket->connect("test.onlocalhost.com", serverPort);

            THEN("TcpSocket connects, completes tls handshake and then aborts connection")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketCompletedHandshakeSemaphore, 10));
                REQUIRE(pTlsSocket->state() == TcpSocket::State::Unconnected);
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                REQUIRE(pPeerSocket->error() == QAbstractSocket::SocketError::RemoteHostClosedError);
            }
        }

        WHEN("TcpSocket connects to server and is destroyed while handling the encrypted signal")
        {
            Object::connect(pTlsSocket.get(), &TlsSocket::encrypted, [&]()
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerCompletedHandshakeSemaphore, 10));
                pTlsSocket.release()->scheduleForDeletion();
            });
            pTlsSocket->connect("test.onlocalhost.com", serverPort);

            THEN("TcpSocket connects, completes handshake and then aborts connection")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketCompletedHandshakeSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                REQUIRE(pPeerSocket->error() == QAbstractSocket::SocketError::RemoteHostClosedError);
            }
        }

        WHEN("TcpSocket connects to server and connects again while handling the encrypted signal")
        {
            auto *pCtxObject = new Object;
            Object::connect(pTlsSocket.get(), &TlsSocket::encrypted, pCtxObject, [&]()
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerCompletedHandshakeSemaphore, 10));
                QObject::connect(pPeerSocket.get(), &QSslSocket::disconnected, pPeerSocket.get(), &QObject::deleteLater);
                pPeerSocket.release();
                pTlsSocket->connect("test.onlocalhost.com", serverPort);
                delete pCtxObject;
            });
            pTlsSocket->connect("test.onlocalhost.com", serverPort);

            THEN("TcpSocket connects, completes tls handshake, aborts and then reconnects to peer")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketCompletedHandshakeSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketCompletedHandshakeSemaphore, 10));
            }
        }

        WHEN("TcpSocket connects to server and connects to a non-existent server address while handling the encrypted signal")
        {
            Object::connect(pTlsSocket.get(), &TlsSocket::encrypted, [&]()
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerCompletedHandshakeSemaphore, 10));
                QObject::connect(pPeerSocket.get(), &QTcpSocket::disconnected, pPeerSocket.get(), &QObject::deleteLater);
                pPeerSocket.release();
                server.close();
                pTlsSocket->connect("test.onlocalhost.com", serverPort);
            });
            pTlsSocket->connect("test.onlocalhost.com", serverPort);

            THEN("TcpSocket connects, completes tls handshake, aborts and fails to connect to the non-existent server")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketCompletedHandshakeSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketFailedSemaphore, 10));
                REQUIRE(pTlsSocket->errorMessage().starts_with("Failed to connect to test.onlocalhost.com at"));
            }
        }

        WHEN("TlsSocket connects to server and completes tls handshake")
        {
            pTlsSocket->connect("test.onlocalhost.com", serverPort);
            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketConnectedSemaphore, 10));
            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketCompletedHandshakeSemaphore, 10));
            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerCompletedHandshakeSemaphore, 10));
            REQUIRE(!tlsSocketDisconnectedSemaphore.tryAcquire());
            REQUIRE(!peerDisconnectedSemaphore.tryAcquire());
            REQUIRE(!tlsSocketFailedSemaphore.tryAcquire());
            REQUIRE(!peerFailedSemaphore.tryAcquire());

            THEN("connected peers can start exchanging data")
            {
                AND_WHEN("connected peer sends some data to TcpSocket")
                {
                    pPeerSocket->write("abcdefgh");

                    AND_WHEN("TlsSocket disconnects while handling the receivedData signal")
                    {
                        Object::connect(pTlsSocket.get(), &TlsSocket::receivedData, [&](){pTlsSocket->disconnectFromPeer();});

                        THEN("TlsSocket disconnects from peer")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketDisconnectedSemaphore, 10));
                            REQUIRE(pTlsSocket->errorMessage().empty());
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                            REQUIRE(pPeerSocket->error() == QAbstractSocket::SocketError::RemoteHostClosedError);
                        }
                    }


                    AND_WHEN("TlsSocket aborts connection while handling the receivedData signal")
                    {
                        Object::connect(pTlsSocket.get(), &TlsSocket::receivedData, [&](){pTlsSocket->abort();});

                        THEN("TlsSocket disconnects from peer")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(pTlsSocket->state() == TcpSocket::State::Unconnected);
                            REQUIRE(pTlsSocket->errorMessage().empty());
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                            REQUIRE(pPeerSocket->error() == QAbstractSocket::SocketError::RemoteHostClosedError);
                        }
                    }

                    AND_WHEN("TlsSocket is destroyed while handling the receivedData signal")
                    {
                        Object::connect(pTlsSocket.get(), &TlsSocket::receivedData, [&](){pTlsSocket.release()->scheduleForDeletion();});

                        THEN("TlsSocket disconnects from peer")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                            REQUIRE(pPeerSocket->error() == QAbstractSocket::SocketError::RemoteHostClosedError);
                        }
                    }

                    AND_WHEN("TlsSocket is reconnected while handling the receivedData signal")
                    {
                        QObject::connect(pPeerSocket.get(), &QTcpSocket::disconnected, pPeerSocket.get(), &QObject::deleteLater);
                        pPeerSocket.release();

                        Object::connect(pTlsSocket.get(), &TlsSocket::receivedData, [&]()
                        {
                            pTlsSocket->connect("test.onlocalhost.com", serverPort);
                        });

                        THEN("TcpSocket aborts and then reconnects")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketConnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketCompletedHandshakeSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                        }
                    }
                }

                AND_WHEN("TlsSocket sends more data than the socket's send buffer can store")
                {
                    const auto socketSendBufferSize = pTlsSocket->getSocketOption(TcpSocket::SocketOption::SendBufferSize);
                    REQUIRE(socketSendBufferSize > 1);
                    const size_t dataSizeToSend = qCeil<double>((3.0 * socketSendBufferSize)/sizeof(quint32))*sizeof(quint32);
                    REQUIRE((dataSizeToSend % sizeof(quint32)) == 0);
                    REQUIRE(dataSizeToSend > socketSendBufferSize);
                    QByteArray dataToSend(dataSizeToSend, ' ');
                    QRandomGenerator64::global()->fillRange<quint32>((quint32*)dataToSend.data(), dataToSend.size()/sizeof(quint32));
                    pTlsSocket->write(dataToSend.data(), dataToSend.size());

                    AND_WHEN("TlsSocket disconnects while handling the sentData signal with data still to be written")
                    {
                        Object::connect(pTlsSocket.get(), &TlsSocket::sentData, [&]()
                        {
                            if (pTlsSocket->dataToWrite())
                                pTlsSocket->disconnectFromPeer();
                        });

                        THEN("TlsSocket disconnects from peer")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                            REQUIRE(pPeerSocket->error() == QAbstractSocket::SocketError::RemoteHostClosedError);
                        }
                    }

                    AND_WHEN("TcpSocket disconnects while handling the sentData signal with no more data still to be written")
                    {
                        Object::connect(pTlsSocket.get(), &TlsSocket::sentData, [&]()
                        {
                            if (!pTlsSocket->dataToWrite())
                                pTlsSocket->disconnectFromPeer();
                        });

                        THEN("TcpSocket disconnects from peer")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                            REQUIRE(pPeerSocket->error() == QAbstractSocket::SocketError::RemoteHostClosedError);
                        }
                    }

                    AND_WHEN("TlsSocket aborts connection while handling the sentData signal with data still to be written")
                    {
                        Object::connect(pTlsSocket.get(), &TlsSocket::sentData, [&]()
                        {
                            if (pTlsSocket->dataToWrite())
                                pTlsSocket->abort();
                        });

                        THEN("TlsSocket disconnects from peer")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(pTlsSocket->state() == TcpSocket::State::Unconnected);
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                            REQUIRE(pPeerSocket->error() == QAbstractSocket::SocketError::RemoteHostClosedError);
                        }
                    }

                    AND_WHEN("TlsSocket aborts connection while handling the sentData signal with no more data data still to be written")
                    {
                        Object::connect(pTlsSocket.get(), &TlsSocket::sentData, [&]()
                        {
                            if (!pTlsSocket->dataToWrite())
                                pTlsSocket->abort();
                        });

                        THEN("TlsSocket disconnects from peer")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(pTlsSocket->state() == TcpSocket::State::Unconnected);
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                            REQUIRE(pPeerSocket->error() == QAbstractSocket::SocketError::RemoteHostClosedError);
                        }
                    }

                    AND_WHEN("TlsSocket is destroyed while handling the sentData signal with data still to be written")
                    {
                        Object::connect(pTlsSocket.get(), &TlsSocket::sentData, [&]()
                        {
                            if (pTlsSocket->dataToWrite())
                                pTlsSocket.release()->scheduleForDeletion();
                        });

                        THEN("TlsSocket disconnects from peer")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                            REQUIRE(pPeerSocket->error() == QAbstractSocket::SocketError::RemoteHostClosedError);
                        }
                    }

                    AND_WHEN("TlsSocket is destroyed while handling the sentData signal with no more data still to be written")
                    {
                        Object::connect(pTlsSocket.get(), &TlsSocket::sentData, [&]()
                        {
                            if (!pTlsSocket->dataToWrite())
                                pTlsSocket.release()->scheduleForDeletion();
                        });

                        THEN("TlsSocket disconnects from peer")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerFailedSemaphore, 10));
                            REQUIRE(pPeerSocket->error() == QAbstractSocket::SocketError::RemoteHostClosedError);
                        }
                    }

                    AND_WHEN("TlsSocket is reconnected while handling the sentData signal with data still to be written")
                    {
                        QObject::connect(pPeerSocket.get(), &QSslSocket::disconnected, pPeerSocket.get(), &QObject::deleteLater);
                        pPeerSocket.release();

                        Object::connect(pTlsSocket.get(), &TlsSocket::sentData, [&]()
                        {
                            if (pTlsSocket->dataToWrite())
                                pTlsSocket->connect("test.onlocalhost.com", serverPort);
                        });

                        THEN("TlsSocket reconnects after disconnecting")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketConnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketCompletedHandshakeSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));

                        }
                    }

                    AND_WHEN("TlsSocket is reconnected while handling the sentData signal with no more data still to be written")
                    {
                        QObject::connect(pPeerSocket.get(), &QSslSocket::disconnected, pPeerSocket.get(), &QObject::deleteLater);
                        pPeerSocket.release();

                        Object::connect(pTlsSocket.get(), &TlsSocket::sentData, [&]()
                        {
                            if (!pTlsSocket->dataToWrite())
                                pTlsSocket->connect("test.onlocalhost.com", serverPort);
                        });

                        THEN("TlsSocket reconnects after disconnecting")
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketConnectedSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketCompletedHandshakeSemaphore, 10));
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerConnectedSemaphore, 10));
                        }
                    }
                }
            }

            AND_WHEN("connected peer disconnects")
            {
                pPeerSocket->disconnectFromHost();
                QSemaphore socketDisconnectedFromPeerSemaphore;

                AND_WHEN("TlsSocket is disconnected while handling the disconnected signal")
                {
                    Object::connect(pTlsSocket.get(), &TlsSocket::disconnected, [&]()
                    {
                        pTlsSocket->disconnectFromPeer();
                        socketDisconnectedFromPeerSemaphore.release();
                    });

                    THEN("no exception is thrown")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketDisconnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedFromPeerSemaphore, 10));
                    }
                }

                AND_WHEN("TcpSocket aborts connection while handling the disconnected signal")
                {
                    Object::connect(pTlsSocket.get(), &TlsSocket::disconnected, [&]()
                    {
                        pTlsSocket->abort();
                        socketDisconnectedFromPeerSemaphore.release();
                    });

                    THEN("no exception is thrown")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketDisconnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedFromPeerSemaphore, 10));
                    }
                }

                AND_WHEN("TlsSocket is destroyed while handling the disconnected signal")
                {
                    Object::connect(pTlsSocket.get(), &TlsSocket::disconnected, [&]()
                    {
                        pTlsSocket.release()->scheduleForDeletion();
                        socketDisconnectedFromPeerSemaphore.release();
                    });

                    THEN("no exception is thrown")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketDisconnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedFromPeerSemaphore, 10));
                    }
                }

                AND_WHEN("TlsSocket is reconnected while handling the disconnected signal")
                {
                    pPeerSocket.release()->deleteLater();
                    Object::connect(pTlsSocket.get(), &TlsSocket::disconnected, [&]()
                    {
                        socketDisconnectedFromPeerSemaphore.release();
                        pTlsSocket->connect("test.onlocalhost.com", serverPort);
                    });

                    THEN("TlsSocket disconnects and then reconnects")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketDisconnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedFromPeerSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketConnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketCompletedHandshakeSemaphore, 10));
                    }
                }
            }
        }

        WHEN("TlsSocket tries to connect to a non-existent server by address")
        {
            auto const serverAddress = QHostAddress("127.1.2.3");
            QTcpSocket socket;
            REQUIRE(socket.bind(serverAddress));
            const auto unusedPortForNow = socket.localPort();
            socket.abort();
            pTlsSocket->connect(serverAddress.toString().toStdString(), unusedPortForNow);
            QSemaphore socketHandledErrorSemaphore;

            AND_WHEN("TlsSocket is disconnected while handling the error signal")
            {
                Object::connect(pTlsSocket.get(), &TlsSocket::error, [&]()
                {
                    REQUIRE(!pTlsSocket->errorMessage().empty());
                    pTlsSocket->disconnectFromPeer();
                    socketHandledErrorSemaphore.release();
                });

                THEN("no exception is thrown")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                }
            }

            AND_WHEN("TlsSocket aborts connection while handling the error signal")
            {
                Object::connect(pTlsSocket.get(), &TlsSocket::error, [&]()
                {
                    REQUIRE(!pTlsSocket->errorMessage().empty());
                    pTlsSocket->abort();
                    socketHandledErrorSemaphore.release();
                });

                THEN("no exception is thrown")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                }
            }

            AND_WHEN("TlsSocket is destroyed while handling the error signal")
            {
                Object::connect(pTlsSocket.get(), &TlsSocket::error, [&]()
                {
                    REQUIRE(!pTlsSocket->errorMessage().empty());
                    pTlsSocket.release()->scheduleForDeletion();
                    socketHandledErrorSemaphore.release();
                });

                THEN("no exception is thrown")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                }
            }

            AND_WHEN("TlsSocket is reconnected to the running server while handling the error signal")
            {
                Object::connect(pTlsSocket.get(), &TcpSocket::error, [&]()
                {
                    REQUIRE(!pTlsSocket->errorMessage().empty());
                    REQUIRE(!tlsSocketConnectedSemaphore.tryAcquire());
                    pTlsSocket->connect("test.onlocalhost.com", serverPort);
                    socketHandledErrorSemaphore.release();
                });

                THEN("TlsSocket reconnects after disconnecting")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketConnectedSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketCompletedHandshakeSemaphore, 10));
                }
            }
        }

        WHEN("TlsSocket tries to connect to a non-existent server by name")
        {
            pTlsSocket->connect("This.domain.name.does.not.exist.for.sure", 3008);
            QSemaphore socketHandledErrorSemaphore;

            AND_WHEN("TlsSocket is disconnected while handling the error signal")
            {
                Object::connect(pTlsSocket.get(), &TlsSocket::error, [&]()
                {
                    REQUIRE(!pTlsSocket->errorMessage().empty());
                    pTlsSocket->disconnectFromPeer();
                    socketHandledErrorSemaphore.release();
                });

                THEN("no exception is thrown")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                }
            }

            AND_WHEN("TlsSocket aborts connection while handling the error signal")
            {
                Object::connect(pTlsSocket.get(), &TlsSocket::error, [&]()
                {
                    REQUIRE(!pTlsSocket->errorMessage().empty());
                    pTlsSocket->abort();
                    socketHandledErrorSemaphore.release();
                });

                THEN("no exception is thrown")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                }
            }

            AND_WHEN("TlsSocket is destroyed while handling the error signal")
            {
                Object::connect(pTlsSocket.get(), &TcpSocket::error, [&]()
                {
                    REQUIRE(!pTlsSocket->errorMessage().empty());
                    pTlsSocket.release()->scheduleForDeletion();
                    socketHandledErrorSemaphore.release();
                });

                THEN("no exception is thrown")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                }
            }

            AND_WHEN("TlsSocket is reconnected to the running server while handling the error signal")
            {
                Object::connect(pTlsSocket.get(), &TcpSocket::error, [&]()
                {
                    REQUIRE(!pTlsSocket->errorMessage().empty());
                    REQUIRE(!tlsSocketConnectedSemaphore.tryAcquire());
                    pTlsSocket->connect("test.onlocalhost.com", serverPort);
                    socketHandledErrorSemaphore.release();
                });

                THEN("TcpSocket reconnects after aborting")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketConnectedSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketCompletedHandshakeSemaphore, 10));
                }
            }
        }

        WHEN("TcpSocket tries to connect to test.onlocalhost.com without any server running")
        {
            pTlsSocket->connect("test.onlocalhost.com", 3008);
            QSemaphore socketHandledErrorSemaphore;

            AND_WHEN("TlsSocket is disconnected while handling the error signal")
            {
                Object::connect(pTlsSocket.get(), &TlsSocket::error, [&]()
                {
                    REQUIRE(!pTlsSocket->errorMessage().empty());
                    pTlsSocket->disconnectFromPeer();
                    socketHandledErrorSemaphore.release();
                });

                THEN("no exception is thrown")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                }
            }

            AND_WHEN("TlsSocket aborts connection while handling the error signal")
            {
                Object::connect(pTlsSocket.get(), &TlsSocket::error, [&]()
                {
                    REQUIRE(!pTlsSocket->errorMessage().empty());
                    pTlsSocket->abort();
                    socketHandledErrorSemaphore.release();
                });

                THEN("no exception is thrown")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                }
            }

            AND_WHEN("TlsSocket is destroyed while handling the error signal")
            {
                Object::connect(pTlsSocket.get(), &TcpSocket::error, [&]()
                {
                    REQUIRE(!pTlsSocket->errorMessage().empty());
                    pTlsSocket.release()->scheduleForDeletion();
                    socketHandledErrorSemaphore.release();
                });

                THEN("no exception is thrown")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                }
            }

            AND_WHEN("TlsSocket is reconnected to the running server while handling the error signal")
            {
                Object::connect(pTlsSocket.get(), &TcpSocket::error, [&]()
                {
                    REQUIRE(!pTlsSocket->errorMessage().empty());
                    REQUIRE(!tlsSocketConnectedSemaphore.tryAcquire());
                    pTlsSocket->connect("test.onlocalhost.com", serverPort);
                    socketHandledErrorSemaphore.release();
                });

                THEN("TcpSocket reconnects after aborting")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketHandledErrorSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketConnectedSemaphore, 10));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketCompletedHandshakeSemaphore, 10));
                }
            }
        }
    }
}


SCENARIO("TlsSockets can be reused")
{
    GIVEN("a QSslServer listening for connections")
    {
        const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
        std::string certificateFile;
        std::string privateKeyFile;
        std::string caCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
        auto certChain = QSslCertificate::fromPath(QString::fromStdString(certificateFile));
        REQUIRE(!certChain.isEmpty());
        auto sslCert = QSslCertificate::fromPath(QString::fromStdString(caCertificateFile));
        REQUIRE(!sslCert.isEmpty());
        QSslConfiguration serverTlsConfiguration;
        serverTlsConfiguration.setLocalCertificateChain(certChain);
        QFile file(QString::fromStdString(privateKeyFile));
        REQUIRE(file.open(QIODevice::ReadOnly));
        const auto keyContents = file.readAll();
        REQUIRE(!keyContents.isEmpty());
        QSslKey sslKey(keyContents, QSsl::Ec);
        REQUIRE(!sslKey.isNull());
        serverTlsConfiguration.setPrivateKey(sslKey);
        serverTlsConfiguration.addCaCertificates(sslCert);
        REQUIRE(!serverTlsConfiguration.isNull());
        TlsConfiguration clientTlsConfiguration;
        clientTlsConfiguration.addCaCertificate(caCertificateFile);
        QSslServer server;
        server.setSslConfiguration(serverTlsConfiguration);
        QSemaphore peerCompletedHandshakeSemaphore;
        QSemaphore peerFailedSemaphore;
        QSemaphore peerDisconnectedSemaphore;
        QSemaphore peerReceivedDataFromSocketSemaphore;
        std::unique_ptr<QSslSocket> pPeerSocket;
        QObject::connect(&server, &QSslServer::pendingConnectionAvailable, [&]()
        {
            REQUIRE(server.hasPendingConnections());
            if (pPeerSocket.get() != nullptr)
                pPeerSocket.release()->setParent(&server);
            pPeerSocket.reset(qobject_cast<QSslSocket*>(server.nextPendingConnection()));
            REQUIRE(pPeerSocket.get() != nullptr);
            pPeerSocket->setParent(nullptr);
            pPeerSocket->setSocketOption(QAbstractSocket::SocketOption::LowDelayOption, 1);
            REQUIRE(!server.hasPendingConnections());
            QObject::connect(pPeerSocket.get(), &QSslSocket::errorOccurred, [&](QAbstractSocket::SocketError error) {peerFailedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::disconnected, [&](){peerDisconnectedSemaphore.release();});
            QObject::connect(pPeerSocket.get(), &QSslSocket::readyRead, [&]()
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
                peerReceivedDataFromSocketSemaphore.release();
            });
            peerCompletedHandshakeSemaphore.release();
        });
        const auto serverAddress = QHostAddress("127.10.20.50");
        REQUIRE(server.listen(serverAddress, 0));
        const auto serverPort = server.serverPort();
        REQUIRE(serverPort >= 1024);

        WHEN("TlsSocket connects to server and play ping pong game three times")
        {
            static constexpr int repCount = 3;
            static constexpr int pingCount = 31;
            QSemaphore socketCompletedHandshakeSemaphore;
            QSemaphore socketFailedSemaphore;
            QSemaphore socketDisconnectedSemaphore;
            QSemaphore socketReceivedDataFromPeerSemaphore;
            int currentPingCount = 0;
            std::unique_ptr<TlsSocket> pSocket(new TlsSocket(clientTlsConfiguration));
            Object::connect(pSocket.get(), &TlsSocket::error, [&]() {socketFailedSemaphore.release();});
            Object::connect(pSocket.get(), &TlsSocket::encrypted, [&]()
            {
                ++currentPingCount;
                pSocket->write("PING\r\n");
                socketCompletedHandshakeSemaphore.release();
            });
            Object::connect(pSocket.get(), &TlsSocket::disconnected, [&]()
            {
                currentPingCount = 0;
                socketDisconnectedSemaphore.release();
            });
            Object::connect(pSocket.get(), &TlsSocket::receivedData, [&]()
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
                pSocket->connect("test.onlocalhost.com", serverPort);
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerCompletedHandshakeSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketCompletedHandshakeSemaphore, 10));
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

    GIVEN("a TlsServer listening for connections")
    {
        const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
        std::string certificateFile;
        std::string privateKeyFile;
        std::string caCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
        TlsConfiguration clientTlsConfiguration;
        clientTlsConfiguration.addCaCertificate(caCertificateFile);
        TlsConfiguration serverTlsConfiguration;
        serverTlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile);
        serverTlsConfiguration.addCaCertificate(caCertificateFile);
        TlsServer server(serverTlsConfiguration);
        QSemaphore peerCompletedHandshakeSemaphore;
        QSemaphore peerFailedSemaphore;
        QSemaphore peerDisconnectedSemaphore;
        QSemaphore peerReceivedDataFromSocketSemaphore;
        std::unique_ptr<TlsSocket> pPeerSocket;
        Object::connect(&server, &TlsServer::newConnection, [&](TlsSocket *pNewSocket)
        {
            pPeerSocket.reset(pNewSocket);
            Object::connect(pPeerSocket.get(), &TlsSocket::encrypted, [&](){peerCompletedHandshakeSemaphore.release();});
            Object::connect(pPeerSocket.get(), &TlsSocket::receivedData, [&]()
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
                peerReceivedDataFromSocketSemaphore.release();
            });
            Object::connect(pPeerSocket.get(), &TlsSocket::disconnected, [&](){peerDisconnectedSemaphore.release();});
            Object::connect(pPeerSocket.get(), &TlsSocket::error, [](){FAIL("This code is supposed to be unreachable.");});
        });
        const auto serverAddress = QHostAddress("127.10.20.50");
        REQUIRE(server.listen(serverAddress, 0));
        const auto serverPort = server.serverPort();
        REQUIRE(serverPort >= 1024);

        WHEN("TlsSocket connects to server and play ping pong game three times")
        {
            static constexpr int repCount = 3;
            static constexpr int pingCount = 31;
            QSemaphore socketCompletedHandshakeSemaphore;
            QSemaphore socketDisconnectedSemaphore;
            QSemaphore socketReceivedDataFromPeerSemaphore;
            int currentPingCount = 0;
            std::unique_ptr<TlsSocket> pSocket(new TlsSocket(clientTlsConfiguration));
            Object::connect(pSocket.get(), &TlsSocket::error, [&]() {FAIL("This code is supposed to be unreachable.");});
            Object::connect(pSocket.get(), &TlsSocket::encrypted, [&]()
            {
                ++currentPingCount;
                pSocket->write("PING\r\n");
                socketCompletedHandshakeSemaphore.release();
            });
            Object::connect(pSocket.get(), &TlsSocket::disconnected, [&]()
            {
                currentPingCount = 0;
                socketDisconnectedSemaphore.release();
            });
            Object::connect(pSocket.get(), &TlsSocket::receivedData, [&]()
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
                pSocket->connect("test.onlocalhost.com", serverPort);
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerCompletedHandshakeSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketCompletedHandshakeSemaphore, 10));
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

        WHEN("TlsSocket connects and then disconnects from server")
        {
            QSemaphore socketCompletedHandshakeSemaphore;
            QSemaphore socketDisconnectedSemaphore;
            QSemaphore socketReceivedDataFromPeerSemaphore;
            TlsSocket socket(clientTlsConfiguration);
            Object::connect(&socket, &TlsSocket::error, [&]() {FAIL("This code is supposed to be unreachable.");});
            Object::connect(&socket, &TlsSocket::encrypted, [&](){socketCompletedHandshakeSemaphore.release(); socket.disconnectFromPeer();});
            Object::connect(&socket, &TlsSocket::disconnected, [&](){socketDisconnectedSemaphore.release();});
            socket.connect("test.onlocalhost.com", serverPort);

            THEN("socket connects and then disconnects")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerCompletedHandshakeSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketCompletedHandshakeSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketDisconnectedSemaphore, 10));

                AND_WHEN("we use server TlsSocket as client to connect to server and play ping pong game three times")
                {
                    static constexpr int repCount = 3;
                    static constexpr int pingCount = 31;
                    int currentPingCount = 0;
                    std::unique_ptr<TlsSocket> pSocket(pPeerSocket.release());
                    REQUIRE(pSocket.get() != nullptr);
                    Object::connect(pSocket.get(), &TlsSocket::error, [&]() {FAIL("This code is supposed to be unreachable.");});
                    Object::connect(pSocket.get(), &TlsSocket::encrypted, [&]()
                    {
                        ++currentPingCount;
                        pSocket->write("PING\r\n");
                        socketCompletedHandshakeSemaphore.release();
                    });
                    Object::connect(pSocket.get(), &TlsSocket::disconnected, [&]()
                    {
                        currentPingCount = 0;
                        socketDisconnectedSemaphore.release();
                    });
                    Object::connect(pSocket.get(), &TlsSocket::receivedData, [&]()
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
                        pSocket->connect("test.onlocalhost.com", serverPort);
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(peerCompletedHandshakeSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(socketCompletedHandshakeSemaphore, 10));
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


namespace TlsSocketTests
{

class ClientTlsSockets : public QObject
{
Q_OBJECT
public:
    ClientTlsSockets(TlsConfiguration tlsClientConfiguration,
                     std::string_view serverHostname,
                     uint16_t serverPort,
                     std::string_view bindAddress,
                     size_t totalConnections,
                     size_t workingConnections,
                     size_t requestsPerWorkingConnection,
                     int a,
                     int b) :
        m_tlsClientConfiguration(tlsClientConfiguration),
        m_serverHostname(serverHostname),
        m_serverPort(serverPort),
        m_bindAddress(bindAddress),
        m_totalConnections(totalConnections),
        m_workingConnections(workingConnections),
        m_requestsPerWorkingConnection(requestsPerWorkingConnection),
        m_a(a),
        m_b(b)
    {
        REQUIRE(!m_serverHostname.empty()
                && (m_serverPort >= 1024)
                && m_totalConnections > 0
                && m_workingConnections > 0
                && (m_totalConnections >= m_workingConnections)
                && m_requestsPerWorkingConnection > 0);
        m_sockets.resize(m_totalConnections);
        for (auto *&pSocket : m_sockets)
            pSocket = new TlsSocket(m_tlsClientConfiguration);
    }
    ~ClientTlsSockets() override = default;

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
            Object::connect(pSocket, &TlsSocket::encrypted, [this]()
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
            Object::connect(pSocket, &TlsSocket::receivedData, [this, pSocket]()
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
            Object::connect(pSocket, &TlsSocket::disconnected, [this, pSocket]()
            {
                REQUIRE(m_hasConnectedAllClients);
                pSocket->scheduleForDeletion();
                if (++m_disconnectionCount == m_totalConnections)
                    emit disconnectedFromServer();
            });
            Object::connect(pSocket, &TlsSocket::error, [this, pSocket]()
            {
                REQUIRE(!m_hasConnectedAllClients);
                // binding failed
                REQUIRE(m_currentBindPort < 65534);
                pSocket->setBindAddressAndPort(m_bindAddress, ++m_currentBindPort);
                pSocket->connect(m_serverHostname, m_serverPort);
            });
            REQUIRE(m_currentBindPort < 65534);
            pSocket->setBindAddressAndPort(m_bindAddress, ++m_currentBindPort);
            pSocket->connect(m_serverHostname, m_serverPort);
        }
    }

private:
    TlsConfiguration m_tlsClientConfiguration;
    size_t m_connectionCount = 0;
    size_t m_responseCount = 0;
    size_t m_disconnectionCount = 0;
    std::vector<TlsSocket*> m_sockets;
    size_t m_currentConnectIndex = 0;
    size_t m_batchConnectionCount = 0;
    const size_t m_connectionsPerBatch = 250;
    std::string m_serverHostname;
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


class ServerTlsSockets : public QObject
{
    Q_OBJECT
public:
    ServerTlsSockets(TlsConfiguration tlsServerConfiguration,
                     std::string_view serverAddress,
                     size_t totalConnections,
                     size_t requestsPerWorkingConnection) :
        m_tlsServerConfiguration(tlsServerConfiguration),
        m_serverAddress(serverAddress),
        m_totalConnections(totalConnections),
        m_requestsPerWorkingConnection(requestsPerWorkingConnection)
    {
        REQUIRE(!m_serverAddress.empty()
                && m_totalConnections > 0);
        m_pTlsServer = new TlsServer(m_tlsServerConfiguration);
        m_pTlsServer->setListenBacklogSize(30000);
        m_pTlsServer->setMaxPendingConnections(30000);
        Object::connect(m_pTlsServer, &TlsServer::newConnection, [this](TlsSocket *pSocket)
        {
            Object::connect(pSocket, &TlsSocket::receivedData, [this, pSocket]()
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
            Object::connect(pSocket, &TlsSocket::disconnected, [this, pSocket]()
            {
                REQUIRE(m_hasConnectedToClients);
                pSocket->scheduleForDeletion();
                if (++m_disconnectionCount == m_totalConnections)
                {
                    m_pTlsServer->scheduleForDeletion();
                    m_pTlsServer = nullptr;
                    emit disconnectedFromClients();
                }
            });
            Object::connect(pSocket, &TlsSocket::error, [this, pSocket]()
            {
                FAIL("This code is supposed to be unreachable.");
            });
            Object::connect(pSocket, &TlsSocket::encrypted, [this]()
            {
                if (++m_connectionCount == m_totalConnections)
                {
                    m_hasConnectedToClients = true;
                    emit connectedToClients();
                }
            });
        });
        REQUIRE(m_pTlsServer->listen(QHostAddress(QString::fromStdString(std::string(m_serverAddress)))));
        m_serverPort = m_pTlsServer->serverPort();
        REQUIRE(m_serverPort > 0);
    }
    ~ServerTlsSockets() override = default;
    uint16_t serverPort() const {return m_serverPort;}

signals:
    void connectedToClients();
    void disconnectedFromClients();

private:
    TlsConfiguration m_tlsServerConfiguration;
    TlsServer *m_pTlsServer = nullptr;
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
    auto pageSize = sysconf(_SC_PAGE_SIZE);
    return (nonProgramMemory - sharedMemory) * pageSize;
}
#endif

}

using namespace TlsSocketTests;


SCENARIO("TlsSocket benchmarks")
{
    const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
    std::string certificateFile;
    std::string privateKeyFile;
    std::string caCertificateFile;
    TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
    TlsConfiguration serverTlsConfiguration;
    serverTlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile);
    serverTlsConfiguration.addCaCertificate(caCertificateFile);
    TlsConfiguration clientTlsConfiguration;
    clientTlsConfiguration.addCaCertificate(caCertificateFile);
    static constexpr std::string_view serverHostname("test.onlocalhost.com");
    static constexpr std::string_view serverAddress("127.10.20.50");
    static constexpr size_t totalConnectionsPerThread = 10000;
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
    std::unique_ptr<AsyncQObject<ServerTlsSockets, TlsConfiguration, std::string_view, size_t, size_t>> server(new AsyncQObject<ServerTlsSockets, TlsConfiguration, std::string_view, size_t, size_t>(serverTlsConfiguration, serverAddress, totalConnections, requestsPerWorkingConnection));
    const auto serverPort = server->get()->serverPort();
    QObject::connect(server->get(), &ServerTlsSockets::connectedToClients, [&](){serverSocketsConnectedSemaphore.release();});
    QObject::connect(server->get(), &ServerTlsSockets::disconnectedFromClients, [&](){serverSocketsDisconnectedSemaphore.release();});
    std::vector<std::unique_ptr<AsyncQObject<ClientTlsSockets, TlsConfiguration, std::string_view, uint16_t, std::string_view, size_t, size_t, size_t, int, int>>> clients(clientThreadCount);
    size_t counter = 0;
    for (auto &client : clients)
    {
        std::string currentBindAddress("127.52.12.");
        currentBindAddress.append(std::to_string(++counter));
        client.reset(new AsyncQObject<ClientTlsSockets, TlsConfiguration, std::string_view, uint16_t, std::string_view, size_t, size_t, size_t, int, int>(clientTlsConfiguration, serverHostname, serverPort, currentBindAddress, totalConnectionsPerThread, workingConnectionsPerThread, requestsPerWorkingConnection, a, b));
    }
    memoryConsumedAfterCreatingClientSockets = getUsedMemory();
    QObject ctxObject;
    for (auto &client : clients)
    {
        QObject::connect(client->get(), &ClientTlsSockets::connectedToServer, &ctxObject, [&]()
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
        QObject::connect(client->get(), &ClientTlsSockets::receivedResponses, &ctxObject, [&]()
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
        QObject::connect(client->get(), &ClientTlsSockets::disconnectedFromServer, &ctxObject, [&]()
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


namespace TlsSocketTests
{

class ClientQSslSockets : public QObject
{
    Q_OBJECT
public:
    ClientQSslSockets(QSslConfiguration clientSslConfiguration,
                      std::string_view serverHostname,
                      uint16_t serverPort,
                      std::string_view bindAddress,
                      size_t totalConnections,
                      size_t workingConnections,
                      size_t requestsPerWorkingConnection,
                      int a,
                      int b) :
        m_clientSslConfiguration(clientSslConfiguration),
        m_serverHostname(QString::fromStdString(std::string(serverHostname))),
        m_serverPort(serverPort),
        m_bindAddress(QHostAddress(QString::fromStdString(std::string(bindAddress)))),
        m_totalConnections(totalConnections),
        m_workingConnections(workingConnections),
        m_requestsPerWorkingConnection(requestsPerWorkingConnection),
        m_a(a),
        m_b(b)
    {
        REQUIRE(!m_serverHostname.isEmpty()
                && !m_bindAddress.isNull()
                && m_serverPort > 0
                && m_totalConnections > 0
                && m_workingConnections > 0
                && (m_totalConnections >= m_workingConnections)
                && m_requestsPerWorkingConnection > 0);
        m_sockets.resize(m_totalConnections);
        for (auto *&pSocket : m_sockets)
        {
            pSocket = new QSslSocket;
            pSocket->setSslConfiguration(m_clientSslConfiguration);
        }
    }
    ~ClientQSslSockets() override = default;

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
            QObject::connect(pSocket, &QSslSocket::encrypted, [this, pSocket]()
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
            QObject::connect(pSocket, &QSslSocket::readyRead, [this, pSocket]()
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
            QObject::connect(pSocket, &QSslSocket::disconnected, [this, pSocket]()
            {
                REQUIRE(m_hasConnectedAllClients);
                pSocket->deleteLater();
                if (++m_disconnectionCount == m_totalConnections)
                    emit disconnectedFromServer();
            });
            while (!pSocket->bind(m_bindAddress, ++m_currentBindPort)) {}
            QObject::connect(pSocket, &QSslSocket::errorOccurred, [this, pSocket]()
            {
                REQUIRE(!m_hasConnectedAllClients);
                REQUIRE(pSocket->error() == QAbstractSocket::SocketError::AddressInUseError);
                // binding failed
                QMetaObject::invokeMethod(this, "reconnectSocket", Qt::QueuedConnection, pSocket);
            });
            pSocket->connectToHostEncrypted(m_serverHostname, m_serverPort);
        }
    }

    void reconnectSocket(QSslSocket *pSocket)
    {
        while (!pSocket->bind(m_bindAddress, ++m_currentBindPort)) {}
        pSocket->connectToHostEncrypted(m_serverHostname, m_serverPort);
    }

private:
    QSslConfiguration m_clientSslConfiguration;
    size_t m_connectionCount = 0;
    size_t m_responseCount = 0;
    size_t m_disconnectionCount = 0;
    std::vector<QSslSocket*> m_sockets;
    size_t m_currentConnectIndex = 0;
    size_t m_batchConnectionCount = 0;
    const size_t m_connectionsPerBatch = 250;
    QString m_serverHostname;
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


class ServerQSslSockets : public QObject
{
    Q_OBJECT
public:
    ServerQSslSockets(QSslConfiguration serverSslConfiguration,
                      std::string_view serverAddress,
                      size_t totalConnections,
                      size_t requestsPerWorkingConnection) :
        m_serverSslConfiguration(serverSslConfiguration),
        m_serverAddress(serverAddress),
        m_totalConnections(totalConnections),
        m_requestsPerWorkingConnection(requestsPerWorkingConnection)
    {
        REQUIRE(!m_serverAddress.empty()
                && m_totalConnections > 0);
        m_pServer = new QSslServer;
        m_pServer->setListenBacklogSize(30000);
        m_pServer->setMaxPendingConnections(30000);
        m_pServer->setHandshakeTimeout(300000);
        m_pServer->setSslConfiguration(m_serverSslConfiguration);
        QObject::connect(m_pServer, &QSslServer::pendingConnectionAvailable, [this]()
        {
            while (m_pServer->hasPendingConnections())
            {
                auto *pSocket = qobject_cast<QSslSocket*>(m_pServer->nextPendingConnection());
                REQUIRE(pSocket != nullptr);
                REQUIRE(pSocket->state() == QAbstractSocket::ConnectedState);
                REQUIRE(pSocket->isEncrypted());
                pSocket->setSocketOption(QAbstractSocket::SocketOption::LowDelayOption, 1);
                pSocket->setSocketOption(QAbstractSocket::SocketOption::KeepAliveOption, 1);
                QObject::connect(pSocket, &QSslSocket::readyRead, [this, pSocket]()
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
                QObject::connect(pSocket, &QSslSocket::disconnected, [this, pSocket]()
                {
                    REQUIRE(m_hasConnectedToAllClients);
                    pSocket->deleteLater();
                    if (++m_disconnectionCount == m_totalConnections)
                    {
                        m_pServer->deleteLater();
                        m_pServer = nullptr;
                        emit disconnectedFromClients();
                    }
                });
                QObject::connect(pSocket, &QSslSocket::errorOccurred, [this, pSocket]()
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
        REQUIRE(m_pServer->listen(QHostAddress(QString::fromStdString(std::string(m_serverAddress)))));
        m_serverPort = m_pServer->serverPort();
        REQUIRE(m_serverPort > 0);
    }
    ~ServerQSslSockets() override = default;
    uint16_t serverPort() const {return m_serverPort;}

signals:
    void connectedToClients();
    void disconnectedFromClients();

private:
    QSslConfiguration m_serverSslConfiguration;
    QSslServer *m_pServer = nullptr;
    size_t m_newIncomingConnectionCount = 0;
    size_t m_connectionCount = 0;
    std::set<QSslSocket*> m_sockets;
    size_t m_disconnectionCount = 0;
    size_t m_errorCount = 0;
    std::string_view m_serverAddress;
    uint16_t m_serverPort = 0;
    const size_t m_totalConnections;
    const size_t m_requestsPerWorkingConnection;
    bool m_hasConnectedToAllClients = false;
};

}


SCENARIO("QSslSocket benchmarks")
{
    const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
    std::string certificateFile;
    std::string privateKeyFile;
    std::string caCertificateFile;
    TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
    std::string certificateFileContents;
    std::string privateKeyFileContents;
    std::string privateKeyPassword;
    std::string caCertificateFileContents;
    TlsTestCertificates::getContentsFromCertificateType(certificateType, certificateFileContents, privateKeyFileContents, privateKeyPassword, caCertificateFileContents);
    QSslConfiguration serverTlsConfiguration;
    const auto localCertificateChain = QSslCertificate::fromData(QByteArray(certificateFileContents.data(), certificateFileContents.size()));
    REQUIRE(!localCertificateChain.isEmpty() && !localCertificateChain[0].isNull());
    serverTlsConfiguration.setLocalCertificateChain(localCertificateChain);
    const auto caCertificateChain = QSslCertificate::fromData(QByteArray(caCertificateFileContents.data(), caCertificateFileContents.size()));
    REQUIRE(!caCertificateChain.isEmpty() && !caCertificateChain[0].isNull());
    serverTlsConfiguration.addCaCertificates(caCertificateChain);
    QSslKey sslKey(QByteArray(privateKeyFileContents.data(), privateKeyFileContents.size()), QSsl::Ec);
    REQUIRE(!sslKey.isNull());
    serverTlsConfiguration.setPrivateKey(sslKey);
    QSslConfiguration clientTlsConfiguration;
    clientTlsConfiguration.addCaCertificates(caCertificateChain);
    const std::string_view serverHostname("test.onlocalhost.com");
    const std::string_view serverAddress("127.10.20.50");
    const size_t totalConnectionsPerThread = 10000;
    const size_t workingConnectionsPerThread = 10000;
    const size_t clientThreadCount = 5;
    const size_t totalConnections = totalConnectionsPerThread * clientThreadCount;
    const size_t requestsPerWorkingConnection = 1000;
    const int a = 5;
    const int b = 3;
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
    std::unique_ptr<AsyncQObject<ServerQSslSockets, QSslConfiguration, std::string_view, size_t, size_t>> server(new AsyncQObject<ServerQSslSockets, QSslConfiguration, std::string_view, size_t, size_t>(serverTlsConfiguration, serverAddress, totalConnections, requestsPerWorkingConnection));
    const auto serverPort = server->get()->serverPort();
    QObject::connect(server->get(), &ServerQSslSockets::connectedToClients, [&](){serverSocketsConnectedSemaphore.release();});
    QObject::connect(server->get(), &ServerQSslSockets::disconnectedFromClients, [&](){serverSocketsDisconnectedSemaphore.release();});
    std::vector<std::unique_ptr<AsyncQObject<ClientQSslSockets, QSslConfiguration, std::string_view, uint16_t, std::string_view, size_t, size_t, size_t, int, int>>> clients(clientThreadCount);
    size_t counter = 0;
    for (auto &client : clients)
    {
        std::string currentBindAddress("127.53.17.");
        currentBindAddress.append(std::to_string(++counter));
        client.reset(new AsyncQObject<ClientQSslSockets, QSslConfiguration, std::string_view, uint16_t, std::string_view, size_t, size_t, size_t, int, int>(clientTlsConfiguration, serverHostname, serverPort, currentBindAddress, totalConnectionsPerThread, workingConnectionsPerThread, requestsPerWorkingConnection, a, b));
    }
    memoryConsumedAfterCreatingClientSockets = getUsedMemory();
    QObject ctxObject;
    for (auto &client : clients)
    {
        QObject::connect(client->get(), &ClientQSslSockets::connectedToServer, &ctxObject, [&]()
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
        QObject::connect(client->get(), &ClientQSslSockets::receivedResponses, &ctxObject, [&]()
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
        QObject::connect(client->get(), &ClientQSslSockets::disconnectedFromServer, &ctxObject, [&]()
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

#include "TlsSocket.spec.moc"
