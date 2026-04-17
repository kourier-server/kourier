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
#include <Spectator.h>
#include "Core/TlsConfiguration.h"
#include <Tests/Resources/TcpServer.h>
#include <Tests/Resources/TlsServer.h>
#include <Tests/Resources/TlsTestCertificates.h>
#include <Tests/Resources/TestHostNamesFetcher.h>
#include <QRandomGenerator64>
#include <QList>
#include <QSemaphore>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <chrono>
#include <memory>
#include <sys/socket.h>
#include <sys/mman.h>

using Kourier::TcpServer;
using Kourier::TlsServer;
using Kourier::TcpSocket;
using Kourier::TlsSocket;
using Kourier::TlsConfiguration;
using Kourier::Object;
using Kourier::TestResources::TlsTestCertificateInfo;
using Kourier::TestResources::TlsTestCertificates;
using Kourier::TestResources::TestHostNamesFetcher;


namespace TlsSocketTests
{
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
                Object::connect(serverPeer.get(), &TlsSocket::connected, [](){Spectator::FAIL("This code is supposed to be unreachable.");});
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

        WHEN("client peer connects to server")
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

            THEN("peers connect and perform TLS handshake")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(clientPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerCompletedHandshakeSemaphore, 10));

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


SCENARIO("Connected TlsSocket peers interact with each other")
{
    GIVEN("two connected peers")
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
        REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
        REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
        REQUIRE(TRY_ACQUIRE(clientPeerCompletedTlsHandshakeSemaphore, 10));
        REQUIRE(TRY_ACQUIRE(serverPeerCompletedTlsHandshakeSemaphore, 10));

        WHEN("sending peer sends data to receiving peer")
        {
            const auto dataToSend = GENERATE(AS(QByteArray),
                                                QByteArray("a"),
                                                QByteArray("abcdefgh"),
                                                largeData);
            const bool isClientTheSendingPeer = GENERATE(AS(bool), true, false);
            auto &pSendingPeer = isClientTheSendingPeer ? pClientPeer : pServerPeer;
            auto &pReceivingPeer = isClientTheSendingPeer ? pServerPeer : pClientPeer;
            pSendingPeer->write(dataToSend);

            THEN("receiving peer receives sent data")
            {
                auto &receivedData = isClientTheSendingPeer ? receivedServerPeerData : receivedClientPeerData;
                auto &connectedPeerReceivedDataSemaphore = isClientTheSendingPeer ? serverPeerReceivedDataSemaphore : clientPeerReceivedDataSemaphore;
                while(dataToSend != receivedData)
                {
                    REQUIRE(TRY_ACQUIRE(connectedPeerReceivedDataSemaphore, 1));
                }

                AND_WHEN("sending peer sends more data to the receiving peer")
                {
                    receivedData.clear();
                    const QByteArray someMoreData("0123456789");
                    pSendingPeer->write(someMoreData);

                    THEN("receiving peer receives sent data")
                    {
                        while(someMoreData != receivedData)
                        {
                            REQUIRE(TRY_ACQUIRE(connectedPeerReceivedDataSemaphore, 1));
                        }

                        AND_WHEN("sending peer disconnects from the receiving peer")
                        {
                            pSendingPeer->disconnectFromPeer();

                            THEN("peers disconnect")
                            {
                                REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                            }
                        }
                    }
                }
            }
        }

        WHEN("sending peer closes connection after sending data to receiving peer")
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

            THEN("receiving peer receives sent data")
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

        WHEN("sending peer aborts the connection after sending data to the receiving peer")
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

            THEN("peers disconnects without emitting any errors")
            {
                auto &connectedPeerDisconnectedSemaphore = isClientTheSendingPeer ? serverPeerDisconnectedSemaphore : clientPeerDisconnectedSemaphore;
                REQUIRE(TRY_ACQUIRE(connectedPeerDisconnectedSemaphore, 10));
                REQUIRE(pSendingPeer->errorMessage().empty());
                REQUIRE(pReceivingPeer->errorMessage().empty());
            }
        }

        WHEN("sending peer is deleted after sending data to the receiving peer")
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

            THEN("sending peer aborts the connection during destruction disconnecting the receiving peer which does not emit any errors")
            {
                auto &connectedPeerDisconnectedSemaphore = isClientTheSendingPeer ? serverPeerDisconnectedSemaphore : clientPeerDisconnectedSemaphore;
                REQUIRE(TRY_ACQUIRE(connectedPeerDisconnectedSemaphore, 10));
                REQUIRE(pReceivingPeer->errorMessage().empty());
            }
        }

        WHEN("a peer disconnects from the connected peer")
        {
            const bool isTlsSocketTheClient = GENERATE(AS(bool), true, false);
            auto &pTlsSocket = isTlsSocketTheClient ? pClientPeer : pServerPeer;
            auto &pConnectedPeer = isTlsSocketTheClient ? pServerPeer : pClientPeer;
            pTlsSocket->disconnectFromPeer();

            THEN("peers disconnect")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                REQUIRE(pTlsSocket->errorMessage().empty());
                REQUIRE(pConnectedPeer->errorMessage().empty());

                AND_WHEN("peer that initiated disconnection is deleted")
                {
                    auto &tlsSocketFailedSemaphore = isTlsSocketTheClient ? clientPeerFailedSemaphore : serverPeerFailedSemaphore;
                    auto &connectedPeerFailedSemaphore = isTlsSocketTheClient ? serverPeerFailedSemaphore : clientPeerFailedSemaphore;
                    pTlsSocket.reset(nullptr);

                    THEN("neither peer emit any error")
                    {
                        REQUIRE(!TRY_ACQUIRE(connectedPeerFailedSemaphore, QDeadlineTimer(1)));
                        REQUIRE(!tlsSocketFailedSemaphore.tryAcquire());
                    }
                }
            }
        }

        WHEN("a peer aborts the connection with connected peer")
        {
            const bool isTlsSocketTheClient = GENERATE(AS(bool), true, false);
            auto &pTlsSocket = isTlsSocketTheClient ? pClientPeer : pServerPeer;
            auto &pConnectedPeer = isTlsSocketTheClient ? pServerPeer : pClientPeer;
            pTlsSocket->abort();

            THEN("connected peer disconnect")
            {
                auto &connectedPeerDisconnectedSemaphore = isTlsSocketTheClient ? serverPeerDisconnectedSemaphore : clientPeerDisconnectedSemaphore;
                REQUIRE(TRY_ACQUIRE(connectedPeerDisconnectedSemaphore, 10));
                REQUIRE(pTlsSocket->errorMessage().empty());
                REQUIRE(pConnectedPeer->errorMessage().empty());

                AND_WHEN("peer that aborted the connection is deleted")
                {
                    auto &tlsSocketFailedSemaphore = isTlsSocketTheClient ? clientPeerFailedSemaphore : serverPeerFailedSemaphore;
                    auto &connectedPeerFailedSemaphore = isTlsSocketTheClient ? serverPeerFailedSemaphore : clientPeerFailedSemaphore;
                    pTlsSocket.reset(nullptr);

                    THEN("neither peer emit any error")
                    {
                        REQUIRE(!TRY_ACQUIRE(connectedPeerFailedSemaphore, QDeadlineTimer(1)));
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

            THEN("peers disconnect")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                REQUIRE(pTlsSocket->errorMessage().empty());
                REQUIRE(pConnectedPeer->errorMessage().empty());

                AND_WHEN("one of the peers is deleted")
                {
                    auto &tlsSocketFailedSemaphore = isTlsSocketTheClient ? clientPeerFailedSemaphore : serverPeerFailedSemaphore;
                    auto &connectedPeerFailedSemaphore = isTlsSocketTheClient ? serverPeerFailedSemaphore : clientPeerFailedSemaphore;
                    pTlsSocket.reset(nullptr);

                    THEN("neither peer emit any error")
                    {
                        REQUIRE(!TRY_ACQUIRE(connectedPeerFailedSemaphore, QDeadlineTimer(1)));
                        REQUIRE(!tlsSocketFailedSemaphore.tryAcquire());
                    }
                }
            }
        }

        WHEN("both peers abort the connection")
        {
            const bool isTlsSocketTheClient = GENERATE(AS(bool), true, false);
            auto &pTlsSocket = isTlsSocketTheClient ? pClientPeer : pServerPeer;
            auto &pConnectedPeer = isTlsSocketTheClient ? pServerPeer : pClientPeer;
            pTlsSocket->abort();
            pConnectedPeer->abort();

            THEN("both peers abort the connection without errors")
            {
                REQUIRE(pTlsSocket->errorMessage().empty());
                REQUIRE(pConnectedPeer->errorMessage().empty());

                AND_WHEN("one of the peers is deleted")
                {
                    auto &tlsSocketFailedSemaphore = isTlsSocketTheClient ? clientPeerFailedSemaphore : serverPeerFailedSemaphore;
                    auto &connectedPeerFailedSemaphore = isTlsSocketTheClient ? serverPeerFailedSemaphore : clientPeerFailedSemaphore;
                    pTlsSocket.reset(nullptr);

                    THEN("neither peer emit any error")
                    {
                        REQUIRE(!TRY_ACQUIRE(connectedPeerFailedSemaphore, QDeadlineTimer(1)));
                        REQUIRE(!tlsSocketFailedSemaphore.tryAcquire());
                    }
                }
            }
        }

        WHEN("one of the peers is deleted")
        {
            const bool isTlsSocketTheClient = GENERATE(AS(bool), true, false);
            auto &pTlsSocket = isTlsSocketTheClient ? pClientPeer : pServerPeer;
            auto &pConnectedPeer = isTlsSocketTheClient ? pServerPeer : pClientPeer;
            pTlsSocket.reset(nullptr);

            THEN("connected peer disconnects without errors")
            {
                auto &connectedPeerDisconnectedSemaphore = isTlsSocketTheClient ? serverPeerDisconnectedSemaphore : clientPeerDisconnectedSemaphore;
                REQUIRE(TRY_ACQUIRE(connectedPeerDisconnectedSemaphore, 10));
                auto &tlsSocketFailedSemaphore = isTlsSocketTheClient ? clientPeerFailedSemaphore : serverPeerFailedSemaphore;
                auto &connectedPeerFailedSemaphore = isTlsSocketTheClient ? serverPeerFailedSemaphore : clientPeerFailedSemaphore;
                REQUIRE(!TRY_ACQUIRE(connectedPeerFailedSemaphore, QDeadlineTimer(1)));
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
                Object::connect(serverPeer.get(), &TlsSocket::connected, [](){Spectator::FAIL("This code is supposed to be unreachable.");});
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

        WHEN("client peer connects to server")
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

            THEN("peers connect and perform TLS handshake")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(clientPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerCompletedHandshakeSemaphore, 10));

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


SCENARIO("TlsSocket fails as expected")
{
    GIVEN("no server running on any IP related to host name with IPV4/IPV6 addresses")
    {
        WHEN("client peer is connected to host name with IPV4/IPV6 addresses")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration clientPeerTlsConfiguration;
            clientPeerTlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile);
            clientPeerTlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket clientPeer(clientPeerTlsConfiguration);
            QSemaphore clientPeerFailedSemaphore;
            Object::connect(&clientPeer, &TlsSocket::error, [&](){clientPeerFailedSemaphore.release();});
            auto hostName = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses().first;
            clientPeer.connect(hostName, 5000);

            THEN("connection fails")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerFailedSemaphore, 10));
                REQUIRE(clientPeer.state() == TcpSocket::State::Unconnected);
                REQUIRE(clientPeer.errorMessage().starts_with(std::string("Failed to connect to ").append(hostName).append(" at")));
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

        WHEN("client peer with valid TLS configuration and bounded to an IPV4 address is connected to the IPV6 server")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration clientPeerTlsConfiguration;
            clientPeerTlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket clientPeer(clientPeerTlsConfiguration);
            QSemaphore clientPeerFailedSemaphore;
            Object::connect(&clientPeer, &TlsSocket::error, [&](){clientPeerFailedSemaphore.release();});
            clientPeer.setBindAddressAndPort("127.2.2.5");
            const auto connectByName = GENERATE(AS(bool), true, false);
            if (connectByName)
                clientPeer.connect(hostName, server.serverPort());
            else
                clientPeer.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerFailedSemaphore, 10));
                REQUIRE(clientPeer.state() == TcpSocket::State::Unconnected);
                const auto expectedErrorMessage = connectByName ? std::string("Failed to connect to ").append(hostName).append(" at 127.10.20.90:") : std::string("Failed to connect to [::1]:");
                REQUIRE(clientPeer.errorMessage().starts_with(expectedErrorMessage));
                REQUIRE(connectionCount == 0);
            }
        }

        WHEN("client peer with valid TLS configuration and bounded to a privileged port on IPV6 is connected to the server")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration clientPeerTlsConfiguration;
            clientPeerTlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket clientPeer(clientPeerTlsConfiguration);
            QSemaphore clientPeerFailedSemaphore;
            Object::connect(&clientPeer, &TlsSocket::error, [&](){clientPeerFailedSemaphore.release();});
            clientPeer.setBindAddressAndPort("::1", 443);
            const auto connectByName = GENERATE(AS(bool), true, false);
            if (connectByName)
                clientPeer.connect(hostName, server.serverPort());
            else
                clientPeer.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerFailedSemaphore, 10));
                REQUIRE(clientPeer.state() == TcpSocket::State::Unconnected);
                REQUIRE(clientPeer.errorMessage() == "Failed to bind socket to [::1]:443. POSIX error EACCES(13): Permission denied.");
                REQUIRE(connectionCount == 0);
            }
        }

        WHEN("we try to connect to server a client peer that is configured to bound to an already used address/port pair prior to connecting")
        {
            TcpSocket previouslyConnectedSocket;
            QSemaphore previouslyConnectedSocketSemaphore;
            Object::connect(&previouslyConnectedSocket, &TcpSocket::connected, [&](){previouslyConnectedSocketSemaphore.release();});
            previouslyConnectedSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());
            REQUIRE(TRY_ACQUIRE(previouslyConnectedSocketSemaphore, 10));
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration clientPeerTlsConfiguration;
            clientPeerTlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket clientPeer(clientPeerTlsConfiguration);
            QSemaphore clientPeerFailedSemaphore;
            Object::connect(&clientPeer, &TlsSocket::error, [&](){clientPeerFailedSemaphore.release();});
            clientPeer.setBindAddressAndPort(previouslyConnectedSocket.localAddress(), previouslyConnectedSocket.localPort());
            const auto connectByName = GENERATE(AS(bool), true, false);
            if (connectByName)
                clientPeer.connect(hostName, server.serverPort());
            else
                clientPeer.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerFailedSemaphore, 10));
                REQUIRE(clientPeer.state() == TcpSocket::State::Unconnected);
                REQUIRE(clientPeer.errorMessage() == std::string("Failed to bind socket to [::1]:").append(std::to_string(previouslyConnectedSocket.localPort())).append(". POSIX error EADDRINUSE(98): Address already in use."));
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

        WHEN("client peer with valid TLS configuration and bounded to a IPV6 address is connected to the IPV4 server")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration clientPeerTlsConfiguration;
            clientPeerTlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket clientPeer(clientPeerTlsConfiguration);
            QSemaphore clientPeerFailedSemaphore;
            Object::connect(&clientPeer, &TlsSocket::error, [&](){clientPeerFailedSemaphore.release();});
            clientPeer.setBindAddressAndPort("::1");
            const auto connectByName = GENERATE(AS(bool), true, false);
            if (connectByName)
                clientPeer.connect(hostName, server.serverPort());
            else
                clientPeer.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerFailedSemaphore, 10));
                REQUIRE(clientPeer.state() == TcpSocket::State::Unconnected);
                const auto expectedErrorMessage = connectByName ? std::string("Failed to connect to ").append(hostName).append(" at ").append(serverAddress.toString().toStdString()).append(":") : std::string("Failed to connect to ").append(serverAddress.toString().toStdString()).append(":");
                REQUIRE(clientPeer.errorMessage().starts_with(expectedErrorMessage));
                REQUIRE(connectionCount == 0);
            }
        }

        WHEN("we try to connect to server a client peer with a valid TLS configuration and configured to bound to a privileged port on IPV4 prior to connecting")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration clientPeerTlsConfiguration;
            clientPeerTlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket clientPeer(clientPeerTlsConfiguration);
            QSemaphore clientPeerFailedSemaphore;
            Object::connect(&clientPeer, &TlsSocket::error, [&](){clientPeerFailedSemaphore.release();});
            clientPeer.setBindAddressAndPort("127.0.0.1", 443);
            const auto connectByName = GENERATE(AS(bool), true, false);
            if (connectByName)
                clientPeer.connect(hostName, server.serverPort());
            else
                clientPeer.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerFailedSemaphore, 10));
                REQUIRE(clientPeer.state() == TcpSocket::State::Unconnected);
                REQUIRE(clientPeer.errorMessage() == "Failed to bind socket to 127.0.0.1:443. POSIX error EACCES(13): Permission denied.");
                REQUIRE(connectionCount == 0);
            }
        }

        WHEN("we try to connect to server a client peer configured to bound to an already used address/port pair prior to connecting")
        {
            TcpSocket previouslyConnectedSocket;
            QSemaphore previouslyConnectedSocketSemaphore;
            Object::connect(&previouslyConnectedSocket, &TcpSocket::connected, [&](){previouslyConnectedSocketSemaphore.release();});
            previouslyConnectedSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());
            REQUIRE(TRY_ACQUIRE(previouslyConnectedSocketSemaphore, 10));
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration clientPeerTlsConfiguration;
            clientPeerTlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket clientPeer(clientPeerTlsConfiguration);
            QSemaphore clientPeerFailedSemaphore;
            Object::connect(&clientPeer, &TlsSocket::error, [&](){clientPeerFailedSemaphore.release();});
            clientPeer.setBindAddressAndPort(previouslyConnectedSocket.localAddress(), previouslyConnectedSocket.localPort());
            const auto connectByName = GENERATE(AS(bool), true, false);
            if (connectByName)
                clientPeer.connect(hostName, server.serverPort());
            else
                clientPeer.connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("connection fails")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerFailedSemaphore, 10));
                REQUIRE(clientPeer.state() == TcpSocket::State::Unconnected);
                REQUIRE(clientPeer.errorMessage() == std::string("Failed to bind socket to 127.0.0.1:").append(std::to_string(previouslyConnectedSocket.localPort())).append(". POSIX error EADDRINUSE(98): Address already in use."));
                REQUIRE(connectionCount == 1);
            }
        }
    }

    GIVEN("a descriptor that does not represent a socket")
    {
        const auto fileDescriptor = ::memfd_create("Kourier_tls_socket_spec_a_descriptor_that_does_not_represent_a_socket", 0);
        REQUIRE(fileDescriptor >= 0);

        WHEN("we try to create a TlsSocket with a valid TLS configuration and the given descriptor")
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

        WHEN("we try to create a TlsSocket with a valid TLS configuration and the given descritor")
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

        WHEN("we try to create a TlsSocket with a valid TLS configuration and the given descritor")
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

    GIVEN("a server with a saturated backlog that does not accept new connections")
    {
        QTcpServer server;
        static constexpr int backlogSize = 128;
        server.setListenBacklogSize(backlogSize);
        QObject::connect(&server, &QTcpServer::newConnection, [](){Spectator::FAIL("This code is supposed to be unreachable.");});
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
            isServerAcceptingConnections = TRY_ACQUIRE(connectedSemaphore, QDeadlineTimer(10));
        }
        while (isServerAcceptingConnections);
        sockets.front()->abort();

        WHEN("we try to connect to server a client peer with valid TLS configuration")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration clientTlsConfiguration;
            clientTlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket clientPeer(clientTlsConfiguration);
            clientPeer.setConnectTimeout(std::chrono::milliseconds(5));
            QSemaphore clientPeerFailedSemaphore;
            Object::connect(&clientPeer, &TlsSocket::error, [&]() {clientPeerFailedSemaphore.release();});
            clientPeer.connect(hostName, server.serverPort());

            THEN("client peer times out while trying to connect to server")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerFailedSemaphore, 70));
                REQUIRE(clientPeer.state() == TcpSocket::State::Unconnected);
                const auto expectedErrorMessage = std::string("Failed to connect to ").append(hostName).append(" at ").append(serverAddress.toString().toStdString()).append(":").append(std::to_string(server.serverPort())).append(".");
                REQUIRE(clientPeer.errorMessage() == expectedErrorMessage);
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

        WHEN("we try to connect to server a client peer with a valid TLS configuration")
        {
            const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
            std::string certificateFile;
            std::string privateKeyFile;
            std::string caCertificateFile;
            TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
            TlsConfiguration clientTlsConfiguration;
            clientTlsConfiguration.addCaCertificate(caCertificateFile);
            TlsSocket clientPeer(clientTlsConfiguration);
            clientPeer.setTlsHandshakeTimeout(std::chrono::milliseconds(5));
            QSemaphore clientPeerFailedSemaphore;
            Object::connect(&clientPeer, &TlsSocket::error, [&]() {clientPeerFailedSemaphore.release();});
            clientPeer.connect(hostName, server.serverPort());

            THEN("client peer TLS handshake times out")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerFailedSemaphore, 70));
                REQUIRE(clientPeer.state() == TcpSocket::State::Unconnected);
                const auto expectedErrorMessage = std::string("Failed to connect to ")
                                                  .append(hostName)
                                                  .append(" at ")
                                                  .append(serverAddress.toString().toStdString()).append(":")
                                                  .append(std::to_string(server.serverPort()))
                                                  .append(". TLS handshake timed out.");
                REQUIRE(clientPeer.errorMessage() == expectedErrorMessage);
            }
        }
    }

    GIVEN("a client peer that does not do TLS handshakes")
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
        QSemaphore serverPeerConnectedSemaphore;
        QSemaphore serverPeerFailedSemaphore;
        std::unique_ptr<TlsSocket> pServerPeer;
        Object::connect(&server, &TlsServer::newConnection, [&](TlsSocket *pNewSocket)
        {
            REQUIRE(!pServerPeer);
            pServerPeer.reset(pNewSocket);
            pServerPeer->setTlsHandshakeTimeout(std::chrono::milliseconds(5));
            Object::connect(pServerPeer.get(), &TlsSocket::error, [&]() {serverPeerFailedSemaphore.release();});
            serverPeerConnectedSemaphore.release();
        });
        REQUIRE(server.listen(serverAddress));
        TcpSocket clientPeer;
        QSemaphore clientPeerConnectedSemaphore;
        QSemaphore clientPeerDisconnectedSemaphore;
        std::string localAddress;
        uint16_t localPort = 0;
        Object::connect(&clientPeer, &TcpSocket::connected, [&]()
            {
                localAddress = clientPeer.localAddress();
                localPort = clientPeer.localPort();
                clientPeerConnectedSemaphore.release();
            });
        Object::connect(&clientPeer, &TcpSocket::disconnected, [&]() {clientPeerDisconnectedSemaphore.release();});

        WHEN("client peer connects to server")
        {
            clientPeer.connect(serverAddress.toString().toStdString(), server.serverPort());

            THEN("peers establish a TCP connection")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 1));
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 1));

                AND_THEN("server peer handshake times out, aborts the connection and disconnects the client peer")
                {
                    REQUIRE(TRY_ACQUIRE(serverPeerFailedSemaphore, 70));
                    REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 1));
                    REQUIRE(clientPeer.errorMessage().empty());
                    REQUIRE(pServerPeer->state() == TcpSocket::State::Unconnected);
                    std::string expectedErrorMessage("Failed to connect to ");
                    expectedErrorMessage.append(localAddress);
                    expectedErrorMessage.append(":");
                    expectedErrorMessage.append(std::to_string(localPort));
                    expectedErrorMessage.append(". TLS handshake timed out.");
                    REQUIRE(pServerPeer->errorMessage() == expectedErrorMessage);
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
        Object::connect(&clientPeer, &TlsSocket::encrypted, [](){Spectator::FAIL("This code is supposed to be unreachable.");});
        Object::connect(&clientPeer, &TlsSocket::error, [&]() {clientPeerFailedSemaphore.release();});
        Object::connect(&clientPeer, &TlsSocket::disconnected, []() {Spectator::FAIL("This code is supposed to be unreachable.");});
        Object::connect(&clientPeer, &TlsSocket::receivedData, []() {Spectator::FAIL("This code is supposed to be unreachable.");});
        const auto serverCertificateType = TlsTestCertificates::CertificateType::ECDSA;
        std::string serverCertificateFile;
        std::string serverPrivateKeyFile;
        std::string serverCaCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(serverCertificateType, serverCertificateFile, serverPrivateKeyFile, serverCaCertificateFile);
        TlsConfiguration serverTlsConfiguration;
        serverTlsConfiguration.setCertificateKeyPair(serverCertificateFile, serverPrivateKeyFile);
        serverTlsConfiguration.addCaCertificate(serverCaCertificateFile);
        TlsServer server(serverTlsConfiguration);
        QSemaphore serverPeerConnectedSemaphore;
        QSemaphore serverPeerDisconnectedSemaphore;
        std::unique_ptr<TlsSocket> pServerPeer;
        Object::connect(&server, &TlsServer::newConnection, [&](TlsSocket *pNewSocket)
        {
            REQUIRE(!pServerPeer);
            pServerPeer.reset(pNewSocket);
            Object::connect(pServerPeer.get(), &TlsSocket::connected, []() {Spectator::FAIL("This code is supposed to be unreachable.");});
            Object::connect(pServerPeer.get(), &TlsSocket::encrypted, []() {Spectator::FAIL("This code is supposed to be unreachable.");});
            Object::connect(pServerPeer.get(), &TlsSocket::error, []() {Spectator::FAIL("This code is supposed to be unreachable.");});
            Object::connect(pServerPeer.get(), &TlsSocket::disconnected, [&]() {serverPeerDisconnectedSemaphore.release();});
            Object::connect(pServerPeer.get(), &TlsSocket::receivedData, []() {Spectator::FAIL("This code is supposed to be unreachable.");});
            serverPeerConnectedSemaphore.release();
        });
        auto [hostName, hostAddresses] = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses();
        const QHostAddress serverAddress("127.10.20.90");
        REQUIRE(hostAddresses.contains(serverAddress));
        REQUIRE(server.listen(serverAddress));

        WHEN("client peer tries to connect to server")
        {
            clientPeer.connect(hostName, server.serverPort());

            THEN("peers establish a TCP connection; server peer sends to client an untrusted certificate; client aborts the connection and disconnects the server peer")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(clientPeerFailedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
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
            pClientPeer->setBindAddressAndPort(serverAddress.toString().toStdString());
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

            THEN("peers establish TCP connection; client peer verifies server and completes its handshake after sending an untrusted certificate to server peer; server peer fails to complete TLS handshake and aborts the connection disconnecting the client peer")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(clientPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerFailedSemaphore, 10));
                REQUIRE(!pServerPeer->errorMessage().empty());
                REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                REQUIRE(pClientPeer->errorMessage().empty());
            }
        }
    }

    GIVEN("a client peer that does not send any certificate to a server that requires client authentication")
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

        WHEN("client peer connects to host")
        {
            pClientPeer->connect(hostName, server.serverPort());

            THEN("peers establish TCP connection; client peer completes it's TLS handshake; server peer's TLS handshake times out; server peer aborts the connection disconnecting the client peer")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(clientPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerFailedSemaphore, 10));
                REQUIRE(!pServerPeer->errorMessage().empty());
                REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
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
        clientTlsConfiguration.setTlsVersion(TlsConfiguration::TlsVersion::TLS_1_3);
        clientTlsConfiguration.addCaCertificate(caCertificateFile);
        TlsConfiguration serverTlsConfiguration;
        serverTlsConfiguration.setTlsVersion(TlsConfiguration::TlsVersion::TLS_1_3);
        serverTlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile, privateKeyPassword);
        serverTlsConfiguration.addCaCertificate(caCertificateFile);
        TlsServer server(serverTlsConfiguration);
        QSemaphore serverPeerConnectedSemaphore;
        QSemaphore serverPeerCompletedHandshakeSemaphore;
        QSemaphore serverPeerFailedSemaphore;
        QSemaphore serverPeerDisconnectedSemaphore;
        QSemaphore serverPeerReceivedDataSemaphore;
        QByteArray serverPeerReceivedData;
        std::unique_ptr<TlsSocket> pServerPeer;
        Object::connect(&server, &TlsServer::newConnection, [&](TlsSocket *pNewSocket)
        {
            pServerPeer.reset(pNewSocket);
            Object::connect(pServerPeer.get(), &TlsSocket::connected, [](){Spectator::FAIL("This code is supposed to be unreachable.");});
            Object::connect(pServerPeer.get(), &TlsSocket::encrypted, [&](){serverPeerCompletedHandshakeSemaphore.release();});
            Object::connect(pServerPeer.get(), &TlsSocket::error, [&]() {serverPeerFailedSemaphore.release();});
            Object::connect(pServerPeer.get(), &TlsSocket::disconnected, [&serverPeerDisconnectedSemaphore]() {serverPeerDisconnectedSemaphore.release();});
            Object::connect(pServerPeer.get(), &TlsSocket::receivedData, [&]()
            {
                serverPeerReceivedData.append(pServerPeer->readAll());
                serverPeerReceivedDataSemaphore.release();
            });
            serverPeerConnectedSemaphore.release();
        });
        auto [hostName, hostAddresses] = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses();
        const QHostAddress serverAddress("127.10.20.90");
        REQUIRE(hostAddresses.contains(serverAddress));
        REQUIRE(server.listen(serverAddress));
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
        enum class TlsRecordElement {Type, Version, Length};
        const auto invalidTlsRecordElement = GENERATE(AS(TlsRecordElement),
                                                        TlsRecordElement::Type,
                                                        TlsRecordElement::Version,
                                                        TlsRecordElement::Length);
        const QByteArray invalidTlsRecord = [](TlsRecordElement tlsRecordElement)
        {
            switch(tlsRecordElement)
            {
                case TlsRecordElement::Type:
                    return QByteArray::fromHex("aa03040500");
                case TlsRecordElement::Version:
                    return QByteArray::fromHex("1703ff0500");
                case TlsRecordElement::Length:
                    return QByteArray::fromHex("170304ffff");
                }
        }(invalidTlsRecordElement);

        WHEN("peer connects to host")
        {
            pClientPeer->connect(hostName, server.serverPort());

            THEN("both peers connect and complete TLS handshake successfully")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(clientPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerCompletedHandshakeSemaphore, 10));
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
                            REQUIRE(TRY_ACQUIRE(serverPeerReceivedDataSemaphore, 10));
                        } while (serverPeerReceivedData != dataToSend);
                    }
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

                    THEN("server peer fails and aborts the connection, disconnecting the client peer")
                    {
                        REQUIRE(TRY_ACQUIRE(serverPeerFailedSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                        REQUIRE(pServerPeer->errorMessage().starts_with("Failed to decrypt data."));
                    }
                }
            }
        }
    }
}


SCENARIO("TlsSocket allows connected slots to take any action")
{
    GIVEN("a client peer and a running server")
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
        TlsConfiguration clientTlsConfiguration;
        clientTlsConfiguration.addCaCertificate(caCertificateFile);
        QSemaphore serverPeerConnectedSemaphore;
        QSemaphore serverPeerCompletedHandshakeSemaphore;
        QSemaphore serverPeerFailedSemaphore;
        QSemaphore serverPeerDisconnectedSemaphore;
        QSemaphore serverPeerReceivedDataSemaphore;
        QByteArray serverPeerReceivedData;
        std::unique_ptr<TlsSocket> pServerPeer;
        Object::connect(&server, &TlsServer::newConnection, [&](TlsSocket *pSocket)
        {
            REQUIRE(!pServerPeer);
            pServerPeer.reset(pSocket);
            REQUIRE(!pServerPeer->isEncrypted());
            Object::connect(pServerPeer.get(), &TlsSocket::error, [&]() {serverPeerFailedSemaphore.release();});
            Object::connect(pServerPeer.get(), &TlsSocket::encrypted, [&](){serverPeerCompletedHandshakeSemaphore.release();});
            Object::connect(pServerPeer.get(), &TlsSocket::disconnected, [&](){serverPeerDisconnectedSemaphore.release();});
            Object::connect(pServerPeer.get(), &TlsSocket::receivedData, [&]()
            {
                if (pServerPeer)
                    serverPeerReceivedData.append(pServerPeer->readAll());
                serverPeerReceivedDataSemaphore.release();
            });
            serverPeerConnectedSemaphore.release(2);
        });
        // This test creates a lot of sockets in TIME_WAIT state if run
        // repeatedly (by using the -r command line option), as this test 
        // intentionally interrupts the connection abruptly.
        // To prevent interfering with other tests, we use exclusive addresses for this test.
        const auto serverAddresses = QList<QHostAddress>() << QHostAddress("127.127.127.10")
                                                           << QHostAddress("127.127.127.20")
                                                           << QHostAddress("127.127.127.30")
                                                           << QHostAddress("127.127.127.40")
                                                           << QHostAddress("127.127.127.50")
                                                           << QHostAddress("127.127.127.60")
                                                           << QHostAddress("127.127.127.70")
                                                           << QHostAddress("127.127.127.80")
                                                           << QHostAddress("127.127.127.90")
                                                           << QHostAddress("127.127.127.100");
        const auto serverAddress = serverAddresses[QRandomGenerator64::global()->bounded(serverAddresses.size())];
        REQUIRE(server.listen(serverAddress));
        QSemaphore clientPeerConnectedSemaphore;
        QSemaphore clientPeerCompletedHandshakeSemaphore;
        QSemaphore clientPeerFailedSemaphore;
        QSemaphore clientPeerDisconnectedSemaphore;
        std::unique_ptr<TlsSocket> pClientPeer(new TlsSocket(clientTlsConfiguration));
        Object::connect(pClientPeer.get(), &TlsSocket::connected, [&]()
            {
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                clientPeerConnectedSemaphore.release();
            });
        Object::connect(pClientPeer.get(), &TlsSocket::error, [&]() {clientPeerFailedSemaphore.release();});
        Object::connect(pClientPeer.get(), &TlsSocket::encrypted, [&](){clientPeerCompletedHandshakeSemaphore.release();});
        Object::connect(pClientPeer.get(), &TlsSocket::disconnected, [&](){clientPeerDisconnectedSemaphore.release();});

        WHEN("client peer connects to server and disconnects while handling the connected signal")
        {
            Object::connect(pClientPeer.get(), &TlsSocket::connected, [&]() {pClientPeer->disconnectFromPeer();});
            pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("peers connect and client peer disconnects from server peer")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                REQUIRE(!clientPeerDisconnectedSemaphore.tryAcquire());
            }
        }

        WHEN("client peer connects to server and aborts connection while handling the connected signal")
        {
            Object::connect(pClientPeer.get(), &TlsSocket::connected, [&]() {pClientPeer->abort();});
            pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("peers connect, client peer aborts connection and disconnects server peer")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                REQUIRE(!clientPeerDisconnectedSemaphore.tryAcquire());
            }
        }

        WHEN("client peer connects to server and is destroyed while handling the connected signal")
        {
            Object::connect(pClientPeer.get(), &TlsSocket::connected, [&]() {pClientPeer.release()->scheduleForDeletion();});
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
            Object::connect(pClientPeer.get(), &TlsSocket::connected, pCtxObject, [&, ptrCtxObject = pCtxObject]()
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
                REQUIRE(TRY_ACQUIRE(clientPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerCompletedHandshakeSemaphore, 10));
                pClientPeer->disconnectFromPeer();
                REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
            }
        }

        WHEN("client peer connects to server and connects to a non-existent server address while handling the connected signal")
        {
            Object::connect(pClientPeer.get(), &TlsSocket::connected, [&]()
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

        WHEN("TcpSocket connects to server and disconnects while handling the encrypted signal")
        {
            Object::connect(pClientPeer.get(), &TlsSocket::encrypted, [&]() {pClientPeer->disconnectFromPeer();});
            pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("client peer connects, completes TLS handshake and then disconnects from peer")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(clientPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
            }
        }

        WHEN("client peer connects to server and aborts connection while handling the encrypted signal")
        {
            Object::connect(pClientPeer.get(), &TlsSocket::encrypted, [&]() {pClientPeer->abort();});
            pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("client peer connects, completes TLS handshake and then aborts connection disconnecting the server peer")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(clientPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
            }
        }

        WHEN("client peer connects to server and is destroyed while handling the encrypted signal")
        {
            Object::connect(pClientPeer.get(), &TlsSocket::encrypted, [&]() {pClientPeer.release()->scheduleForDeletion();});
            pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("peers connect and complete handshake then client peer aborts connection disconnecting server peer")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(clientPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
            }
        }

        WHEN("TcpSocket connects to server and connects again while handling the encrypted signal")
        {
            Object *pCtxObject = new Object;
            Object::connect(pClientPeer.get(), &TlsSocket::encrypted, pCtxObject, [&, ptrCtxObject = pCtxObject]()
                {
                    while (!pServerPeer) {QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents, 1);}
                    Object::connect(pServerPeer.get(), &TcpSocket::disconnected, [pPeer = pServerPeer.get()](){pPeer->scheduleForDeletion();});
                    pServerPeer.release();
                    pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());
                    delete ptrCtxObject;
                });
            pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());

            THEN("peers connect and complete TLS handshake, client peer aborst connection disconnecting server peer and then client peer reconnects to another server peer")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(clientPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(clientPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerCompletedHandshakeSemaphore, 10));
                pClientPeer->disconnectFromPeer();
                REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
            }
        }

        WHEN("client peer connects to server and connects to a non-existent server address while handling the encrypted signal")
        {
            Object::connect(pClientPeer.get(), &TlsSocket::encrypted, [&]()
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

            THEN("peers connect and complete TLS handshake, client peer aborts and server peer disconnects and then client peer fails to connect to the non-existent server")
            {
                REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(clientPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(clientPeerFailedSemaphore, 10));
                REQUIRE(pClientPeer->errorMessage().starts_with("Failed to connect to 127.1.2.3:"));
            }
        }

        WHEN("client peer connects to server and completes TLS handshake")
        {
            pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());
            REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
            REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
            REQUIRE(TRY_ACQUIRE(clientPeerCompletedHandshakeSemaphore, 10));
            REQUIRE(TRY_ACQUIRE(serverPeerCompletedHandshakeSemaphore, 10));

            THEN("connected peers can start exchanging data")
            {
                AND_WHEN("server peer sends some data to client peer which disconnects while handling the receivedData signal")
                {
                    Object::connect(pClientPeer.get(), &TlsSocket::receivedData, [&](){pClientPeer->disconnectFromPeer();});
                    pServerPeer->write("abcdefgh");

                    THEN("peers disconnect")
                    {
                        REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                    }
                }


                AND_WHEN("server peer sends some data to client peer which aborts the connection while handling the receivedData signal")
                {
                    Object::connect(pClientPeer.get(), &TlsSocket::receivedData, [&](){pClientPeer->abort();});
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
                    Object::connect(pReleasedSocket, &TlsSocket::receivedData, [=](){pReleasedSocket->scheduleForDeletion();});
                    pServerPeer->write("abcdefgh");

                    THEN("client peer is destroyed and server peer disconnects")
                    {
                        REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                    }
                }

                AND_WHEN("server peer sends some data to client peer which reconnects while handling the receivedData signal")
                {
                    Object::connect(pServerPeer.get(), &TlsSocket::disconnected, [pPeer = pServerPeer.get()](){pPeer->scheduleForDeletion();});
                    Object::connect(pClientPeer.get(), &TlsSocket::receivedData, [&]()
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
                        REQUIRE(TRY_ACQUIRE(clientPeerCompletedHandshakeSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(serverPeerCompletedHandshakeSemaphore, 10));
                        pClientPeer->disconnectFromPeer();
                        REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                    }
                }

                AND_WHEN("client peer sends more data the underlying socket's send buffer can store and disconnects while handling the sentData signal with data still to be written")
                {
                    auto *pObject = new Object;
                    Object::connect(pClientPeer.get(), &TlsSocket::sentData, pObject, [&, pCtxObject = pObject]()
                    {
                        if (pClientPeer->dataToWrite())
                        {
                            pClientPeer->disconnectFromPeer();
                            delete pCtxObject;
                        }
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
                    auto *pObject = new Object;
                    Object::connect(pClientPeer.get(), &TlsSocket::sentData, pObject, [&, pCtxObject = pObject]()
                    {
                        if (!pClientPeer->dataToWrite())
                        {
                            pClientPeer->disconnectFromPeer();
                            delete pCtxObject;
                        }
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
                    auto *pObject = new Object;
                    Object::connect(pClientPeer.get(), &TlsSocket::sentData, pObject, [&, pCtxObject = pObject]()
                    {
                        if (pClientPeer->dataToWrite())
                        {
                            pClientPeer->abort();
                            delete pCtxObject;
                        }
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
                    auto *pObject = new Object;
                    Object::connect(pClientPeer.get(), &TlsSocket::sentData, pObject, [&, pCtxObject = pObject]()
                    {
                        if (!pClientPeer->dataToWrite())
                        {
                            pClientPeer->abort();
                            delete pCtxObject;
                        }
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
                    auto *pObject = new Object;
                    Object::connect(pClientPeer.get(), &TlsSocket::sentData, [&, pCtxObject = pObject, ptrSocket = pClientPeer.get()]()
                    {
                        if (ptrSocket->dataToWrite())
                        {
                            ptrSocket->scheduleForDeletion();
                            pClientPeer.release();
                            delete pCtxObject;
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
                    auto *pObject = new Object;
                    Object::connect(pClientPeer.get(), &TlsSocket::sentData, pObject, [&, pCtxObject = pObject]()
                    {
                        if (!pClientPeer->dataToWrite())
                        {
                            pClientPeer.release()->scheduleForDeletion();
                            delete pCtxObject;
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

                AND_WHEN("client peer sends more data the underlying socket's send buffer can store and is reconnected while handling the sentData signal with data still to be written")
                {
                    auto *pPeer = pServerPeer.release();
                    Object::connect(pPeer, &TlsSocket::disconnected, [=](){pPeer->scheduleForDeletion();});
                    auto *pObject = new Object;
                    Object::connect(pClientPeer.get(), &TlsSocket::sentData, pObject, [&, pCtxObject = pObject]()
                    {
                        if (pClientPeer->dataToWrite())
                        {
                            pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());
                            delete pCtxObject;
                        }
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
                        REQUIRE(TRY_ACQUIRE(clientPeerCompletedHandshakeSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(serverPeerCompletedHandshakeSemaphore, 10));
                        pClientPeer->disconnectFromPeer();
                        REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                    }
                }

                AND_WHEN("client peer sends more data the underlying socket's send buffer can store and is reconnected while handling the sentData signal with no more data still to be written")
                {
                    auto *pPeer = pServerPeer.release();
                    Object::connect(pPeer, &TlsSocket::disconnected, [=](){pPeer->scheduleForDeletion();});
                    auto *pObject = new Object;
                    Object::connect(pClientPeer.get(), &TlsSocket::sentData, pObject, [&, pCtxObject = pObject]()
                    {
                        if (!pClientPeer->dataToWrite())
                        {
                            pClientPeer->connect(server.serverAddress().toString().toStdString(), server.serverPort());
                            delete pCtxObject;
                        }
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
                        REQUIRE(TRY_ACQUIRE(clientPeerCompletedHandshakeSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(serverPeerCompletedHandshakeSemaphore, 10));
                        pClientPeer->disconnectFromPeer();
                        REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                        REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                    }
                }
            }

            AND_WHEN("server peer disconnects and client peer is disconnected while handling the disconnected signal")
            {
                QSemaphore socketDisconnectedFromPeerSemaphore;
                Object::connect(pClientPeer.get(), &TlsSocket::disconnected, [&]()
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
                Object::connect(pClientPeer.get(), &TlsSocket::disconnected, [&]()
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
                Object::connect(pClientPeer.get(), &TlsSocket::disconnected, [&]()
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
                Object::connect(pClientPeer.get(), &TlsSocket::disconnected, pCtxObject, [&, ptrCtxObject = pCtxObject]()
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
                    REQUIRE(TRY_ACQUIRE(clientPeerCompletedHandshakeSemaphore, 10));
                    REQUIRE(TRY_ACQUIRE(serverPeerCompletedHandshakeSemaphore, 10));
                    pClientPeer->disconnectFromPeer();
                    REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                    REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                }
            }
        }

        WHEN("client peer tries to connect to a non-existent server by address and is disconnected while handling the error signal")
        {
            QSemaphore socketHandledErrorSemaphore;
            Object::connect(pClientPeer.get(), &TlsSocket::error, [&]()
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
            Object::connect(pClientPeer.get(), &TlsSocket::error, [&]()
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
            Object::connect(pClientPeer.get(), &TlsSocket::error, [&]()
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
            Object::connect(pClientPeer.get(), &TlsSocket::error, [&]()
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
                REQUIRE(TRY_ACQUIRE(clientPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(pClientPeer->errorMessage().empty());
                pClientPeer->disconnectFromPeer();
                REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
            }
        }

        WHEN("client peer tries to connect to host name with IPV4/IPV6 addresses without any server running and disconnects while handling the error signal")
        {
            QSemaphore socketHandledErrorSemaphore;
            Object::connect(pClientPeer.get(), &TlsSocket::error, [&]()
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
            Object::connect(pClientPeer.get(), &TlsSocket::error, [&]()
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
            Object::connect(pClientPeer.get(), &TlsSocket::error, [&]()
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
            Object::connect(pClientPeer.get(), &TlsSocket::error, [&]()
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
                REQUIRE(TRY_ACQUIRE(clientPeerCompletedHandshakeSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerCompletedHandshakeSemaphore, 10));
                pClientPeer->disconnectFromPeer();
                REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
            }
        }
    }
}


SCENARIO("TlsSockets can be reused")
{
    GIVEN("A running server whose sockets close the connections just after the TLS handshake completes")
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
        const auto certificateType = TlsTestCertificates::CertificateType::ECDSA;
        std::string certificateFile;
        std::string privateKeyFile;
        std::string caCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(certificateType, certificateFile, privateKeyFile, caCertificateFile);
        TlsConfiguration serverTlsConfiguration;
        serverTlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile);
        serverTlsConfiguration.addCaCertificate(caCertificateFile);
        TlsServer server(serverTlsConfiguration);
        TlsConfiguration clientTlsConfiguration;
        clientTlsConfiguration.addCaCertificate(caCertificateFile);
        QSemaphore serverPeerConnectedSemaphore;
        QSemaphore serverPeerCompletedHandshakeSemaphore;
        QSemaphore serverPeerFailedSemaphore;
        QSemaphore serverPeerDisconnectedSemaphore;
        QSemaphore serverPeerReceivedDataSemaphore;
        QByteArray serverPeerReceivedData;
        std::unique_ptr<TlsSocket> pServerPeer;
        Object::connect(&server, &TlsServer::newConnection, [&](TlsSocket *pSocket)
        {
            REQUIRE(!pServerPeer);
            pServerPeer.reset(pSocket);
            REQUIRE(!pServerPeer->isEncrypted());
            Object::connect(pServerPeer.get(), &TlsSocket::error, [&]() {serverPeerFailedSemaphore.release();});
            Object::connect(pServerPeer.get(), &TlsSocket::encrypted, [&](){pServerPeer->disconnectFromPeer(); serverPeerCompletedHandshakeSemaphore.release();});
            Object::connect(pServerPeer.get(), &TlsSocket::disconnected, [&](){pServerPeer.release()->scheduleForDeletion(); serverPeerDisconnectedSemaphore.release();});
            serverPeerConnectedSemaphore.release();
        });
        REQUIRE(server.listen(serverAddress));

        WHEN("client peer connects to server three times")
        {
            TlsSocket clientPeer(clientTlsConfiguration);
            QSemaphore clientPeerConnectedSemaphore;
            QSemaphore clientPeerCompletedHandshakeSemaphore;
            QSemaphore clientPeerDisconnectedSemaphore;
            Object::connect(&clientPeer, &TlsSocket::connected, [&](){clientPeerConnectedSemaphore.release();});
            Object::connect(&clientPeer, &TlsSocket::encrypted, [&](){clientPeerCompletedHandshakeSemaphore.release();});
            Object::connect(&clientPeer, &TlsSocket::disconnected, [&](){clientPeerDisconnectedSemaphore.release();});

            THEN("peers connect and disconnect as many times as client peer connected to server")
            {
                for (auto i = 0; i < 3; ++i)
                {
                    clientPeer.connect(server.serverAddress().toString().toStdString(), server.serverPort());
                    REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, 10));
                    REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, 10));
                    REQUIRE(TRY_ACQUIRE(clientPeerCompletedHandshakeSemaphore, 10));
                    REQUIRE(TRY_ACQUIRE(serverPeerCompletedHandshakeSemaphore, 10));
                    REQUIRE(TRY_ACQUIRE(clientPeerDisconnectedSemaphore, 10));
                    REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, 10));
                }
            }
        }
    }
}
