// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <Spectator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QByteArray>
#include <QSemaphore>
#include <QDeadlineTimer>
#include <memory>


SCENARIO("QTcpSocket sends data to connected peer before disconnecting")
{
    GIVEN("Two connected sockets")
    {
        QTcpServer server;
        // Using a generator for the host address. This path will be run twice.
        const auto hostAddress = GENERATE(AS(QHostAddress), QHostAddress::LocalHost, QHostAddress::LocalHostIPv6);
        REQUIRE(server.listen(hostAddress));
        std::unique_ptr<QTcpSocket> pServerPeer;
        QSemaphore serverPeerConnectedSemaphore;
        QObject::connect(&server, &QTcpServer::newConnection, [&]()
        {
            REQUIRE(pServerPeer.get() == nullptr);
            pServerPeer.reset(server.nextPendingConnection());
            REQUIRE(pServerPeer.get() != nullptr);
            pServerPeer->setParent(nullptr);
            REQUIRE(server.nextPendingConnection() == nullptr);
            serverPeerConnectedSemaphore.release(1);
        });
        QTcpSocket clientPeer;
        QSemaphore clientPeerConnectedSemaphore;
        QObject::connect(&clientPeer, &QTcpSocket::connected, [&]()
        {
            clientPeerConnectedSemaphore.release(1);
        });
        clientPeer.connectToHost(server.serverAddress(), server.serverPort());
        // During TRY_ACQUIRE, events are processed normally.
        REQUIRE(TRY_ACQUIRE(clientPeerConnectedSemaphore, QDeadlineTimer(5000)));
        REQUIRE(TRY_ACQUIRE(serverPeerConnectedSemaphore, QDeadlineTimer(5000)));
        clientPeer.setSocketOption(QAbstractSocket::LowDelayOption, 1);
        pServerPeer->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        QSemaphore serverPeerDisconnectedSemaphore;
        QObject::connect(pServerPeer.get(), &QTcpSocket::disconnected, [&]
        {
            serverPeerDisconnectedSemaphore.release(1);
        });

        WHEN("client peer sends data to server peer before disconnecting")
        {
            // This path will be run four times (two different data transfers for each of the
            // two localhost (IPV4 and IPV6) connections established above).
            const auto sentData = GENERATE(AS(QByteArray), "Hello Peer!", "The test will be run again with this text as sent data.");
            clientPeer.write(sentData);
            clientPeer.disconnectFromHost();

            THEN("server peer receives sent data before disconnecting")
            {
                REQUIRE(TRY_ACQUIRE(serverPeerDisconnectedSemaphore, QDeadlineTimer(5000)));
                REQUIRE(pServerPeer->readAll() == sentData);
            }
        }
    }
}
