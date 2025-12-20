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
#include <Tests/Resources/TcpServer.h>
#include <Tests/Resources/TlsServer.h>
#include <Tests/Resources/TlsTestCertificates.h>
#include <Tests/Resources/TestHostNamesFetcher.h>
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
#include <chrono>
#include <memory>
#include <fstream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <Spectator.h>

using Kourier::TcpServer;
using Kourier::TlsServer;
using Kourier::TcpSocket;
using Kourier::TlsSocket;
using Kourier::TlsConfiguration;
using Kourier::Object;
using Kourier::AsyncQObject;
using Spectator::SemaphoreAwaiter;
using Kourier::TestResources::TlsTestCertificateInfo;
using Kourier::TestResources::TlsTestCertificates;
using Kourier::TestResources::TestHostNamesFetcher;


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


SCENARIO("TlsSocket connects to server, sends a PING and gets a PONG as response")
{
    GIVEN("a running server")
    {
        auto [hostName, hostAddresses] = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses();
        const auto connectByName = GENERATE(AS(bool), true, false);
        const auto serverAddress = GENERATE(AS(QHostAddress),
                                            QHostAddress("127.10.20.50"),
                                            QHostAddress("127.10.20.60"),
                                            QHostAddress("127.10.20.70"),
                                            QHostAddress("127.10.20.80"),
                                            QHostAddress("127.10.20.90"),
                                            QHostAddress("::1"));
        REQUIRE(hostAddresses.contains(serverAddress));
        const auto certificateType = GENERATE(AS(TlsTestCertificates::CertificateType),
                                              TlsTestCertificates::CertificateType::RSA_2048,
                                              TlsTestCertificates::CertificateType::RSA_2048_CHAIN,
                                              TlsTestCertificates::CertificateType::RSA_2048_EncryptedPrivateKey,
                                              TlsTestCertificates::CertificateType::RSA_2048_CHAIN_EncryptedPrivateKey,
                                              TlsTestCertificates::CertificateType::ECDSA,
                                              TlsTestCertificates::CertificateType::ECDSA_CHAIN,
                                              TlsTestCertificates::CertificateType::ECDSA_EncryptedPrivateKey,
                                              TlsTestCertificates::CertificateType::ECDSA_CHAIN_EncryptedPrivateKey
                                            );
        std::string certificateFile;
        std::string privateKeyFile;
        std::string caCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
        std::string certificateContents;
        std::string privateKeyContents;
        std::string privateKeyPassword;
        std::string caCertificateContents;
        TlsTestCertificates::getContentsFromCertificateType(certificateType, certificateContents, privateKeyContents, privateKeyPassword, caCertificateContents);
        TlsConfiguration clientTlsConfiguration;
        clientTlsConfiguration.addCaCertificate(caCertificateFile);
        TlsConfiguration serverTlsConfiguration;
        serverTlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile, privateKeyPassword);
        serverTlsConfiguration.addCaCertificate(caCertificateFile);
        TlsServer server(serverTlsConfiguration);
        REQUIRE(server.listen(serverAddress));
        std::unique_ptr<TlsSocket> serverPeer;
        QSemaphore serverPeerConnectedSemaphore;
        QSemaphore serverPeerCompletedHandshakeSemaphore;
        QSemaphore serverPeerDisconnectedSemaphore;
        QSemaphore serverPeerReceivedPingSemaphore;
        QByteArray serverPeerReceivedData;
        Object::connect(&server, &TlsServer::newConnection, [&](TlsSocket *pNewSocket)
            {
                REQUIRE(!serverPeer);
                serverPeer.reset(pNewSocket);
                REQUIRE(!serverPeer->isEncrypted());
                Object::connect(serverPeer.get(), &TlsSocket::connected, [](){FAIL("This code is supposed to be unreachable.");});
                Object::connect(serverPeer.get(), &TlsSocket::encrypted, [&]()
                    {
                        REQUIRE(serverPeer->isEncrypted());
                        serverPeerCompletedHandshakeSemaphore.release();
                    });
                Object::connect(serverPeer.get(), &TlsSocket::disconnected, [&]{serverPeerDisconnectedSemaphore.release();});
                Object::connect(serverPeer.get(), &TlsSocket::receivedData, [&]()
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

        WHEN("TlsSocket connects to server")
        {
            TlsSocket clientPeer(clientTlsConfiguration);
            QSemaphore clientPeerConnectedSemaphore;
            QSemaphore clientPeerCompletedHandshakeSemaphore;
            QSemaphore clientPeerDisconnectedSemaphore;
            QSemaphore clientPeerReceivedPongSemaphore;
            QByteArray clientPeerReceivedData;
            Object::connect(&clientPeer, &TlsSocket::connected, [&]{clientPeerConnectedSemaphore.release();});
            Object::connect(&clientPeer, &TlsSocket::encrypted, [&](){clientPeerCompletedHandshakeSemaphore.release();});
            Object::connect(&clientPeer, &TlsSocket::disconnected, [&]{clientPeerDisconnectedSemaphore.release();});
            Object::connect(&clientPeer, &TlsSocket::receivedData, [&]()
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

            THEN("client connects to server and performs TLS handshake with peer")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerCompletedHandshakeSemaphore, 10));

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


SCENARIO("Connected TlsSocket peers interact with each other")
{
    GIVEN("two connected TlsSockets")
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
        TlsConfiguration clientTlsConfiguration;
        clientTlsConfiguration.addCaCertificate(caCertificateFile);
        TlsConfiguration serverTlsConfiguration;
        serverTlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile, privateKeyPassword);
        serverTlsConfiguration.addCaCertificate(caCertificateFile);
        TlsServer server(serverTlsConfiguration);
        const QHostAddress serverAddress("::1");
        REQUIRE(server.listen(serverAddress));
        std::unique_ptr<TlsSocket> pServerPeer;
        QSemaphore serverPeerConnectedSemaphore;
        QSemaphore serverPeerCompletedTlsHandshakeSemaphore;
        QSemaphore serverPeerDisconnectedSemaphore;
        QSemaphore serverPeerFailedSemaphore;
        QSemaphore serverPeerReceivedDataSemaphore;
        QByteArray receivedServerPeerData;
        Object::connect(&server, &TlsServer::newConnection, [&](TlsSocket *pNewSocket)
            {
                REQUIRE(!pServerPeer);
                pServerPeer.reset(pNewSocket);
                Object::connect(pServerPeer.get(), &TlsSocket::encrypted, [&]{serverPeerCompletedTlsHandshakeSemaphore.release();});
                Object::connect(pServerPeer.get(), &TlsSocket::disconnected, [&]{serverPeerDisconnectedSemaphore.release();});
                Object::connect(pServerPeer.get(), &TlsSocket::error, [&]{serverPeerFailedSemaphore.release();});
                Object::connect(pServerPeer.get(), &TlsSocket::receivedData, [&]()
                    {
                        receivedServerPeerData.append(pServerPeer->readAll());
                        serverPeerReceivedDataSemaphore.release();
                    });
                serverPeerConnectedSemaphore.release();
            });
        std::unique_ptr<TlsSocket> pClientPeer(new TlsSocket(clientTlsConfiguration));
        QSemaphore clientPeerConnectedSemaphore;
        QSemaphore clientPeerCompletedTlsHandshakeSemaphore;
        QSemaphore clientPeerDisconnectedSemaphore;
        QSemaphore clientPeerFailedSemaphore;
        QSemaphore clientPeerReceivedDataSemaphore;
        QByteArray receivedClientPeerData;
        Object::connect(pClientPeer.get(), &TlsSocket::connected, [&]{clientPeerConnectedSemaphore.release();});
        Object::connect(pClientPeer.get(), &TlsSocket::encrypted, [&](){clientPeerCompletedTlsHandshakeSemaphore.release();});
        Object::connect(pClientPeer.get(), &TlsSocket::disconnected, [&]{clientPeerDisconnectedSemaphore.release();});
        Object::connect(pClientPeer.get(), &TlsSocket::error, [&]{clientPeerFailedSemaphore.release();});
        Object::connect(pClientPeer.get(), &TlsSocket::receivedData, [&]()
            {
                receivedClientPeerData.append(pClientPeer->readAll());
                clientPeerReceivedDataSemaphore.release();
            });
        pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerConnectedSemaphore, 10));
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerConnectedSemaphore, 10));
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerCompletedTlsHandshakeSemaphore, 10));
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerCompletedTlsHandshakeSemaphore, 10));

        WHEN("TlsSocket sends data to connected peer")
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

                AND_WHEN("TlsSocket sends more data to connected peer")
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

                        AND_WHEN("TlsSocket disconnects from connected peer")
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

        WHEN("TlsSocket closes connection after sending data to connected peer")
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

        WHEN("TlsSocket aborts after sending data to connected peer")
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

        WHEN("TlsSocket is deleted after sending data to connected peer")
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

        WHEN("TlsSocket disconnects from connected peer")
        {
            const bool isTlsSocketTheClient = GENERATE(AS(bool), true, false);
            auto &pTlsSocket = isTlsSocketTheClient ? pClientPeer : pServerPeer;
            auto &pConnectedPeer = isTlsSocketTheClient ? pServerPeer : pClientPeer;
            pTlsSocket->disconnectFromPeer();

            THEN("both peers disconnect")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerDisconnectedSemaphore, 10));
                REQUIRE(pTlsSocket->errorMessage().empty());
                REQUIRE(pConnectedPeer->errorMessage().empty());

                AND_WHEN("TcpSocket is deleted")
                {
                    auto &tlsSocketFailedSemaphore = isTlsSocketTheClient ? clientPeerFailedSemaphore : serverPeerFailedSemaphore;
                    auto &connectedPeerFailedSemaphore = isTlsSocketTheClient ? serverPeerFailedSemaphore : clientPeerFailedSemaphore;
                    pTlsSocket.reset(nullptr);

                    THEN("neither peer emit any error")
                    {
                        REQUIRE(!SemaphoreAwaiter::signalSlotAwareWait(connectedPeerFailedSemaphore, QDeadlineTimer(1)));
                        REQUIRE(!tlsSocketFailedSemaphore.tryAcquire());
                    }
                }
            }
        }

        WHEN("TlsSocket aborts connection")
        {
            const bool isTlsSocketTheClient = GENERATE(AS(bool), true, false);
            auto &pTlsSocket = isTlsSocketTheClient ? pClientPeer : pServerPeer;
            auto &pConnectedPeer = isTlsSocketTheClient ? pServerPeer : pClientPeer;
            pTlsSocket->abort();

            THEN("connected peer disconnect")
            {
                auto &connectedPeerDisconnectedSemaphore = isTlsSocketTheClient ? serverPeerDisconnectedSemaphore : clientPeerDisconnectedSemaphore;
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(connectedPeerDisconnectedSemaphore, 10));
                REQUIRE(pTlsSocket->errorMessage().empty());
                REQUIRE(pConnectedPeer->errorMessage().empty());

                AND_WHEN("TcpSocket is deleted")
                {
                    auto &tlsSocketFailedSemaphore = isTlsSocketTheClient ? clientPeerFailedSemaphore : serverPeerFailedSemaphore;
                    auto &connectedPeerFailedSemaphore = isTlsSocketTheClient ? serverPeerFailedSemaphore : clientPeerFailedSemaphore;
                    pTlsSocket.reset(nullptr);

                    THEN("neither peer emit any error")
                    {
                        REQUIRE(!SemaphoreAwaiter::signalSlotAwareWait(connectedPeerFailedSemaphore, QDeadlineTimer(1)));
                        REQUIRE(!tlsSocketFailedSemaphore.tryAcquire());
                    }
                }
            }
        }

        WHEN("both peers disconnect")
        {
            const bool isTlsSocketTheClient = GENERATE(AS(bool), true, false);
            auto &pTlsSocket = isTlsSocketTheClient ? pClientPeer : pServerPeer;
            auto &pConnectedPeer = isTlsSocketTheClient ? pServerPeer : pClientPeer;
            pTlsSocket->disconnectFromPeer();
            pConnectedPeer->disconnectFromPeer();

            THEN("both peers disconnect")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerDisconnectedSemaphore, 10));
                REQUIRE(pTlsSocket->errorMessage().empty());
                REQUIRE(pConnectedPeer->errorMessage().empty());

                AND_WHEN("TlsSocket is deleted")
                {
                    auto &tlsSocketFailedSemaphore = isTlsSocketTheClient ? clientPeerFailedSemaphore : serverPeerFailedSemaphore;
                    auto &connectedPeerFailedSemaphore = isTlsSocketTheClient ? serverPeerFailedSemaphore : clientPeerFailedSemaphore;
                    pTlsSocket.reset(nullptr);

                    THEN("neither peer emit any error")
                    {
                        REQUIRE(!SemaphoreAwaiter::signalSlotAwareWait(connectedPeerFailedSemaphore, QDeadlineTimer(1)));
                        REQUIRE(!tlsSocketFailedSemaphore.tryAcquire());
                    }
                }
            }
        }

        WHEN("both peers abort connection")
        {
            const bool isTlsSocketTheClient = GENERATE(AS(bool), true, false);
            auto &pTlsSocket = isTlsSocketTheClient ? pClientPeer : pServerPeer;
            auto &pConnectedPeer = isTlsSocketTheClient ? pServerPeer : pClientPeer;
            pTlsSocket->abort();
            pConnectedPeer->abort();

            THEN("both peers abort connection without errors")
            {
                REQUIRE(pTlsSocket->errorMessage().empty());
                REQUIRE(pConnectedPeer->errorMessage().empty());

                AND_WHEN("TlsSocket is deleted")
                {
                    auto &tlsSocketFailedSemaphore = isTlsSocketTheClient ? clientPeerFailedSemaphore : serverPeerFailedSemaphore;
                    auto &connectedPeerFailedSemaphore = isTlsSocketTheClient ? serverPeerFailedSemaphore : clientPeerFailedSemaphore;
                    pTlsSocket.reset(nullptr);

                    THEN("neither peer emit any error")
                    {
                        REQUIRE(!SemaphoreAwaiter::signalSlotAwareWait(connectedPeerFailedSemaphore, QDeadlineTimer(1)));
                        REQUIRE(!tlsSocketFailedSemaphore.tryAcquire());
                    }
                }
            }
        }

        WHEN("TlsSocket is deleted")
        {
            const bool isTlsSocketTheClient = GENERATE(AS(bool), true, false);
            auto &pTlsSocket = isTlsSocketTheClient ? pClientPeer : pServerPeer;
            auto &pConnectedPeer = isTlsSocketTheClient ? pServerPeer : pClientPeer;
            pTlsSocket.reset(nullptr);

            THEN("connected peer disconnects without errors")
            {
                auto &connectedPeerDisconnectedSemaphore = isTlsSocketTheClient ? serverPeerDisconnectedSemaphore : clientPeerDisconnectedSemaphore;
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(connectedPeerDisconnectedSemaphore, 10));
                auto &tlsSocketFailedSemaphore = isTlsSocketTheClient ? clientPeerFailedSemaphore : serverPeerFailedSemaphore;
                auto &connectedPeerFailedSemaphore = isTlsSocketTheClient ? serverPeerFailedSemaphore : clientPeerFailedSemaphore;
                REQUIRE(!SemaphoreAwaiter::signalSlotAwareWait(connectedPeerFailedSemaphore, QDeadlineTimer(1)));
                REQUIRE(!tlsSocketFailedSemaphore.tryAcquire());
                REQUIRE(pConnectedPeer->errorMessage().empty());
            }
        }
    }
}


SCENARIO("TlsSocket supports client authentication (two-way SSL)")
{
    GIVEN("a running server")
    {
        auto [hostName, hostAddresses] = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses();
        const auto connectByName = GENERATE(AS(bool), true, false);
        const auto serverAddress = GENERATE(AS(QHostAddress),
                                            QHostAddress("127.10.20.50"),
                                            QHostAddress("::1"));
        REQUIRE(hostAddresses.contains(serverAddress));
        const auto clientCertificateType = GENERATE(AS(TlsTestCertificates::CertificateType),
                                                    TlsTestCertificates::CertificateType::RSA_2048);
        std::string clientCertificateFile;
        std::string clientPrivateKeyFile;
        std::string clientCaCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(clientCertificateType, clientCertificateFile, clientPrivateKeyFile, clientCaCertificateFile);
        std::string clientCertificateContents;
        std::string clientPrivateKeyContents;
        std::string clientPrivateKeyPassword;
        std::string clientCaCertificateContents;
        TlsTestCertificates::getContentsFromCertificateType(clientCertificateType, clientCertificateContents, clientPrivateKeyContents, clientPrivateKeyPassword, clientCaCertificateContents);
        const auto serverCertificateType = GENERATE(AS(TlsTestCertificates::CertificateType),
                                                    TlsTestCertificates::CertificateType::ECDSA);
        std::string serverCertificateFile;
        std::string serverPrivateKeyFile;
        std::string serverCaCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(serverCertificateType, serverCertificateFile, serverPrivateKeyFile, serverCaCertificateFile);
        std::string serverCertificateContents;
        std::string serverPrivateKeyContents;
        std::string serverPrivateKeyPassword;
        std::string serverCaCertificateContents;
        TlsTestCertificates::getContentsFromCertificateType(serverCertificateType, serverCertificateContents, serverPrivateKeyContents, serverPrivateKeyPassword, serverCaCertificateContents);
        TlsConfiguration clientTlsConfiguration;
        clientTlsConfiguration.addCaCertificate(clientCaCertificateFile);
        clientTlsConfiguration.addCaCertificate(serverCaCertificateFile);
        clientTlsConfiguration.setCertificateKeyPair(clientCertificateFile, clientPrivateKeyFile, clientPrivateKeyPassword);
        TlsConfiguration serverTlsConfiguration;
        serverTlsConfiguration.setPeerVerifyMode(TlsConfiguration::PeerVerifyMode::On);
        serverTlsConfiguration.setCertificateKeyPair(serverCertificateFile, serverPrivateKeyFile, serverPrivateKeyPassword);
        serverTlsConfiguration.addCaCertificate(serverCaCertificateFile);
        serverTlsConfiguration.addCaCertificate(clientCaCertificateFile);
        TlsServer server(serverTlsConfiguration);
        REQUIRE(server.listen(serverAddress));
        std::unique_ptr<TlsSocket> serverPeer;
        QSemaphore serverPeerConnectedSemaphore;
        QSemaphore serverPeerCompletedHandshakeSemaphore;
        QSemaphore serverPeerDisconnectedSemaphore;
        QSemaphore serverPeerReceivedPingSemaphore;
        QByteArray serverPeerReceivedData;
        Object::connect(&server, &TlsServer::newConnection, [&](TlsSocket *pNewSocket)
            {
                REQUIRE(!serverPeer);
                serverPeer.reset(pNewSocket);
                REQUIRE(!serverPeer->isEncrypted());
                Object::connect(serverPeer.get(), &TlsSocket::connected, [](){FAIL("This code is supposed to be unreachable.");});
                Object::connect(serverPeer.get(), &TlsSocket::encrypted, [&]()
                    {
                        REQUIRE(serverPeer->isEncrypted());
                        serverPeerCompletedHandshakeSemaphore.release();
                    });
                Object::connect(serverPeer.get(), &TlsSocket::disconnected, [&]{serverPeerDisconnectedSemaphore.release();});
                Object::connect(serverPeer.get(), &TlsSocket::receivedData, [&]()
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

        WHEN("TlsSocket connects to server")
        {
            TlsSocket clientPeer(clientTlsConfiguration);
            QSemaphore clientPeerConnectedSemaphore;
            QSemaphore clientPeerCompletedHandshakeSemaphore;
            QSemaphore clientPeerDisconnectedSemaphore;
            QSemaphore clientPeerReceivedPongSemaphore;
            QByteArray clientPeerReceivedData;
            Object::connect(&clientPeer, &TlsSocket::connected, [&]{clientPeerConnectedSemaphore.release();});
            Object::connect(&clientPeer, &TlsSocket::encrypted, [&](){clientPeerCompletedHandshakeSemaphore.release();});
            Object::connect(&clientPeer, &TlsSocket::disconnected, [&]{clientPeerDisconnectedSemaphore.release();});
            Object::connect(&clientPeer, &TlsSocket::receivedData, [&]()
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

            THEN("client connects to server and performs TLS handshake with peer")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerCompletedHandshakeSemaphore, 10));

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


SCENARIO("TlsSocket fails as expected")
{
    GIVEN("no server running on any IP related to host name with IPV4/IPV6 addresses")
    {
        WHEN("TlsSocket is connected to host name with IPV4/IPV6 addresses")
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
            auto hostName = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses().first;
            tlsSocket.connect(hostName, 5000);

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketFailedSemaphore, 10));
                REQUIRE(tlsSocket.state() == TcpSocket::State::Unconnected);
                REQUIRE(tlsSocket.errorMessage().starts_with(std::string("Failed to connect to ").append(hostName).append(" at")));
            }
        }
    }

    GIVEN("a server running on IPV6 localhost")
    {
        TcpServer server;
        auto [hostName, hostAddresses] = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses();
        const QHostAddress serverAddress("::1");
        REQUIRE(hostAddresses.contains(serverAddress));
        REQUIRE(server.listen(serverAddress));
        size_t connectionCount = 0;
        Object::connect(&server, &TcpServer::newConnection, [&](){++connectionCount;});

        WHEN("a TlsSocket with valid TLS configuration and bounded to an IPV4 address is connected to the IPV6 server")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration tlsConfiguration;
            tlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket tlsSocket(tlsConfiguration);
            QSemaphore tlsSocketFailedSemaphore;
            Object::connect(&tlsSocket, &TlsSocket::error, [&](){tlsSocketFailedSemaphore.release();});
            tlsSocket.setBindAddressAndPort("127.2.2.5");
            const auto connectByName = GENERATE(AS(bool), true, false);
            if (connectByName)
                tlsSocket.connect(hostName, server.serverPort());
            else
                tlsSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketFailedSemaphore, 10));
                REQUIRE(tlsSocket.state() == TcpSocket::State::Unconnected);
                const auto expectedErrorMessage = connectByName ? std::string("Failed to connect to ").append(hostName).append(" at 127.10.20.90:") : std::string("Failed to connect to [::1]:");
                REQUIRE(tlsSocket.errorMessage().starts_with(expectedErrorMessage));
                REQUIRE(connectionCount == 0);
            }
        }

        WHEN("TlsSocket with valid TLS configuration and bounded to a privileged port on IPV6 is connected to the server")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration tlsConfiguration;
            tlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket tlsSocket(tlsConfiguration);
            QSemaphore tlsSocketFailedSemaphore;
            Object::connect(&tlsSocket, &TlsSocket::error, [&](){tlsSocketFailedSemaphore.release();});
            tlsSocket.setBindAddressAndPort("::1", 443);
            const auto connectByName = GENERATE(AS(bool), true, false);
            if (connectByName)
                tlsSocket.connect(hostName, server.serverPort());
            else
                tlsSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketFailedSemaphore, 10));
                REQUIRE(tlsSocket.state() == TcpSocket::State::Unconnected);
                REQUIRE(tlsSocket.errorMessage() == "Failed to bind socket to [::1]:443. POSIX error EACCES(13): Permission denied.");
                REQUIRE(connectionCount == 0);
            }
        }

        WHEN("TLSSocket bound to an already used address/port pair is connected to server")
        {
            TcpSocket previouslyConnectedSocket;
            QSemaphore previouslyConnectedSocketSemaphore;
            Object::connect(&previouslyConnectedSocket, &TcpSocket::connected, [&](){previouslyConnectedSocketSemaphore.release();});
            previouslyConnectedSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());
            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(previouslyConnectedSocketSemaphore, 10));
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration tlsConfiguration;
            tlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket tlsSocket(tlsConfiguration);
            QSemaphore tlsSocketFailedSemaphore;
            Object::connect(&tlsSocket, &TlsSocket::error, [&](){tlsSocketFailedSemaphore.release();});
            tlsSocket.setBindAddressAndPort(previouslyConnectedSocket.localAddress(), previouslyConnectedSocket.localPort());
            const auto connectByName = GENERATE(AS(bool), true, false);
            if (connectByName)
                tlsSocket.connect(hostName, server.serverPort());
            else
                tlsSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketFailedSemaphore, 10));
                REQUIRE(tlsSocket.state() == TcpSocket::State::Unconnected);
                REQUIRE(tlsSocket.errorMessage() == std::string("Failed to bind socket to [::1]:").append(std::to_string(previouslyConnectedSocket.localPort())).append(". POSIX error EADDRINUSE(98): Address already in use."));
                REQUIRE(connectionCount == 1);
            }
        }
    }

    GIVEN("a server running on IPV4 localhost")
    {
        TcpServer server;
        auto [hostName, hostAddresses] = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses();
        const QHostAddress serverAddress("127.10.20.90");
        REQUIRE(hostAddresses.contains(serverAddress));
        REQUIRE(server.listen(serverAddress));
        size_t connectionCount = 0;
        Object::connect(&server, &TcpServer::newConnection, [&](){++connectionCount;});

        WHEN("a TlsSocket with valid TLS configuration and bounded to a IPV6 address is connected to the IPV4 server")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration tlsConfiguration;
            tlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket tlsSocket(tlsConfiguration);
            QSemaphore tlsSocketFailedSemaphore;
            Object::connect(&tlsSocket, &TlsSocket::error, [&](){tlsSocketFailedSemaphore.release();});
            tlsSocket.setBindAddressAndPort("::1");
            const auto connectByName = GENERATE(AS(bool), true, false);
            if (connectByName)
                tlsSocket.connect(hostName, server.serverPort());
            else
                tlsSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketFailedSemaphore, 10));
                REQUIRE(tlsSocket.state() == TcpSocket::State::Unconnected);
                const auto expectedErrorMessage = connectByName ? std::string("Failed to connect to ").append(hostName).append(" at ").append(serverAddress.toString().toStdString()).append(":") : std::string("Failed to connect to ").append(serverAddress.toString().toStdString()).append(":");
                REQUIRE(tlsSocket.errorMessage().starts_with(expectedErrorMessage));
                REQUIRE(connectionCount == 0);
            }
        }

        WHEN("TlsSocket with valid TLS configuration and bounded to a privileged port on IPV4 is connected to the server")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration tlsConfiguration;
            tlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket tlsSocket(tlsConfiguration);
            QSemaphore tlsSocketFailedSemaphore;
            Object::connect(&tlsSocket, &TlsSocket::error, [&](){tlsSocketFailedSemaphore.release();});
            tlsSocket.setBindAddressAndPort("127.0.0.1", 443);
            const auto connectByName = GENERATE(AS(bool), true, false);
            if (connectByName)
                tlsSocket.connect(hostName, server.serverPort());
            else
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
            TcpSocket previouslyConnectedSocket;
            QSemaphore previouslyConnectedSocketSemaphore;
            Object::connect(&previouslyConnectedSocket, &TcpSocket::connected, [&](){previouslyConnectedSocketSemaphore.release();});
            previouslyConnectedSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());
            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(previouslyConnectedSocketSemaphore, 10));
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration tlsConfiguration;
            tlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket tlsSocket(tlsConfiguration);
            QSemaphore tlsSocketFailedSemaphore;
            Object::connect(&tlsSocket, &TlsSocket::error, [&](){tlsSocketFailedSemaphore.release();});
            tlsSocket.setBindAddressAndPort(previouslyConnectedSocket.localAddress(), previouslyConnectedSocket.localPort());
            const auto connectByName = GENERATE(AS(bool), true, false);
            if (connectByName)
                tlsSocket.connect(hostName, server.serverPort());
            else
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

        WHEN("a TlsSocket with valid TLS configuration is created with the given descriptor")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration tlsConfiguration;
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

        WHEN("a TlsSocket with valid TLS configuration is created with the given descritor")
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

        WHEN("a TlsSocket with valid TLS configuration is created with the given descritor")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration tlsConfiguration;
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
        auto [hostName, hostAddresses] = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses();
        const QHostAddress serverAddress("127.10.20.90");
        REQUIRE(hostAddresses.contains(serverAddress));
        REQUIRE(server.listen(serverAddress));
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
            isServerAcceptingConnections = SemaphoreAwaiter::signalSlotAwareWait(connectedSemaphore, QDeadlineTimer(10));
        }
        while (isServerAcceptingConnections);
        sockets.front()->abort();

        WHEN("a TlsSocket with valid TLS configuration tries to connect to server")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration tlsConfiguration;
            tlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket tlsSocket(tlsConfiguration);
            tlsSocket.setConnectTimeout(std::chrono::milliseconds(5));
            QSemaphore tlsSocketFailedSemaphore;
            Object::connect(&tlsSocket, &TlsSocket::error, [&]() {tlsSocketFailedSemaphore.release();});
            tlsSocket.connect(hostName, server.serverPort());

            THEN("TlsSocket times out while trying to connect to server")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketFailedSemaphore, 70));
                REQUIRE(tlsSocket.state() == TcpSocket::State::Unconnected);
                const auto expectedErrorMessage = std::string("Failed to connect to ").append(hostName).append(" at ").append(serverAddress.toString().toStdString()).append(":").append(std::to_string(server.serverPort())).append(".");
                REQUIRE(tlsSocket.errorMessage() == expectedErrorMessage);
            }
        }
    }

    GIVEN("a server that does not do TLS hanshakes")
    {
        auto [hostName, hostAddresses] = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses();
        const QHostAddress serverAddress("127.10.20.90");
        REQUIRE(hostAddresses.contains(serverAddress));
        TcpServer server;
        std::unique_ptr<TcpSocket> pServerPeer;
        Object::connect(&server, &TcpServer::newConnection, [&](TcpSocket *pSocket)
            {
                REQUIRE(!pServerPeer);
                pServerPeer.reset(pSocket);
            });
        REQUIRE(server.listen(serverAddress));

        WHEN("a TlsSocket with valid TLS configuration tries to connect to server")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration tlsConfiguration;
            tlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket tlsSocket(tlsConfiguration);
            tlsSocket.setTlsHandshakeTimeout(std::chrono::milliseconds(5));
            QSemaphore tlsSocketFailedSemaphore;
            Object::connect(&tlsSocket, &TlsSocket::error, [&]() {tlsSocketFailedSemaphore.release();});
            tlsSocket.connect(hostName, server.serverPort());

            THEN("TlsSocket handshake times out")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketFailedSemaphore, 70));
                REQUIRE(tlsSocket.state() == TcpSocket::State::Unconnected);
                const auto expectedErrorMessage = std::string("Failed to connect to ")
                                                  .append(hostName)
                                                  .append(" at ")
                                                  .append(serverAddress.toString().toStdString()).append(":")
                                                  .append(std::to_string(server.serverPort()))
                                                  .append(". TLS handshake timed out.");
                REQUIRE(tlsSocket.errorMessage() == expectedErrorMessage);
            }
        }
    }

    GIVEN("a client that does not do TLS hanshakes")
    {
        auto [hostName, hostAddresses] = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses();
        const QHostAddress serverAddress("127.10.20.90");
        REQUIRE(hostAddresses.contains(serverAddress));
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
            pSocket->setTlsHandshakeTimeout(std::chrono::milliseconds(5));
            Object::connect(pSocket.get(), &TlsSocket::error, [&]() {tlsSocketFailedSemaphore.release();});
        });
        REQUIRE(server.listen(serverAddress));

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
            tcpSocket.connect(serverAddress.toString().toStdString(), server.serverPort());

            THEN("TcpSocket connects")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tcpSocketConnectedSemaphore, 1));

                AND_THEN("TlsSocket server peer handshake times out and disconnects from client")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tlsSocketFailedSemaphore, 70));
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tcpSocketDisconnectedSemaphore, 1));
                    REQUIRE(tcpSocket.errorMessage().empty());
                    REQUIRE(pSocket->state() == TcpSocket::State::Unconnected);
                    std::string expectedErrorMessage("Failed to connect to ");
                    expectedErrorMessage.append(localAddress);
                    expectedErrorMessage.append(":");
                    expectedErrorMessage.append(std::to_string(localPort));
                    expectedErrorMessage.append(". TLS handshake timed out.");
                    REQUIRE(pSocket->errorMessage() == expectedErrorMessage);
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
        auto [hostName, hostAddresses] = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses();
        const QHostAddress serverAddress("127.10.20.90");
        REQUIRE(hostAddresses.contains(serverAddress));
        REQUIRE(server.listen(serverAddress));

        WHEN("client peer tries to connect to server")
        {
            clientPeer.connect(hostName, server.serverPort());

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
        TlsConfiguration clientTlsConfiguration;
        clientTlsConfiguration.setCertificateKeyPair(clientCertificateFile, clientPrivateKeyFile, clientPrivateKeyPassword);
        clientTlsConfiguration.addCaCertificate(clientCaCertificateFile);
        clientTlsConfiguration.addCaCertificate(serverCaCertificateFile);
        TlsConfiguration serverTlsConfiguration;
        serverTlsConfiguration.setPeerVerifyMode(TlsConfiguration::PeerVerifyMode::On);
        serverTlsConfiguration.setCertificateKeyPair(serverCertificateFile, serverPrivateKeyFile, serverPrivateKeyPassword);
        serverTlsConfiguration.addCaCertificate(serverCaCertificateFile);
        TlsServer server(serverTlsConfiguration);
        QSemaphore serverPeerConnectedSemaphore;
        QSemaphore serverPeerFailedSemaphore;
        QSemaphore serverPeerDisconnectedSemaphore;
        QSemaphore serverPeerReceivedDataSemaphore;
        QByteArray serverPeerReceivedData;
        std::unique_ptr<TlsSocket> pServerPeer;
        Object::connect(&server, &TlsServer::newConnection, [&](TlsSocket *pNewSocket)
        {
            REQUIRE(!pServerPeer);
            pServerPeer.reset(pNewSocket);
            Object::connect(pServerPeer.get(), &TlsSocket::encrypted, [&](){FAIL("This code is supposed to be unreachable.");});
            Object::connect(pServerPeer.get(), &TlsSocket::error, [&]() {serverPeerFailedSemaphore.release();});
            Object::connect(pServerPeer.get(), &TlsSocket::disconnected, [&serverPeerDisconnectedSemaphore]() {serverPeerDisconnectedSemaphore.release();});
            Object::connect(pServerPeer.get(), &TlsSocket::receivedData, [&]()
            {
                QByteArray readData;
                readData.resize(pServerPeer->dataAvailable());
                pServerPeer->read(readData.data(), readData.size());
                serverPeerReceivedData.append(readData);
                serverPeerReceivedDataSemaphore.release();
            });
            serverPeerConnectedSemaphore.release();
        });
        auto [hostName, hostAddresses] = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses();
        const QHostAddress serverAddress("127.10.20.90");
        REQUIRE(hostAddresses.contains(serverAddress));
        REQUIRE(server.listen(serverAddress));

        WHEN("client peer tries to connect to server")
        {
            QSemaphore clientPeerConnectedSemaphore;
            QSemaphore clientPeerCompletedHandshakeSemaphore;
            QSemaphore clientPeerFailedSemaphore;
            QSemaphore clientPeerDisconnectedSemaphore;
            QSemaphore clientPeerReceivedDataSemaphore;
            QByteArray clientPeerReceivedData;
            std::unique_ptr<TlsSocket> pClientPeer(new TlsSocket(clientTlsConfiguration));
            Object::connect(pClientPeer.get(), &TlsSocket::connected, [&]() {clientPeerConnectedSemaphore.release();});
            Object::connect(pClientPeer.get(), &TlsSocket::error, [&]() {clientPeerFailedSemaphore.release();});
            Object::connect(pClientPeer.get(), &TlsSocket::encrypted, [&](){clientPeerCompletedHandshakeSemaphore.release();});
            Object::connect(pClientPeer.get(), &TlsSocket::disconnected, [&clientPeerDisconnectedSemaphore](){clientPeerDisconnectedSemaphore.release();});
            Object::connect(pClientPeer.get(), &TlsSocket::receivedData, [&]()
            {
                clientPeerReceivedData.append(pClientPeer->readAll());
                clientPeerReceivedDataSemaphore.release();
            });
            pClientPeer->connect(hostName, server.serverPort());

            THEN("client peer successfully connects and consider the TLS handshake as successfully completed, while server peer fails to complete TLS handshake and closes the connection disconnecting the client peer")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerFailedSemaphore, 10));
                REQUIRE(!pServerPeer->errorMessage().empty())
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerDisconnectedSemaphore, 10));
                REQUIRE(pClientPeer->errorMessage().empty());
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
        TlsConfiguration clientTlsConfiguration;
        clientTlsConfiguration.addCaCertificate(serverCaCertificateFile);
        TlsConfiguration serverTlsConfiguration;
        serverTlsConfiguration.setPeerVerifyMode(TlsConfiguration::PeerVerifyMode::On);
        serverTlsConfiguration.setCertificateKeyPair(serverCertificateFile, serverPrivateKeyFile, serverPrivateKeyPassword);
        serverTlsConfiguration.addCaCertificate(serverCaCertificateFile);
        serverTlsConfiguration.addCaCertificate(clientCaCertificateFile);
        TlsServer server(serverTlsConfiguration);
        QSemaphore serverPeerConnectedSemaphore;
        QSemaphore serverPeerFailedSemaphore;
        QSemaphore serverPeerDisconnectedSemaphore;
        QSemaphore serverPeerReceivedDataSemaphore;
        QByteArray serverPeerReceivedData;
        std::unique_ptr<TlsSocket> pServerPeer;
        Object::connect(&server, &TlsServer::newConnection, [&](TlsSocket *pNewSocket)
        {
            pServerPeer.reset(pNewSocket);
            Object::connect(pServerPeer.get(), &TlsSocket::encrypted, [&](){FAIL("This code is supposed to be unreachable.");});
            Object::connect(pServerPeer.get(), &TlsSocket::error, [&]() {serverPeerFailedSemaphore.release();});
            Object::connect(pServerPeer.get(), &TlsSocket::disconnected, [&serverPeerDisconnectedSemaphore]() {serverPeerDisconnectedSemaphore.release();});
            Object::connect(pServerPeer.get(), &TlsSocket::receivedData, [&]()
            {
                QByteArray readData;
                readData.resize(pServerPeer->dataAvailable());
                pServerPeer->read(readData.data(), readData.size());
                serverPeerReceivedData.append(readData);
                serverPeerReceivedDataSemaphore.release();
            });
            serverPeerConnectedSemaphore.release();
        });
        auto [hostName, hostAddresses] = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses();
        const QHostAddress serverAddress("127.10.20.90");
        REQUIRE(hostAddresses.contains(serverAddress));
        REQUIRE(server.listen(serverAddress));

        WHEN("client peer connects to host")
        {
            QSemaphore clientPeerConnectedSemaphore;
            QSemaphore clientPeerCompletedHandshakeSemaphore;
            QSemaphore clientPeerFailedSemaphore;
            QSemaphore clientPeerDisconnectedSemaphore;
            QSemaphore clientPeerReceivedDataSemaphore;
            QByteArray clientPeerReceivedData;
            std::unique_ptr<TlsSocket> pClientPeer(new TlsSocket(clientTlsConfiguration));
            Object::connect(pClientPeer.get(), &TlsSocket::connected, [&]() {clientPeerConnectedSemaphore.release();});
            Object::connect(pClientPeer.get(), &TlsSocket::error, [&]() {clientPeerFailedSemaphore.release();});
            Object::connect(pClientPeer.get(), &TlsSocket::encrypted, [&](){clientPeerCompletedHandshakeSemaphore.release();});
            Object::connect(pClientPeer.get(), &TlsSocket::disconnected, [&clientPeerDisconnectedSemaphore](){clientPeerDisconnectedSemaphore.release();});
            Object::connect(pClientPeer.get(), &TlsSocket::receivedData, [&]()
            {
                clientPeerReceivedData.append(pClientPeer->readAll());
                clientPeerReceivedDataSemaphore.release();
            });
            pClientPeer->connect(hostName, server.serverPort());

            THEN("client peer successfully connects and consider it's TLS handshake as successfully completed, while server peer fails and closes connection disconnecting client peer")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerFailedSemaphore, 10));
                REQUIRE(!pServerPeer->errorMessage().empty())
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerDisconnectedSemaphore, 10));
                REQUIRE(pClientPeer->errorMessage().empty());
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
        TlsConfiguration clientTlsConfiguration;
        clientTlsConfiguration.addCaCertificate(caCertificateFile);
        TlsConfiguration serverTlsConfiguration;
        serverTlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile, privateKeyPassword);
        serverTlsConfiguration.addCaCertificate(caCertificateFile);
        TlsServer server(serverTlsConfiguration);
        QSemaphore serverPeerCompletedHandshakeSemaphore;
        QSemaphore serverPeerFailedSemaphore;
        QSemaphore serverPeerDisconnectedSemaphore;
        QSemaphore serverPeerReceivedDataSemaphore;
        QByteArray serverPeerReceivedData;
        std::unique_ptr<TlsSocket> pServerPeer;
        Object::connect(&server, &TlsServer::newConnection, [&](TlsSocket *pNewSocket)
        {
            pServerPeer.reset(pNewSocket);
            Object::connect(pServerPeer.get(), &TlsSocket::connected, [](){FAIL("This code is supposed to be unreachable.");});
            Object::connect(pServerPeer.get(), &TlsSocket::encrypted, [&](){serverPeerCompletedHandshakeSemaphore.release();});
            Object::connect(pServerPeer.get(), &TlsSocket::error, [&]() {serverPeerFailedSemaphore.release();});
            Object::connect(pServerPeer.get(), &TlsSocket::disconnected, [&serverPeerDisconnectedSemaphore]() {serverPeerDisconnectedSemaphore.release();});
            Object::connect(pServerPeer.get(), &TlsSocket::receivedData, [&]()
            {
                serverPeerReceivedData.append(pServerPeer->readAll());
                serverPeerReceivedDataSemaphore.release();
            });
        });
        auto [hostName, hostAddresses] = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses();
        const QHostAddress serverAddress("127.10.20.90");
        REQUIRE(hostAddresses.contains(serverAddress));
        REQUIRE(server.listen(serverAddress));

        WHEN("peer connects to host")
        {
            QSemaphore clientPeerConnectedSemaphore;
            QSemaphore clientPeerCompletedHandshakeSemaphore;
            QSemaphore clientPeerFailedSemaphore;
            QSemaphore clientPeerDisconnectedSemaphore;
            QSemaphore clientPeerReceivedDataSemaphore;
            QByteArray clientPeerReceivedData;
            std::unique_ptr<TlsSocket> pClientPeer(new TlsSocket(clientTlsConfiguration));
            Object::connect(pClientPeer.get(), &TlsSocket::connected, [&]() {clientPeerConnectedSemaphore.release();});
            Object::connect(pClientPeer.get(), &TlsSocket::error, [&]() {clientPeerFailedSemaphore.release();});
            Object::connect(pClientPeer.get(), &TlsSocket::encrypted, [&clientPeerCompletedHandshakeSemaphore](){clientPeerCompletedHandshakeSemaphore.release();});
            Object::connect(pClientPeer.get(), &TlsSocket::disconnected, [&clientPeerDisconnectedSemaphore](){clientPeerDisconnectedSemaphore.release();});
            Object::connect(pClientPeer.get(), &TlsSocket::receivedData, [&]()
            {
                clientPeerReceivedData.append(pClientPeer->readAll());
                clientPeerReceivedDataSemaphore.release();
            });
            pClientPeer->connect(hostName, server.serverPort());

            THEN("both peers connect and complete TLS handshake successfully")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(pClientPeer->state() == TcpSocket::State::Connected);
                REQUIRE(pClientPeer->isEncrypted());
                REQUIRE(pServerPeer->state() == TcpSocket::State::Connected);
                REQUIRE(pServerPeer->isEncrypted());
                REQUIRE(pClientPeer->localAddress() == pServerPeer->peerAddress());
                REQUIRE(pClientPeer->localPort() == pServerPeer->peerPort());
                REQUIRE(pClientPeer->peerAddress() == pServerPeer->localAddress());
                REQUIRE(pClientPeer->peerPort() == pServerPeer->localPort());

                AND_WHEN("client peer sends an invalid TLS record to server peer")
                {
                    const auto sendValidDataFirst = GENERATE(AS(bool), true, false);
                    if (sendValidDataFirst)
                    {
                        const QByteArray dataToSend("This is some data that will be sent in a valid TLS record");
                        pClientPeer->write(dataToSend);
                        do
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerReceivedDataSemaphore, 10));
                        } while (serverPeerReceivedData != dataToSend);
                    }
                    const QByteArray invalidTlsRecord("This is an invalid TLS record for sure.");
                    const auto clientPeerSocketDescriptor = pClientPeer->fileDescriptor();
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
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverPeerFailedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientPeerDisconnectedSemaphore, 10));
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
                    m_pTlsServer->close();
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
    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientSocketsDisconnectedSemaphore, 10000));
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
                    m_pServer->close();
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
    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientSocketsDisconnectedSemaphore, 10000));
    WARN(QByteArray("Memory consumed after creating client sockets: ").append(QByteArray::number(memoryConsumedAfterCreatingClientSockets)));
    WARN(QByteArray("Memory consumed after connecting: ").append(QByteArray::number(memoryConsumedAfterConnecting)));
    WARN(QByteArray("Memory consumed after responses: ").append(QByteArray::number(memoryConsumedAfterResponses)));
    WARN(QByteArray("Memory consumed after disconnecting: ").append(QByteArray::number(memoryConsumedAfterDisconnecting)));
    WARN(QByteArray("Connections per second: ").append(QByteArray::number(connectionsPerSecond)));
    WARN(QByteArray("Requests per second: ").append(QByteArray::number(requestsPerSecond)));
    WARN(QByteArray("Disconnections per second: ").append(QByteArray::number(disconnectionsPerSecond)));
}

#include "TlsSocket.spec.moc"
