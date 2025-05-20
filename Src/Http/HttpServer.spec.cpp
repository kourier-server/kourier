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

#include "HttpServer.h"
#include "ErrorHandler.h"
#include "HttpServerOptions.h"
#include "../Core/TcpSocket.h"
#include "../Core/TlsSocket.h"
#include <Tests/Resources/TlsTestCertificates.h>
#include <Spectator.h>
#include <QProcess>
#include <QTemporaryFile>
#include <QThread>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMutex>
#include <memory>
#include <string>
#include <vector>


using Kourier::HttpServer;
using Kourier::ErrorHandler;
using Kourier::HttpServerOptions;
using Kourier::TcpSocket;
using Kourier::TlsSocket;
using Kourier::Object;
using Kourier::HttpRequest;
using Kourier::HttpBroker;
using Kourier::TlsConfiguration;
using TlsVersion = Kourier::TlsConfiguration::TlsVersion;
using Kourier::TestResources::TlsTestCertificates;
using Spectator::SemaphoreAwaiter;


SCENARIO("HttpServer sets default values for its options")
{
    GIVEN("a http server")
    {
        HttpServer server;

        WHEN("server options are fetched")
        {
            const auto option = GENERATE(AS(HttpServer::ServerOption),
                                         HttpServer::ServerOption::WorkerCount,
                                         HttpServer::ServerOption::TcpServerBacklogSize,
                                         HttpServer::ServerOption::IdleTimeoutInSecs,
                                         HttpServer::ServerOption::RequestTimeoutInSecs,
                                         HttpServer::ServerOption::MaxUrlSize,
                                         HttpServer::ServerOption::MaxHeaderNameSize,
                                         HttpServer::ServerOption::MaxHeaderValueSize,
                                         HttpServer::ServerOption::MaxHeaderLineCount,
                                         HttpServer::ServerOption::MaxTrailerNameSize,
                                         HttpServer::ServerOption::MaxTrailerValueSize,
                                         HttpServer::ServerOption::MaxTrailerLineCount,
                                         HttpServer::ServerOption::MaxChunkMetadataSize,
                                         HttpServer::ServerOption::MaxRequestSize,
                                         HttpServer::ServerOption::MaxBodySize,
                                         HttpServer::ServerOption::MaxConnectionCount);
            const auto optionValue = server.serverOption(option);

            THEN("server sets default values for its options")
            {
                REQUIRE(HttpServerOptions::defaultOptionValue(option) == optionValue);
            }
        }

    }
}


SCENARIO("HttpServer emits started after all workers start")
{
    GIVEN("a http server")
    {
        HttpServer server;
        QSemaphore serverEmittedStartedSemaphore;
        QObject::connect(&server, &HttpServer::started, [&](){serverEmittedStartedSemaphore.release();});
        QObject::connect(&server, &HttpServer::failed, [](){FAIL("This code is supposed to be unreachable.");});

        WHEN("server is started")
        {
            REQUIRE(!server.isRunning());
            server.start(QHostAddress::LocalHost, 0);

            THEN("server emits started after all its workers start")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverEmittedStartedSemaphore, 10));
                REQUIRE(server.isRunning());
            }
        }
    }
}


SCENARIO("HttpServer closes all connections gracefully before emitting stopped")
{
    GIVEN("a running http server with a connected client")
    {
        HttpServer server;
        REQUIRE(server.connectionCount() == 0);
        QSemaphore serverStartedSemaphore;
        QObject::connect(&server, &HttpServer::started, [&](){serverStartedSemaphore.release();});
        QSemaphore serverStoppedSemaphore;
        QObject::connect(&server, &HttpServer::stopped, [&](){serverStoppedSemaphore.release();});
        QObject::connect(&server, &HttpServer::failed, [](){FAIL("This code is supposed to be unreachable.");});
        REQUIRE(!server.isRunning());
        server.start(QHostAddress::LocalHost, 0);
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStartedSemaphore, 10));
        REQUIRE(server.isRunning());
        TcpSocket clientSocket;
        QSemaphore clientConnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::connected, [&](){clientConnectedSemaphore.release();});
        QSemaphore clientDisconnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::disconnected, [&](){clientDisconnectedSemaphore.release();});
        Object::connect(&clientSocket, &TcpSocket::error, [](){FAIL("This code is supposed to be unreachable.");});
        clientSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientConnectedSemaphore, 10));
        REQUIRE(!clientDisconnectedSemaphore.tryAcquire());
        while (server.connectionCount() != 1)
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents, 1);
        }

        WHEN("server is stopped")
        {
            server.stop();

            THEN("server closes all connections gracefully before emitting stopped")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStoppedSemaphore, 10));
                REQUIRE(server.connectionCount() == 0);
            }
        }
    }
}


SCENARIO("HttpServer can be deleted with established connections")
{
    GIVEN("a running http server with a connected client")
    {
        std::unique_ptr<HttpServer> pServer(new HttpServer);
        REQUIRE(pServer->connectionCount() == 0);
        QSemaphore serverStartedSemaphore;
        QObject::connect(pServer.get(), &HttpServer::started, [&](){serverStartedSemaphore.release();});
        QSemaphore serverStoppedSemaphore;
        QObject::connect(pServer.get(), &HttpServer::stopped, [&](){serverStoppedSemaphore.release();});
        QObject::connect(pServer.get(), &HttpServer::failed, [](){FAIL("This code is supposed to be unreachable.");});
        REQUIRE(!pServer->isRunning());
        pServer->start(QHostAddress::LocalHost, 0);
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStartedSemaphore, 10));
        REQUIRE(pServer->isRunning());
        TcpSocket clientSocket;
        QSemaphore clientConnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::connected, [&](){clientConnectedSemaphore.release();});
        QSemaphore clientDisconnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::disconnected, [&](){clientDisconnectedSemaphore.release();});
        Object::connect(&clientSocket, &TcpSocket::error, [](){FAIL("This code is supposed to be unreachable.");});
        clientSocket.connect(pServer->serverAddress().toString().toStdString(), pServer->serverPort());
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientConnectedSemaphore, 10));
        REQUIRE(!clientDisconnectedSemaphore.tryAcquire());
        while (pServer->connectionCount() != 1)
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents, 1);
        }

        WHEN("server is deleted")
        {
            pServer.reset();

            THEN("server aborts all connections")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientDisconnectedSemaphore, 10));
            }
        }
    }
}


SCENARIO("HttpServer allows limiting max connection count")
{
    GIVEN("a server with a limit on max connection count")
    {
        const auto maxConnectionCount = GENERATE(AS(int64_t), 1, 3, 5);
        HttpServer server;
        REQUIRE(server.setServerOption(HttpServer::ServerOption::MaxConnectionCount, maxConnectionCount));
        REQUIRE(server.connectionCount() == 0);
        QSemaphore serverStartedSemaphore;
        QObject::connect(&server, &HttpServer::started, [&](){serverStartedSemaphore.release();});
        QSemaphore serverStoppedSemaphore;
        QObject::connect(&server, &HttpServer::stopped, [&](){serverStoppedSemaphore.release();});
        QObject::connect(&server, &HttpServer::failed, [](){FAIL("This code is supposed to be unreachable.");});
        REQUIRE(!server.isRunning());
        server.start(QHostAddress::LocalHost, 0);
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStartedSemaphore, 10));
        REQUIRE(server.isRunning());

        WHEN("clients up to max connection limit try to connect to server")
        {
            std::vector<TcpSocket> clients(maxConnectionCount);
            QSemaphore clientConnectedSemaphore;
            QSemaphore clientDisconnectedSemaphore;
            for (auto &client : clients)
            {
                Object::connect(&client, &TcpSocket::connected, [&](){clientConnectedSemaphore.release();});
                Object::connect(&client, &TcpSocket::disconnected, [&](){clientDisconnectedSemaphore.release();});
                Object::connect(&client, &TcpSocket::error, [](){FAIL("This code is supposed to be unreachable.");});
                client.connect(server.serverAddress().toString().toStdString(), server.serverPort());
            }

            THEN("all clients connects to server")
            {
                for (auto i = 0; i < maxConnectionCount; ++i)
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientConnectedSemaphore, 10));
                }
                REQUIRE(!clientDisconnectedSemaphore.tryAcquire());
                while (server.connectionCount() != maxConnectionCount)
                {
                    QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents, 1);
                }

                AND_WHEN("one more client tries to connect to server")
                {
                    TcpSocket oneMoreClient;
                    QSemaphore oneMoreClientConnectedSemaphore;
                    QSemaphore oneMoreClientDisconnectedSemaphore;
                    Object::connect(&oneMoreClient, &TcpSocket::connected, [&](){oneMoreClientConnectedSemaphore.release();});
                    Object::connect(&oneMoreClient, &TcpSocket::disconnected, [&](){oneMoreClientDisconnectedSemaphore.release();});
                    Object::connect(&oneMoreClient, &TcpSocket::error, [](){FAIL("This code is supposed to be unreachable.");});
                    oneMoreClient.connect(server.serverAddress().toString().toStdString(), server.serverPort());

                    THEN("server disconnects client after client connects to server")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(oneMoreClientConnectedSemaphore, 10));
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(oneMoreClientDisconnectedSemaphore, 10));
                        while (server.connectionCount() != maxConnectionCount)
                        {
                            QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents, 1);
                        }
                    }
                }
            }
        }
    }
}


SCENARIO("HttpServer fails if given TLS configuration is not valid")
{
    GIVEN("a valid tls configuration")
    {
        TlsConfiguration tlsConfiguration;
        std::string certificateFile;
        std::string privateKeyFile;
        std::string caCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(TlsTestCertificates::CertificateType::ECDSA, certificateFile, privateKeyFile, caCertificateFile);
        tlsConfiguration.addCaCertificate(caCertificateFile);
        tlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile);

        WHEN("tls configuration is set on server")
        {
            HttpServer server;
            REQUIRE(server.errorMessage().empty());
            const auto succeeded = server.setTlsConfiguration(tlsConfiguration);

            THEN("server successfully sets given tls configuration")
            {
                REQUIRE(succeeded);
                REQUIRE(server.errorMessage().empty());
            }
        }
    }

    GIVEN("a tls configuration without a ca certificate")
    {
        TlsConfiguration invalidTlsConfiguration;
        std::string certificateFile;
        std::string privateKeyFile;
        std::string caCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(TlsTestCertificates::CertificateType::ECDSA, certificateFile, privateKeyFile, caCertificateFile);
        invalidTlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile);

        WHEN("tls configuration is set on server")
        {
            HttpServer server;
            REQUIRE(server.errorMessage().empty());
            const auto succeeded = server.setTlsConfiguration(invalidTlsConfiguration);

            THEN("server fails to set tls configuration")
            {
                REQUIRE(!succeeded);
                REQUIRE(!server.errorMessage().empty());
            }
        }
    }

    GIVEN("a tls configuration with a non-existent ca certificate path")
    {
        TlsConfiguration invalidTlsConfiguration;
        std::string certificateFile;
        std::string privateKeyFile;
        std::string caCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(TlsTestCertificates::CertificateType::ECDSA, certificateFile, privateKeyFile, caCertificateFile);
        invalidTlsConfiguration.addCaCertificate("An invalid path for sure");
        invalidTlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile);

        WHEN("tls configuration is set on server")
        {
            HttpServer server;
            REQUIRE(server.errorMessage().empty());
            const auto succeeded = server.setTlsConfiguration(invalidTlsConfiguration);

            THEN("server fails to set tls configuration")
            {
                REQUIRE(!succeeded);
                REQUIRE(!server.errorMessage().empty());
            }
        }
    }

    GIVEN("a tls configuration with an invalid ca certificate")
    {
        TlsConfiguration invalidTlsConfiguration;
        std::string certificateFile;
        std::string privateKeyFile;
        std::string caCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(TlsTestCertificates::CertificateType::ECDSA, certificateFile, privateKeyFile, caCertificateFile);
        QTemporaryFile invalidCaCertificate;
        REQUIRE(invalidCaCertificate.open());
        invalidCaCertificate.write("An invalid ca certificate content for sure.");
        invalidCaCertificate.flush();
        invalidTlsConfiguration.addCaCertificate(invalidCaCertificate.fileName().toStdString());
        invalidTlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile);

        WHEN("tls configuration is set on server")
        {
            HttpServer server;
            REQUIRE(server.errorMessage().empty());
            const auto succeeded = server.setTlsConfiguration(invalidTlsConfiguration);

            THEN("server fails to set tls configuration")
            {
                REQUIRE(!succeeded);
                REQUIRE(!server.errorMessage().empty());
            }
        }
    }

    GIVEN("a tls configuration with a non-existent certificate")
    {
        TlsConfiguration invalidTlsConfiguration;
        std::string certificateFile;
        std::string privateKeyFile;
        std::string caCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(TlsTestCertificates::CertificateType::ECDSA, certificateFile, privateKeyFile, caCertificateFile);
        invalidTlsConfiguration.addCaCertificate(caCertificateFile);
        invalidTlsConfiguration.setCertificateKeyPair("An invalid certificate path for sure", privateKeyFile);

        WHEN("tls configuration is set on server")
        {
            HttpServer server;
            REQUIRE(server.errorMessage().empty());
            const auto succeeded = server.setTlsConfiguration(invalidTlsConfiguration);

            THEN("server fails to set tls configuration")
            {
                REQUIRE(!succeeded);
                REQUIRE(!server.errorMessage().empty());
            }
        }
    }

    GIVEN("a tls configuration with an invalid certificate")
    {
        TlsConfiguration invalidTlsConfiguration;
        std::string certificateFile;
        std::string privateKeyFile;
        std::string caCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(TlsTestCertificates::CertificateType::ECDSA, certificateFile, privateKeyFile, caCertificateFile);
        QTemporaryFile invalidCertificate;
        REQUIRE(invalidCertificate.open());
        invalidCertificate.write("An invalid certificate content for sure.");
        invalidCertificate.flush();
        invalidTlsConfiguration.addCaCertificate(caCertificateFile);
        invalidTlsConfiguration.setCertificateKeyPair(invalidCertificate.fileName().toStdString(), privateKeyFile);

        WHEN("tls configuration is set on server")
        {
            HttpServer server;
            REQUIRE(server.errorMessage().empty());
            const auto succeeded = server.setTlsConfiguration(invalidTlsConfiguration);

            THEN("server fails to set tls configuration")
            {
                REQUIRE(!succeeded);
                REQUIRE(!server.errorMessage().empty());
            }
        }
    }

    GIVEN("a tls configuration with a non-existent private key")
    {
        TlsConfiguration invalidTlsConfiguration;
        std::string certificateFile;
        std::string privateKeyFile;
        std::string caCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(TlsTestCertificates::CertificateType::ECDSA, certificateFile, privateKeyFile, caCertificateFile);
        invalidTlsConfiguration.addCaCertificate(caCertificateFile);
        invalidTlsConfiguration.setCertificateKeyPair(certificateFile, "An invalid private key path for sure");

        WHEN("tls configuration is set on server")
        {
            HttpServer server;
            REQUIRE(server.errorMessage().empty());
            const auto succeeded = server.setTlsConfiguration(invalidTlsConfiguration);

            THEN("server fails to set tls configuration")
            {
                REQUIRE(!succeeded);
                REQUIRE(!server.errorMessage().empty());
            }
        }
    }

    GIVEN("a tls configuration with an invalid private key")
    {
        TlsConfiguration invalidTlsConfiguration;
        std::string certificateFile;
        std::string privateKeyFile;
        std::string caCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(TlsTestCertificates::CertificateType::ECDSA, certificateFile, privateKeyFile, caCertificateFile);
        QTemporaryFile invalidPrivateKey;
        REQUIRE(invalidPrivateKey.open());
        invalidPrivateKey.write("An invalid private key content for sure.");
        invalidPrivateKey.flush();
        invalidTlsConfiguration.addCaCertificate(caCertificateFile);
        invalidTlsConfiguration.setCertificateKeyPair(certificateFile, invalidPrivateKey.fileName().toStdString());

        WHEN("tls configuration is set on server")
        {
            HttpServer server;
            REQUIRE(server.errorMessage().empty());
            const auto succeeded = server.setTlsConfiguration(invalidTlsConfiguration);

            THEN("server fails to set tls configuration")
            {
                REQUIRE(!succeeded);
                REQUIRE(!server.errorMessage().empty());
            }
        }
    }
}


SCENARIO("HttpServer does not accept TLS 1.2 clients if it is configured to accept TLS 1.3 clients only")
{
    GIVEN("a running server that only accepts TLS 1.3")
    {
        HttpServer server;
        TlsConfiguration serverTlsConfiguration;
        const auto serverTlsVersion = GENERATE(AS(TlsConfiguration::TlsVersion),
                                               TlsConfiguration::TlsVersion::TLS_1_3,
                                               TlsConfiguration::TlsVersion::TLS_1_3_or_newer);
        serverTlsConfiguration.setTlsVersion(serverTlsVersion);
        std::string certificateFile;
        std::string privateKeyFile;
        std::string caCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(TlsTestCertificates::CertificateType::ECDSA, certificateFile, privateKeyFile, caCertificateFile);
        serverTlsConfiguration.setCaCertificates({caCertificateFile});
        serverTlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile);
        REQUIRE(server.setTlsConfiguration(serverTlsConfiguration));
        REQUIRE(server.connectionCount() == 0);
        QSemaphore serverStartedSemaphore;
        QObject::connect(&server, &HttpServer::started, [&](){serverStartedSemaphore.release();});
        QSemaphore serverStoppedSemaphore;
        QObject::connect(&server, &HttpServer::stopped, [&](){serverStoppedSemaphore.release();});
        QObject::connect(&server, &HttpServer::failed, [](){FAIL("This code is supposed to be unreachable.");});
        REQUIRE(!server.isRunning());
        server.start(QHostAddress("127.10.20.50"), 0);
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStartedSemaphore, 10));
        REQUIRE(server.isRunning());

        WHEN("a client that only accetps TLS 1.2 tries to connect to server")
        {
            TlsConfiguration clientTlsConfiguration;
            clientTlsConfiguration.addCaCertificate({caCertificateFile});
            clientTlsConfiguration.setTlsVersion(TlsConfiguration::TlsVersion::TLS_1_2);
            TlsSocket clientSocket(clientTlsConfiguration);
            QSemaphore clientConnectedSemaphore;
            Object::connect(&clientSocket, &TcpSocket::connected, [&](){clientConnectedSemaphore.release();});
            Object::connect(&clientSocket, &TlsSocket::encrypted, [](){FAIL("This code is supposed to be unreachable.");});
            QSemaphore clientDisconnectedSemaphore;
            Object::connect(&clientSocket, &TcpSocket::disconnected, [&](){clientDisconnectedSemaphore.release();});
            QSemaphore clientFailedSemaphore;
            Object::connect(&clientSocket, &TlsSocket::error, [&](){clientFailedSemaphore.release();});
            clientSocket.connect("test.onlocalhost.com", server.serverPort());

            THEN("client establishes tcp connection before server closes it after receiving the client hello tls message")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientDisconnectedSemaphore, 10));
                REQUIRE(!clientFailedSemaphore.tryAcquire());
                while (server.connectionCount() != 0)
                {
                    QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents, 1);
                }
            }
        }

        WHEN("a client that accetps TLS 1.3 tries to connect to server")
        {
            TlsConfiguration clientTlsConfiguration;
            clientTlsConfiguration.setCaCertificates({caCertificateFile});
            const auto clientTlsVersion = GENERATE(AS(TlsConfiguration::TlsVersion),
                                                   TlsConfiguration::TlsVersion::TLS_1_3,
                                                   TlsConfiguration::TlsVersion::TLS_1_3_or_newer);
            clientTlsConfiguration.setTlsVersion(clientTlsVersion);
            TlsSocket clientSocket(clientTlsConfiguration);
            QSemaphore clientConnectedSemaphore;
            Object::connect(&clientSocket, &TcpSocket::connected, [&](){clientConnectedSemaphore.release();});
            QSemaphore clientEncryptedSemaphore;
            Object::connect(&clientSocket, &TlsSocket::encrypted, [&](){clientEncryptedSemaphore.release();});
            QSemaphore clientDisconnectedSemaphore;
            Object::connect(&clientSocket, &TcpSocket::disconnected, [&](){clientDisconnectedSemaphore.release();});
            Object::connect(&clientSocket, &TlsSocket::error, [](){FAIL("This code is supposed to be unreachable.");});
            clientSocket.connect("test.onlocalhost.com", server.serverPort());

            THEN("client establishes encrypted connection")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientEncryptedSemaphore, 10));
                REQUIRE(!clientDisconnectedSemaphore.tryAcquire());
                while (server.connectionCount() != 1)
                {
                    QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents, 1);
                }
            }
        }
    }
}


SCENARIO("HttpServer does not accept TLS 1.3 clients if it is configured to accept TLS 1.2 clients only")
{
    GIVEN("a running server that only accepts TLS 1.2")
    {
        HttpServer server;
        TlsConfiguration serverTlsConfiguration;
        serverTlsConfiguration.setTlsVersion(TlsConfiguration::TlsVersion::TLS_1_2);
        std::string certificateFile;
        std::string privateKeyFile;
        std::string caCertificateFile;
        TlsTestCertificates::getFilesFromCertificateType(TlsTestCertificates::CertificateType::ECDSA, certificateFile, privateKeyFile, caCertificateFile);
        serverTlsConfiguration.setCaCertificates({caCertificateFile});
        serverTlsConfiguration.setCertificateKeyPair(certificateFile, privateKeyFile);
        REQUIRE(server.setTlsConfiguration(serverTlsConfiguration));
        REQUIRE(server.connectionCount() == 0);
        QSemaphore serverStartedSemaphore;
        QObject::connect(&server, &HttpServer::started, [&](){serverStartedSemaphore.release();});
        QSemaphore serverStoppedSemaphore;
        QObject::connect(&server, &HttpServer::stopped, [&](){serverStoppedSemaphore.release();});
        QObject::connect(&server, &HttpServer::failed, [](){FAIL("This code is supposed to be unreachable.");});
        REQUIRE(!server.isRunning());
        server.start(QHostAddress("127.10.20.50"), 0);
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStartedSemaphore, 10));
        REQUIRE(server.isRunning());

        WHEN("a client that only accetps TLS 1.3 tries to connect to server")
        {
            TlsConfiguration clientTlsConfiguration;
            clientTlsConfiguration.addCaCertificate({caCertificateFile});
            clientTlsConfiguration.setTlsVersion(TlsConfiguration::TlsVersion::TLS_1_3);
            TlsSocket clientSocket(clientTlsConfiguration);
            QSemaphore clientConnectedSemaphore;
            Object::connect(&clientSocket, &TcpSocket::connected, [&](){clientConnectedSemaphore.release();});
            Object::connect(&clientSocket, &TlsSocket::encrypted, [](){FAIL("This code is supposed to be unreachable.");});
            QSemaphore clientDisconnectedSemaphore;
            Object::connect(&clientSocket, &TcpSocket::disconnected, [&](){clientDisconnectedSemaphore.release();});
            QSemaphore clientFailedSemaphore;
            Object::connect(&clientSocket, &TlsSocket::error, [&](){clientFailedSemaphore.release();});
            clientSocket.connect("test.onlocalhost.com", server.serverPort());

            THEN("client establishes tcp connection before server closes it after receiving the client hello tls message")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientDisconnectedSemaphore, 10));
                REQUIRE(!clientFailedSemaphore.tryAcquire());
                while (server.connectionCount() != 0)
                {
                    QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents, 1);
                }
            }
        }

        WHEN("a client that accetps TLS 1.2 tries to connect to server")
        {
            TlsConfiguration clientTlsConfiguration;
            clientTlsConfiguration.setCaCertificates({caCertificateFile});
            const auto clientTlsVersion = GENERATE(AS(TlsConfiguration::TlsVersion),
                                                   TlsConfiguration::TlsVersion::TLS_1_2,
                                                   TlsConfiguration::TlsVersion::TLS_1_2_or_newer);
            clientTlsConfiguration.setTlsVersion(clientTlsVersion);
            TlsSocket clientSocket(clientTlsConfiguration);
            QSemaphore clientConnectedSemaphore;
            Object::connect(&clientSocket, &TcpSocket::connected, [&](){clientConnectedSemaphore.release();});
            QSemaphore clientEncryptedSemaphore;
            Object::connect(&clientSocket, &TlsSocket::encrypted, [&](){clientEncryptedSemaphore.release();});
            QSemaphore clientDisconnectedSemaphore;
            Object::connect(&clientSocket, &TcpSocket::disconnected, [&](){clientDisconnectedSemaphore.release();});
            Object::connect(&clientSocket, &TlsSocket::error, [](){FAIL("This code is supposed to be unreachable.");});
            clientSocket.connect("test.onlocalhost.com", server.serverPort());

            THEN("client establishes encrypted connection")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientConnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientEncryptedSemaphore, 10));
                REQUIRE(!clientDisconnectedSemaphore.tryAcquire());
                while (server.connectionCount() != 1)
                {
                    QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents, 1);
                }
            }
        }
    }
}


SCENARIO("HttpServer adds date and time headers")
{
    GIVEN("a running server")
    {
        HttpServer server;
        REQUIRE(server.addRoute(HttpRequest::Method::GET, "/", [](const HttpRequest&, HttpBroker &broker){broker.writeResponse();}));
        REQUIRE(server.connectionCount() == 0);
        QSemaphore serverStartedSemaphore;
        QObject::connect(&server, &HttpServer::started, [&](){serverStartedSemaphore.release();});
        QSemaphore serverStoppedSemaphore;
        QObject::connect(&server, &HttpServer::stopped, [&](){serverStoppedSemaphore.release();});
        QObject::connect(&server, &HttpServer::failed, [](){FAIL("This code is supposed to be unreachable.");});
        REQUIRE(!server.isRunning());
        server.start(QHostAddress("127.0.0.1"), 0);
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStartedSemaphore, 10));
        REQUIRE(server.isRunning());

        WHEN("client sends a get request")
        {
            QNetworkAccessManager networkAccessManager;
            std::unique_ptr<QNetworkReply> pReply;
            QSemaphore networkReplySemaphore;
            QObject::connect(&networkAccessManager, &QNetworkAccessManager::finished, [&](QNetworkReply *reply)
            {
                pReply.reset(reply);
                networkReplySemaphore.release();
            });
            networkAccessManager.get(QNetworkRequest(QUrl(QString("http://127.0.0.1:%1/").arg(server.serverPort()))));

            THEN("server adds content lenght, date and server headers to response")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(networkReplySemaphore, 10));
                REQUIRE(pReply->hasRawHeader("Content-Length"));
                REQUIRE(pReply->hasRawHeader("Date"));
                REQUIRE(pReply->hasRawHeader("Server"));
                REQUIRE(pReply->readAll().isEmpty());
            }
        }
    }
}


SCENARIO("HttpServer processes requests on most specific route")
{
    GIVEN("a running server with three routes mapped")
    {
        HttpServer server;
        REQUIRE(server.addRoute(HttpRequest::Method::GET, "/route", [](const HttpRequest&, HttpBroker &broker){broker.writeResponse({"/route"});}));
        REQUIRE(server.addRoute(HttpRequest::Method::GET, "/router", [](const HttpRequest&, HttpBroker &broker){broker.writeResponse({"/router"});}));
        REQUIRE(server.addRoute(HttpRequest::Method::GET, "/route/104", [](const HttpRequest&, HttpBroker &broker){broker.writeResponse({"/route/104"});}));
        REQUIRE(server.connectionCount() == 0);
        QSemaphore serverStartedSemaphore;
        QObject::connect(&server, &HttpServer::started, [&](){serverStartedSemaphore.release();});
        QSemaphore serverStoppedSemaphore;
        QObject::connect(&server, &HttpServer::stopped, [&](){serverStoppedSemaphore.release();});
        QObject::connect(&server, &HttpServer::failed, [](){FAIL("This code is supposed to be unreachable.");});
        REQUIRE(!server.isRunning());
        server.start(QHostAddress("127.0.0.1"), 0);
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStartedSemaphore, 10));
        REQUIRE(server.isRunning());

        WHEN("client sends a get request")
        {
            QNetworkAccessManager networkAccessManager;
            std::unique_ptr<QNetworkReply> pReply;
            QSemaphore networkReplySemaphore;
            QObject::connect(&networkAccessManager, &QNetworkAccessManager::finished, &networkAccessManager,[&](QNetworkReply *reply)
            {
                pReply.reset(reply);
                networkReplySemaphore.release();
            });
            const auto targetPathAndExpectedRoute = GENERATE(AS(std::pair<QByteArray, QByteArray>),
                                                             {"/route", "/route"},
                                                             {"/routes", "/route"},
                                                             {"/router", "/router"},
                                                             {"/routers", "/router"},
                                                             {"/route/1", "/route"},
                                                             {"/route/10", "/route"},
                                                             {"/route/104", "/route/104"},
                                                             {"/route/1045", "/route/104"},
                                                             {"/routes/104", "/route"},
                                                             {"/router/104", "/router"});
            networkAccessManager.get(QNetworkRequest(QUrl(QString("http://127.0.0.1:%1%2").arg(server.serverPort()).arg(targetPathAndExpectedRoute.first))));

            THEN("server runs expected handler")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(networkReplySemaphore, 10));
                REQUIRE(pReply->readAll() == targetPathAndExpectedRoute.second);
            }
        }
    }
}


namespace Spec::HttpServer
{

class CustomErrorHandler : public ErrorHandler
{
public:
    CustomErrorHandler() = default;
    ~CustomErrorHandler() override = default;
    void handleError(Kourier::HttpServer::ServerError error, std::string_view clientIp, uint16_t clientPort) override
    {
        QMutexLocker locker(&m_mutex);
        m_reportedErrors.push_back(ErrorInfo{.error = error, .clientIp = std::string{clientIp}, .clientPort = clientPort});
    }
    struct ErrorInfo
    {
        Kourier::HttpServer::ServerError error = Kourier::HttpServer::ServerError::NoError;
        std::string clientIp;
        uint16_t clientPort = 0;
    };
    std::vector<ErrorInfo> reportedErrors() const
    {
        QMutexLocker locker(&m_mutex);
        return m_reportedErrors;
    }

private:
    mutable QMutex m_mutex;
    std::vector<ErrorInfo> m_reportedErrors;
};

}

using namespace Spec::HttpServer;


SCENARIO("HttpServer sends HTTP status 408 Request Timeout response, closes connection, and calls error handler with RequestTimeout server error if first byte of request is not received in specified time")
{
    GIVEN("a running server with an idle timeout set and a connected client")
    {
        HttpServer server;
        REQUIRE(server.addRoute(HttpRequest::Method::GET, "/", [](const HttpRequest&, HttpBroker &broker){broker.writeResponse({"Hello World!"});}));
        REQUIRE(server.connectionCount() == 0);
        const int64_t idleTimeoutInSecs = 1;
        server.setServerOption(HttpServer::ServerOption::IdleTimeoutInSecs, idleTimeoutInSecs);
        const auto setHandler = GENERATE(AS(bool), true, false);
        std::shared_ptr<CustomErrorHandler> pErrorHandler;
        if (setHandler)
        {
            pErrorHandler.reset(new CustomErrorHandler);
            server.setErrorHandler(pErrorHandler);
        }
        QSemaphore serverStartedSemaphore;
        QObject::connect(&server, &HttpServer::started, [&](){serverStartedSemaphore.release();});
        QSemaphore serverStoppedSemaphore;
        QObject::connect(&server, &HttpServer::stopped, [&](){serverStoppedSemaphore.release();});
        QObject::connect(&server, &HttpServer::failed, [](){FAIL("This code is supposed to be unreachable.");});
        REQUIRE(!server.isRunning());
        server.start(QHostAddress("127.0.0.1"), 0);
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStartedSemaphore, 10));
        REQUIRE(server.isRunning());
        TcpSocket clientSocket;
        QSemaphore clientConnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::connected, [&](){clientConnectedSemaphore.release();});
        QSemaphore clientDisconnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::disconnected, [&](){clientDisconnectedSemaphore.release();});
        Object::connect(&clientSocket, &TcpSocket::error, [](){FAIL("This code is supposed to be unreachable.");});
        clientSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientConnectedSemaphore, 10));
        const std::string clientIp = std::string(clientSocket.localAddress());
        const auto clientPort = clientSocket.localPort();
        const auto sendPreviousRequest = GENERATE(AS(bool), true, false);
        if (sendPreviousRequest)
        {
            QSemaphore receivedFirstResponseSemaphore;
            Object::connect(&clientSocket, &TcpSocket::receivedData, [&]()
            {
                if (clientSocket.peekAll().ends_with("Hello World!"))
                {
                    clientSocket.readAll();
                    receivedFirstResponseSemaphore.release();
                }
            });
            clientSocket.write("GET / HTTP/1.1\r\nHost: host\r\n\r\n");
            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(receivedFirstResponseSemaphore, 10));
        }
        else
        {
            while (server.connectionCount() != 1)
            {
                QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents, 1);
            }
        }

        WHEN("client stays idle and server does not receive first byte of next request within idleTimeoutInSecs secs")
        {
            QElapsedTimer elapsedTimer;
            elapsedTimer.start();

            THEN("server writes a 408 Request Timeout response and closes connection")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientDisconnectedSemaphore, 10));
                const auto elapsedTime = elapsedTimer.elapsed();
                REQUIRE((0.95*1000*idleTimeoutInSecs) <= elapsedTime && elapsedTime <= (1.05*1000*idleTimeoutInSecs + 1024));
                REQUIRE(clientSocket.readAll().starts_with("HTTP/1.1 408 Request Timeout\r\n"));
                if (setHandler)
                {
                    while(pErrorHandler->reportedErrors().size() != 1)
                    {
                        QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents, 1);
                    }
                }
                server.stop();
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStoppedSemaphore, 10));
                if (setHandler)
                {
                    const auto reportedErrors = pErrorHandler->reportedErrors();
                    REQUIRE(reportedErrors.size() == 1);
                    REQUIRE(reportedErrors[0].error == HttpServer::ServerError::RequestTimeout);
                    REQUIRE(reportedErrors[0].clientIp == clientIp);
                    REQUIRE(reportedErrors[0].clientPort == clientPort);
                }
            }
        }
    }
}


SCENARIO("HttpServer sends HTTP status 408 Request Timeout response, closes connection, and calls error handler with RequestTimeout server error if request is not fully received in specified time")
{
    GIVEN("a running server with a request timeout set and a connected client")
    {
        HttpServer server;
        REQUIRE(server.addRoute(HttpRequest::Method::GET, "/", [](const HttpRequest&, HttpBroker &broker){broker.writeResponse({"Hello World!"});}));
        REQUIRE(server.connectionCount() == 0);
        const int64_t requestTimeoutInSecs = 1;
        server.setServerOption(HttpServer::ServerOption::RequestTimeoutInSecs, requestTimeoutInSecs);
        const auto setHandler = GENERATE(AS(bool), true, false);
        std::shared_ptr<CustomErrorHandler> pErrorHandler;
        if (setHandler)
        {
            pErrorHandler.reset(new CustomErrorHandler);
            server.setErrorHandler(pErrorHandler);
        }
        QSemaphore serverStartedSemaphore;
        QObject::connect(&server, &HttpServer::started, [&](){serverStartedSemaphore.release();});
        QSemaphore serverStoppedSemaphore;
        QObject::connect(&server, &HttpServer::stopped, [&](){serverStoppedSemaphore.release();});
        QObject::connect(&server, &HttpServer::failed, [](){FAIL("This code is supposed to be unreachable.");});
        REQUIRE(!server.isRunning());
        server.start(QHostAddress("127.0.0.1"), 0);
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStartedSemaphore, 10));
        REQUIRE(server.isRunning());
        TcpSocket clientSocket;
        QSemaphore clientConnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::connected, [&](){clientConnectedSemaphore.release();});
        QSemaphore clientDisconnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::disconnected, [&](){clientDisconnectedSemaphore.release();});
        Object::connect(&clientSocket, &TcpSocket::error, [](){FAIL("This code is supposed to be unreachable.");});
        clientSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientConnectedSemaphore, 10));
        const std::string clientIp = std::string(clientSocket.localAddress());
        const auto clientPort = clientSocket.localPort();
        const auto sendPreviousRequest = GENERATE(AS(bool), true, false);
        if (sendPreviousRequest)
        {
            QSemaphore receivedFirstResponseSemaphore;
            Object::connect(&clientSocket, &TcpSocket::receivedData, [&]()
            {
                if (clientSocket.peekAll().ends_with("Hello World!"))
                {
                    clientSocket.readAll();
                    receivedFirstResponseSemaphore.release();
                }
            });
            clientSocket.write("GET / HTTP/1.1\r\nHost: host\r\n\r\n");
            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(receivedFirstResponseSemaphore, 10));
        }
        else
        {
            while (server.connectionCount() != 1)
            {
                QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents, 1);
            }
        }

        WHEN("client stays idle after sending part of a request and server does not receive full request within requestTimeoutInSecs secs")
        {
            clientSocket.write("GET / ");
            QElapsedTimer elapsedTimer;
            elapsedTimer.start();

            THEN("server writes a 408 Request Timeout response and closes connection")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientDisconnectedSemaphore, 10));
                const auto elapsedTime = elapsedTimer.elapsed();
                REQUIRE((0.95*1000*requestTimeoutInSecs) <= elapsedTime && elapsedTime <= (1.05*1000*requestTimeoutInSecs + 1024));
                REQUIRE(clientSocket.readAll().starts_with("HTTP/1.1 408 Request Timeout\r\n"));
                if (setHandler)
                {
                    while(pErrorHandler->reportedErrors().size() != 1)
                    {
                        QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents, 1);
                    }
                }
                server.stop();
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStoppedSemaphore, 10));
                if (setHandler)
                {
                    const auto reportedErrors = pErrorHandler->reportedErrors();
                    REQUIRE(reportedErrors.size() == 1);
                    REQUIRE(reportedErrors[0].error == HttpServer::ServerError::RequestTimeout);
                    REQUIRE(reportedErrors[0].clientIp == clientIp);
                    REQUIRE(reportedErrors[0].clientPort == clientPort);
                }
            }
        }
    }
}


SCENARIO("HttpServer sends HTTP status 400 Bad Request response, closes connection, and calls error handler with Malformed server error if request is malformed")
{
    GIVEN("a running server and a connected client")
    {
        HttpServer server;
        REQUIRE(server.addRoute(HttpRequest::Method::GET, "/", [](const HttpRequest&, HttpBroker &broker){broker.writeResponse({"Hello World!"});}));
        REQUIRE(server.connectionCount() == 0);
        const auto setHandler = GENERATE(AS(bool), true, false);
        std::shared_ptr<CustomErrorHandler> pErrorHandler;
        if (setHandler)
        {
            pErrorHandler.reset(new CustomErrorHandler);
            server.setErrorHandler(pErrorHandler);
        }
        QSemaphore serverStartedSemaphore;
        QObject::connect(&server, &HttpServer::started, [&](){serverStartedSemaphore.release();});
        QSemaphore serverStoppedSemaphore;
        QObject::connect(&server, &HttpServer::stopped, [&](){serverStoppedSemaphore.release();});
        QObject::connect(&server, &HttpServer::failed, [](){FAIL("This code is supposed to be unreachable.");});
        REQUIRE(!server.isRunning());
        server.start(QHostAddress("127.0.0.1"), 0);
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStartedSemaphore, 10));
        REQUIRE(server.isRunning());
        TcpSocket clientSocket;
        QSemaphore clientConnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::connected, [&](){clientConnectedSemaphore.release();});
        QSemaphore clientDisconnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::disconnected, [&](){clientDisconnectedSemaphore.release();});
        Object::connect(&clientSocket, &TcpSocket::error, [](){FAIL("This code is supposed to be unreachable.");});
        clientSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientConnectedSemaphore, 10));
        const std::string clientIp = std::string(clientSocket.localAddress());
        const auto clientPort = clientSocket.localPort();
        const auto sendPreviousRequest = GENERATE(AS(bool), true, false);
        if (sendPreviousRequest)
        {
            QSemaphore receivedFirstResponseSemaphore;
            Object::connect(&clientSocket, &TcpSocket::receivedData, [&]()
            {
                if (clientSocket.peekAll().ends_with("Hello World!"))
                {
                    clientSocket.readAll();
                    receivedFirstResponseSemaphore.release();
                }
            });
            clientSocket.write("GET / HTTP/1.1\r\nHost: host\r\n\r\n");
            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(receivedFirstResponseSemaphore, 10));
        }

        WHEN("client sends a malformed request to server")
        {
            clientSocket.write("GET ?no_slash_here");

            THEN("server writes a 400 Bad Request response and closes connection")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientDisconnectedSemaphore, 10));
                REQUIRE(clientSocket.readAll().starts_with("HTTP/1.1 400 Bad Request\r\n"));
                if (setHandler)
                {
                    while(pErrorHandler->reportedErrors().size() != 1)
                    {
                        QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents, 1);
                    }
                }
                server.stop();
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStoppedSemaphore, 10));
                if (setHandler)
                {
                    const auto reportedErrors = pErrorHandler->reportedErrors();
                    REQUIRE(reportedErrors.size() == 1);
                    REQUIRE(reportedErrors[0].error == HttpServer::ServerError::MalformedRequest);
                    REQUIRE(reportedErrors[0].clientIp == clientIp);
                    REQUIRE(reportedErrors[0].clientPort == clientPort);
                }
            }
        }
    }
}


SCENARIO("HttpServer sends HTTP 500 Internal Server Error code response if handler throws an unhandled exception")
{
    GIVEN("a running server and a connected client")
    {
        HttpServer server;
        REQUIRE(server.addRoute(HttpRequest::Method::GET, "/", [](const HttpRequest&, HttpBroker &broker){broker.writeResponse({"Hello World!"});}));
        REQUIRE(server.addRoute(HttpRequest::Method::GET, "/throw", [](const HttpRequest&, HttpBroker&){throw std::runtime_error{"This is an unhandled exception for sure."};}));
        REQUIRE(server.connectionCount() == 0);
        QSemaphore serverStartedSemaphore;
        QObject::connect(&server, &HttpServer::started, [&](){serverStartedSemaphore.release();});
        QSemaphore serverStoppedSemaphore;
        QObject::connect(&server, &HttpServer::stopped, [&](){serverStoppedSemaphore.release();});
        QObject::connect(&server, &HttpServer::failed, [](){FAIL("This code is supposed to be unreachable.");});
        REQUIRE(!server.isRunning());
        server.start(QHostAddress("127.0.0.1"), 0);
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStartedSemaphore, 10));
        REQUIRE(server.isRunning());
        TcpSocket clientSocket;
        QSemaphore clientConnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::connected, [&](){clientConnectedSemaphore.release();});
        QSemaphore clientDisconnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::disconnected, [&](){clientDisconnectedSemaphore.release();});
        Object::connect(&clientSocket, &TcpSocket::error, [](){FAIL("This code is supposed to be unreachable.");});
        clientSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientConnectedSemaphore, 10));
        const auto sendPreviousRequest = GENERATE(AS(bool), true, false);
        if (sendPreviousRequest)
        {
            QSemaphore receivedFirstResponseSemaphore;
            Object::connect(&clientSocket, &TcpSocket::receivedData, [&]()
            {
                if (clientSocket.peekAll().ends_with("Hello World!"))
                {
                    clientSocket.readAll();
                    receivedFirstResponseSemaphore.release();
                }
            });
            clientSocket.write("GET / HTTP/1.1\r\nHost: host\r\n\r\n");
            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(receivedFirstResponseSemaphore, 10));
        }

        WHEN("client sends a request to server and the handler throws an unhandled exception")
        {
            clientSocket.write("GET /throw HTTP/1.1\r\nHost: host\r\n\r\n");

            THEN("server catches unhandled exception, sends a 500 Internal Server Error response, and closes the connection")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientDisconnectedSemaphore, 10));
                REQUIRE(clientSocket.readAll().starts_with("HTTP/1.1 500 Internal Server Error\r\n"));
                server.stop();
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStoppedSemaphore, 10));
            }
        }
    }
}


SCENARIO("HttpServer sends HTTP 404 Not Found code response, closes connection, and calls error handler with MalformedRequest server error if no handler is found for request")
{
    GIVEN("a running server and a connected client")
    {
        HttpServer server;
        REQUIRE(server.addRoute(HttpRequest::Method::GET, "/", [](const HttpRequest&, HttpBroker &broker){broker.writeResponse({"Hello World!"});}));
        REQUIRE(server.connectionCount() == 0);
        const auto setHandler = GENERATE(AS(bool), true, false);
        std::shared_ptr<CustomErrorHandler> pErrorHandler;
        if (setHandler)
        {
            pErrorHandler.reset(new CustomErrorHandler);
            server.setErrorHandler(pErrorHandler);
        }
        QSemaphore serverStartedSemaphore;
        QObject::connect(&server, &HttpServer::started, [&](){serverStartedSemaphore.release();});
        QSemaphore serverStoppedSemaphore;
        QObject::connect(&server, &HttpServer::stopped, [&](){serverStoppedSemaphore.release();});
        QObject::connect(&server, &HttpServer::failed, [](){FAIL("This code is supposed to be unreachable.");});
        REQUIRE(!server.isRunning());
        server.start(QHostAddress("127.0.0.1"), 0);
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStartedSemaphore, 10));
        REQUIRE(server.isRunning());
        TcpSocket clientSocket;
        QSemaphore clientConnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::connected, [&](){clientConnectedSemaphore.release();});
        QSemaphore clientDisconnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::disconnected, [&](){clientDisconnectedSemaphore.release();});
        Object::connect(&clientSocket, &TcpSocket::error, [](){FAIL("This code is supposed to be unreachable.");});
        clientSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientConnectedSemaphore, 10));
        const std::string clientIp = std::string(clientSocket.localAddress());
        const auto clientPort = clientSocket.localPort();
        const auto sendPreviousRequest = GENERATE(AS(bool), true, false);
        if (sendPreviousRequest)
        {
            QSemaphore receivedFirstResponseSemaphore;
            Object::connect(&clientSocket, &TcpSocket::receivedData, [&]()
            {
                if (clientSocket.peekAll().ends_with("Hello World!"))
                {
                    clientSocket.readAll();
                    receivedFirstResponseSemaphore.release();
                }
            });
            clientSocket.write("GET / HTTP/1.1\r\nHost: host\r\n\r\n");
            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(receivedFirstResponseSemaphore, 10));
        }

        WHEN("client sends a request targeting an unmapped resource")
        {
            clientSocket.write("POST / HTTP/1.1\r\nHost: host\r\n\r\n");

            THEN("server sends a 404 Not Found response, and closes the connection")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientDisconnectedSemaphore, 10));
                REQUIRE(clientSocket.readAll().starts_with("HTTP/1.1 404 Not Found\r\n"));
                if (setHandler)
                {
                    while(pErrorHandler->reportedErrors().size() != 1)
                    {
                        QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents, 1);
                    }
                }
                server.stop();
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStoppedSemaphore, 10));
                if (setHandler)
                {
                    const auto reportedErrors = pErrorHandler->reportedErrors();
                    REQUIRE(reportedErrors.size() == 1);
                    REQUIRE(reportedErrors[0].error == HttpServer::ServerError::MalformedRequest);
                    REQUIRE(reportedErrors[0].clientIp == clientIp);
                    REQUIRE(reportedErrors[0].clientPort == clientPort);
                }
            }
        }
    }
}


SCENARIO("HttpServer sends HTTP status 400 Bad Request response, closes connection, and calls error handler with TooBigRequest server error if request is too big")
{
    GIVEN("a running server and a connected client")
    {
        HttpServer server;
        const auto entityToLimit = GENERATE(AS(HttpServer::ServerOption),
                                            HttpServer::ServerOption::MaxUrlSize,
                                            HttpServer::ServerOption::MaxHeaderNameSize,
                                            HttpServer::ServerOption::MaxHeaderValueSize,
                                            HttpServer::ServerOption::MaxHeaderLineCount,
                                            HttpServer::ServerOption::MaxTrailerNameSize,
                                            HttpServer::ServerOption::MaxTrailerValueSize,
                                            HttpServer::ServerOption::MaxTrailerLineCount,
                                            HttpServer::ServerOption::MaxChunkMetadataSize,
                                            HttpServer::ServerOption::MaxRequestSize,
                                            HttpServer::ServerOption::MaxBodySize,
                                            HttpServer::ServerOption::TcpServerBacklogSize);
        REQUIRE(server.setServerOption(entityToLimit, 10));
        REQUIRE(server.setServerOption(HttpServer::ServerOption::WorkerCount, 1));
        REQUIRE(server.addRoute(HttpRequest::Method::POST, "/", [](const HttpRequest &request, HttpBroker &broker)
        {
            if (request.isComplete())
                broker.writeResponse("Hello World!");
            else
            {
                broker.setQObject(new QObject);
                QObject::connect(&broker, &HttpBroker::receivedBodyData, [&](std::string_view data, bool isLastPart)
                {
                    if (isLastPart)
                        broker.writeResponse("Hello World!");
                });
            }
        }));
        REQUIRE(server.connectionCount() == 0);
        QSemaphore serverStartedSemaphore;
        QObject::connect(&server, &HttpServer::started, [&](){serverStartedSemaphore.release();});
        QSemaphore serverStoppedSemaphore;
        QObject::connect(&server, &HttpServer::stopped, [&](){serverStoppedSemaphore.release();});
        QObject::connect(&server, &HttpServer::failed, [](){FAIL("This code is supposed to be unreachable.");});
        REQUIRE(!server.isRunning());
        server.start(QHostAddress("127.0.0.1"), 0);
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStartedSemaphore, 10));
        REQUIRE(server.isRunning());
        TcpSocket clientSocket;
        QSemaphore clientConnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::connected, [&](){clientConnectedSemaphore.release();});
        QSemaphore receivedResponseSemaphore;
        Object::connect(&clientSocket, &TcpSocket::receivedData, [&]()
        {
            if (clientSocket.peekAll().ends_with("Hello World!")
                || (clientSocket.peekAll().starts_with("HTTP/1.1 400 Bad Request\r\n")
                    && clientSocket.peekAll().ends_with("\r\n\r\n")))
            {
                receivedResponseSemaphore.release();
            }
        });
        QSemaphore clientDisconnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::disconnected, [&](){clientDisconnectedSemaphore.release();});
        Object::connect(&clientSocket, &TcpSocket::error, [](){FAIL("This code is supposed to be unreachable.");});
        clientSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientConnectedSemaphore, 10));

        WHEN("client sends request to server")
        {
            clientSocket.write("POST /?everybody_loves_a_query HTTP/1.1\r\n"
                               "Host: host\r\n"
                               "name1: value1\r\n"
                               "name2: value2\r\n"
                               "name3: value3\r\n"
                               "name4: value4\r\n"
                               "name5: value5\r\n"
                               "name6: value6\r\n"
                               "name7: value7\r\n"
                               "name8: value8\r\n"
                               "name9: value9\r\n"
                               "a_really_large_name_indeed: a_really_large_value_indeed\r\n"
                               "Content-Length: 12\r\n"
                               "\r\n"
                               "Hello World!");

            THEN("server responds with 400 Bad Request if request is bigger than server is allowed to accept")
            {
                bool expectBadRequestResponse;
                switch(entityToLimit)
                {
                    case Kourier::HttpServer::ServerOption::MaxUrlSize:
                    case Kourier::HttpServer::ServerOption::MaxHeaderNameSize:
                    case Kourier::HttpServer::ServerOption::MaxHeaderValueSize:
                    case Kourier::HttpServer::ServerOption::MaxHeaderLineCount:
                    case Kourier::HttpServer::ServerOption::MaxRequestSize:
                    case Kourier::HttpServer::ServerOption::MaxBodySize:
                        expectBadRequestResponse = true;
                        break;
                    case Kourier::HttpServer::ServerOption::MaxTrailerNameSize:
                    case Kourier::HttpServer::ServerOption::MaxTrailerValueSize:
                    case Kourier::HttpServer::ServerOption::MaxTrailerLineCount:
                    case Kourier::HttpServer::ServerOption::MaxChunkMetadataSize:
                    default:
                        expectBadRequestResponse = false;
                        break;
                }
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(receivedResponseSemaphore, 10));
                const std::string response{clientSocket.readAll()};
                if (expectBadRequestResponse)
                {
                    REQUIRE(response.starts_with("HTTP/1.1 400 Bad Request\r\n")
                            && response.ends_with("\r\n\r\n"));
                }
                else
                {
                    REQUIRE(response.starts_with("HTTP/1.1 200 OK\r\n")
                            && response.ends_with("Hello World!"));
                }
            }
        }

        WHEN("client sends a request with chunked body to server")
        {
            clientSocket.write("POST /?everybody_loves_a_query HTTP/1.1\r\n"
                               "Host: host\r\n"
                               "name1: value1\r\n"
                               "name2: value2\r\n"
                               "name3: value3\r\n"
                               "name4: value4\r\n"
                               "name5: value5\r\n"
                               "name6: value6\r\n"
                               "name7: value7\r\n"
                               "name8: value8\r\n"
                               "name9: value9\r\n"
                               "a_really_large_name_indeed: a_really_large_value_indeed\r\n"
                               "Transfer-Encoding: chunked\r\n\r\n"
                               "5\r\nHello\r\n"
                               "1;everybody_loves_a_chunk_extension=\"true\"\r\n \r\n"
                               "F\r\nWonderful World\r\n"
                               "0\r\n"
                               "name1: value1\r\n"
                               "name2: value2\r\n"
                               "name3: value3\r\n"
                               "name4: value4\r\n"
                               "name5: value5\r\n"
                               "name6: value6\r\n"
                               "name7: value7\r\n"
                               "name8: value8\r\n"
                               "name9: value9\r\n"
                               "name10: value10\r\n"
                               "a_really_large_name_indeed: a_really_large_value_indeed\r\n\r\n");

            THEN("server responds with 400 Bad Request if request is bigger than server is allowed to accept")
            {
                bool expectBadRequestResponse;
                switch(entityToLimit)
                {
                    case Kourier::HttpServer::ServerOption::MaxUrlSize:
                    case Kourier::HttpServer::ServerOption::MaxHeaderNameSize:
                    case Kourier::HttpServer::ServerOption::MaxHeaderValueSize:
                    case Kourier::HttpServer::ServerOption::MaxHeaderLineCount:
                    case Kourier::HttpServer::ServerOption::MaxRequestSize:
                    case Kourier::HttpServer::ServerOption::MaxBodySize:
                    case Kourier::HttpServer::ServerOption::MaxTrailerNameSize:
                    case Kourier::HttpServer::ServerOption::MaxTrailerValueSize:
                    case Kourier::HttpServer::ServerOption::MaxTrailerLineCount:
                    case Kourier::HttpServer::ServerOption::MaxChunkMetadataSize:
                        expectBadRequestResponse = true;
                        break;
                    default:
                        expectBadRequestResponse = false;
                        break;
                }
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(receivedResponseSemaphore, 10));
                const std::string response{clientSocket.readAll()};
                if (expectBadRequestResponse)
                {
                    REQUIRE(response.starts_with("HTTP/1.1 400 Bad Request\r\n")
                            && response.ends_with("\r\n\r\n"));
                }
                else
                {
                    REQUIRE(response.starts_with("HTTP/1.1 200 OK\r\n")
                            && response.ends_with("Hello World!"));
                }
            }
        }
    }
}


SCENARIO("HttpServer calls handler with request containing all body data available at the time the request metadata (request line + headers) was parsed and remaining body data through broker")
{
    GIVEN("a running server and a connected client")
    {
        HttpServer server;
        REQUIRE(server.setServerOption(HttpServer::ServerOption::WorkerCount, 1));
        static const constinit std::string_view fullRequestBody("Hello World!");
        static std::string requestBody;
        static QSemaphore serverReceivedRequest;
        static constinit size_t pendingBodySize = 0;
        static QSemaphore brokerReceivedRemainingBodyData;
        static QSemaphore objectDeletedSemaphore;
        static std::string requestBodyReceivedThroughBroker;
        REQUIRE(server.addRoute(HttpRequest::Method::POST, "/data", [](const HttpRequest &request, HttpBroker &broker)
        {
            REQUIRE(request.method() == HttpRequest::Method::POST);
            REQUIRE(!request.chunked());
            REQUIRE(request.hasBody());
            REQUIRE(!request.isComplete());
            REQUIRE(request.targetQuery().empty());
            REQUIRE(request.targetPath() == "/data");
            REQUIRE(request.bodyType() == HttpRequest::BodyType::NotChunked);
            REQUIRE(request.requestBodySize() == fullRequestBody.size());
            pendingBodySize = request.pendingBodySize();
            requestBody.append(request.body());
            auto *pObject = new QObject;
            QObject::connect(pObject, &QObject::destroyed, [](){objectDeletedSemaphore.release();});
            broker.setQObject(pObject);
            QObject::connect(&broker, &HttpBroker::receivedBodyData, [&](std::string_view data, bool isLastPart)
            {
                requestBodyReceivedThroughBroker.append(data);
                if (isLastPart)
                {
                    brokerReceivedRemainingBodyData.release();
                    broker.writeResponse();
                }
            });
            serverReceivedRequest.release();
        }));
        REQUIRE(server.connectionCount() == 0);
        QSemaphore serverStartedSemaphore;
        QObject::connect(&server, &HttpServer::started, [&](){serverStartedSemaphore.release();});
        QSemaphore serverStoppedSemaphore;
        QObject::connect(&server, &HttpServer::stopped, [&](){serverStoppedSemaphore.release();});
        QObject::connect(&server, &HttpServer::failed, [](){FAIL("This code is supposed to be unreachable.");});
        REQUIRE(!server.isRunning());
        server.start(QHostAddress("127.0.0.1"), 0);
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStartedSemaphore, 10));
        REQUIRE(server.isRunning());
        TcpSocket clientSocket;
        QSemaphore clientConnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::connected, [&](){clientConnectedSemaphore.release();});
        QSemaphore clientDisconnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::disconnected, [&](){clientDisconnectedSemaphore.release();});
        Object::connect(&clientSocket, &TcpSocket::error, [](){FAIL("This code is supposed to be unreachable.");});
        clientSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientConnectedSemaphore, 10));

        WHEN("client sends a request with partial body data")
        {
            clientSocket.write("POST /data HTTP/1.1\r\nHost: host\r\nContent-Length: 12\r\n\r\nHello ");

            THEN("server calls handler with partial body data")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverReceivedRequest, 10));
                REQUIRE(requestBody == "Hello ");
                REQUIRE(pendingBodySize == (fullRequestBody.size() - requestBody.size()));

                AND_WHEN("client sends remaining request data to server")
                {
                    REQUIRE(!brokerReceivedRemainingBodyData.tryAcquire());
                    REQUIRE(!objectDeletedSemaphore.tryAcquire());
                    clientSocket.write("World!");

                    THEN("server sends remaining data to handler through broker, which responds and server destroys set QObject")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(brokerReceivedRemainingBodyData, 10));
                        REQUIRE((requestBody + requestBodyReceivedThroughBroker) == fullRequestBody);
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(objectDeletedSemaphore, 10));
                    }
                }
            }
        }
    }
}


SCENARIO("HttpServer sends chunked body data through broker")
{
    GIVEN("a running server and a connected client")
    {
        HttpServer server;
        REQUIRE(server.setServerOption(HttpServer::ServerOption::WorkerCount, 1));
        static const constinit std::string_view fullRequestBody("Hello World!");
        static QSemaphore serverReceivedRequest;
        static QSemaphore brokerReceivedRemainingBodyData;
        static QSemaphore objectDeletedSemaphore;
        static std::string requestBodyReceivedThroughBroker;
        requestBodyReceivedThroughBroker.clear();
        const auto sendTrailer = GENERATE(AS(bool), true, false);
        static bool hasToSendTrailer;
        hasToSendTrailer = sendTrailer;
        static QSemaphore receivedTrailerSemaphore;
        REQUIRE(server.addRoute(HttpRequest::Method::POST, "/data", [](const HttpRequest &request, HttpBroker &broker)
        {
            REQUIRE(request.method() == HttpRequest::Method::POST);
            REQUIRE(request.chunked());
            REQUIRE(!request.hasBody());
            REQUIRE(!request.isComplete());
            REQUIRE(request.targetQuery().empty());
            REQUIRE(request.targetPath() == "/data");
            REQUIRE(request.bodyType() == HttpRequest::BodyType::Chunked);
            auto *pObject = new QObject;
            QObject::connect(pObject, &QObject::destroyed, [](){objectDeletedSemaphore.release();});
            broker.setQObject(pObject);
            QObject::connect(&broker, &HttpBroker::receivedBodyData, [&](std::string_view data, bool isLastPart)
            {
                requestBodyReceivedThroughBroker.append(data);
                if (isLastPart)
                {
                    REQUIRE(data.empty());
                    if (hasToSendTrailer)
                    {
                        REQUIRE(broker.hasTrailers());
                        REQUIRE(broker.trailersCount() == 2);
                        REQUIRE(broker.hasTrailer("name1"));
                        REQUIRE(broker.trailer("name1") == "value1");
                        REQUIRE(broker.hasTrailer("name2"));
                        REQUIRE(broker.trailer("name2") == "value2");
                        receivedTrailerSemaphore.release();
                    }
                    brokerReceivedRemainingBodyData.release();
                    broker.writeResponse();
                }
                else
                {
                    REQUIRE(!broker.hasTrailers());
                }
            });
            serverReceivedRequest.release();
        }));
        REQUIRE(server.connectionCount() == 0);
        QSemaphore serverStartedSemaphore;
        QObject::connect(&server, &HttpServer::started, [&](){serverStartedSemaphore.release();});
        QSemaphore serverStoppedSemaphore;
        QObject::connect(&server, &HttpServer::stopped, [&](){serverStoppedSemaphore.release();});
        QObject::connect(&server, &HttpServer::failed, [](){FAIL("This code is supposed to be unreachable.");});
        REQUIRE(!server.isRunning());
        server.start(QHostAddress("127.0.0.1"), 0);
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStartedSemaphore, 10));
        REQUIRE(server.isRunning());
        TcpSocket clientSocket;
        QSemaphore clientConnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::connected, [&](){clientConnectedSemaphore.release();});
        QSemaphore clientDisconnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::disconnected, [&](){clientDisconnectedSemaphore.release();});
        Object::connect(&clientSocket, &TcpSocket::error, [](){FAIL("This code is supposed to be unreachable.");});
        clientSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientConnectedSemaphore, 10));

        WHEN("client sends a chunked request to server")
        {
            clientSocket.write("POST /data HTTP/1.1\r\nHost: host\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n1\r\n \r\n6\r\nWorld!\r\n0\r\n");
            if (hasToSendTrailer)
                clientSocket.write("name1: value1\r\nname2: value2\r\n\r\n");
            else
                clientSocket.write("\r\n");

            THEN("server calls handler without any body data and sends all body data through broker, which responds when it received the last part, making the server destroy the set QObject")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverReceivedRequest, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(brokerReceivedRemainingBodyData, 10));
                REQUIRE(requestBodyReceivedThroughBroker == fullRequestBody);
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(objectDeletedSemaphore, 10));
                if (hasToSendTrailer)
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(receivedTrailerSemaphore, 10));
                }
            }
        }
    }
}


SCENARIO("HttpServer closes connection if handler function neither fully responds the request nor sets an object")
{
    GIVEN("a running server and a connected client")
    {
        HttpServer server;
        REQUIRE(server.addRoute(HttpRequest::Method::GET, "/neither_responds_nor_sets_an_qobject", [](const HttpRequest&, HttpBroker &){}));
        REQUIRE(server.connectionCount() == 0);
        QSemaphore serverStartedSemaphore;
        QObject::connect(&server, &HttpServer::started, [&](){serverStartedSemaphore.release();});
        QSemaphore serverStoppedSemaphore;
        QObject::connect(&server, &HttpServer::stopped, [&](){serverStoppedSemaphore.release();});
        QObject::connect(&server, &HttpServer::failed, [](){FAIL("This code is supposed to be unreachable.");});
        REQUIRE(!server.isRunning());
        server.start(QHostAddress("127.0.0.1"), 0);
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStartedSemaphore, 10));
        REQUIRE(server.isRunning());
        TcpSocket clientSocket;
        QSemaphore clientConnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::connected, [&](){clientConnectedSemaphore.release();});
        QSemaphore clientDisconnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::disconnected, [&](){clientDisconnectedSemaphore.release();});
        Object::connect(&clientSocket, &TcpSocket::error, [](){FAIL("This code is supposed to be unreachable.");});
        clientSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientConnectedSemaphore, 10));

        WHEN("client sends a request to server")
        {
            clientSocket.write("GET /neither_responds_nor_sets_an_qobject HTTP/1.1\r\nHost: host\r\n\r\n");

            THEN("server closes connection")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientDisconnectedSemaphore, 10));
                REQUIRE(clientSocket.readAll().empty());
                server.stop();
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStoppedSemaphore, 10));
            }
        }
    }
}


SCENARIO("HttpServer allows handler to respond to a request before it is fully received")
{
    GIVEN("a running server and a connected client")
    {
        HttpServer server;
        const auto setQObject = GENERATE(AS(bool), true, false);
        static bool hasToSetQObject;
        hasToSetQObject = setQObject;
        static QSemaphore deletedQObjectSemaphore;
        REQUIRE(server.addRoute(HttpRequest::Method::POST, "/fast_responder", [](const HttpRequest&, HttpBroker &broker)
        {
            broker.writeResponse("Hello World!");
            if (hasToSetQObject)
            {
                auto *pObject = new QObject;
                QObject::connect(pObject, &QObject::destroyed, [](){deletedQObjectSemaphore.release();});
                broker.setQObject(pObject);
            }
        }));
        REQUIRE(server.connectionCount() == 0);
        QSemaphore serverStartedSemaphore;
        QObject::connect(&server, &HttpServer::started, [&](){serverStartedSemaphore.release();});
        QSemaphore serverStoppedSemaphore;
        QObject::connect(&server, &HttpServer::stopped, [&](){serverStoppedSemaphore.release();});
        QObject::connect(&server, &HttpServer::failed, [](){FAIL("This code is supposed to be unreachable.");});
        REQUIRE(!server.isRunning());
        server.start(QHostAddress("127.0.0.1"), 0);
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStartedSemaphore, 10));
        REQUIRE(server.isRunning());
        TcpSocket clientSocket;
        QSemaphore clientConnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::connected, [&](){clientConnectedSemaphore.release();});
        QSemaphore receivedResponseSemaphore;
        Object::connect(&clientSocket, &TcpSocket::receivedData, [&]()
        {
            if (clientSocket.peekAll().ends_with("Hello World!"))
            {
                clientSocket.readAll();
                receivedResponseSemaphore.release();
            }
        });
        QSemaphore clientDisconnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::disconnected, [&](){clientDisconnectedSemaphore.release();});
        Object::connect(&clientSocket, &TcpSocket::error, [](){FAIL("This code is supposed to be unreachable.");});
        clientSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientConnectedSemaphore, 10));

        WHEN("client sends a request without body data to server")
        {
            clientSocket.write("POST /fast_responder HTTP/1.1\r\nHost: host\r\nContent-Length: 5\r\n\r\n");

            THEN("handler responds without receiving full request body")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(receivedResponseSemaphore, 10));

                AND_WHEN("client sends request body data")
                {
                    clientSocket.write("Hello");

                    THEN("server destroys any set QObject")
                    {
                        if (hasToSetQObject)
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(deletedQObjectSemaphore, 10));
                        }
                        server.stop();
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStoppedSemaphore, 10));
                    }
                }
            }
        }
    }
}


SCENARIO("HttpServer supports pipelined requests")
{
    GIVEN("a running server and a connected client")
    {
        HttpServer server;
        REQUIRE(server.addRoute(HttpRequest::Method::GET, "/", [](const HttpRequest &request, HttpBroker &broker)
        {
            static size_t counter{0};
            if (request.isComplete())
                broker.writeResponse(std::string("Hello World ").append(std::to_string(++counter)));
            else
            {
                broker.setQObject(new QObject);
                QObject::connect(&broker, &HttpBroker::receivedBodyData, [&](std::string_view data, bool isLastPart)
                {
                    if (isLastPart)
                        broker.writeResponse(std::string("Hello World ").append(std::to_string(++counter)));
                });
            }
        }));
        REQUIRE(server.connectionCount() == 0);
        QSemaphore serverStartedSemaphore;
        QObject::connect(&server, &HttpServer::started, [&](){serverStartedSemaphore.release();});
        QSemaphore serverStoppedSemaphore;
        QObject::connect(&server, &HttpServer::stopped, [&](){serverStoppedSemaphore.release();});
        QObject::connect(&server, &HttpServer::failed, [](){FAIL("This code is supposed to be unreachable.");});
        REQUIRE(!server.isRunning());
        server.start(QHostAddress("127.0.0.1"), 0);
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStartedSemaphore, 10));
        REQUIRE(server.isRunning());
        TcpSocket clientSocket;
        QSemaphore clientConnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::connected, [&](){clientConnectedSemaphore.release();});
        QSemaphore receivedResponseSemaphore;
        Object::connect(&clientSocket, &TcpSocket::receivedData, [&]()
        {
            if (clientSocket.peekAll().ends_with("Hello World 3"))
            {
                clientSocket.readAll();
                receivedResponseSemaphore.release();
            }
        });
        QSemaphore clientDisconnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::disconnected, [&](){clientDisconnectedSemaphore.release();});
        Object::connect(&clientSocket, &TcpSocket::error, [](){FAIL("This code is supposed to be unreachable.");});
        clientSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientConnectedSemaphore, 10));

        WHEN("client sends three pipelined requests to server")
        {
            clientSocket.write("GET / HTTP/1.1\r\nHost: host\r\n\r\n"
                               "GET / HTTP/1.1\r\nHost: host\r\nTransfer-Encoding: chunked\r\n\r\n"
                               "5\r\nHello\r\n"
                               "1\r\n \r\n"
                               "A\r\nWonderfull\r\n"
                               "1\r\n \r\n"
                               "6\r\nWorld!\r\n"
                               "0\r\n\r\n"
                               "GET / HTTP/1.1\r\nHost: host\r\n\r\n");

            THEN("server processes sent pipelined requests")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(receivedResponseSemaphore, 10));
                server.stop();
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStoppedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientDisconnectedSemaphore, 10));
            }
        }
    }
}


SCENARIO("HttpServer does not timeout after a complete request is received")
{
    GIVEN("a running server with request and idle timeouts and a connected client")
    {
        HttpServer server;
        server.setServerOption(HttpServer::ServerOption::RequestTimeoutInSecs, 1);
        server.setServerOption(HttpServer::ServerOption::IdleTimeoutInSecs, 1);
        REQUIRE(server.addRoute(HttpRequest::Method::GET, "/", [](const HttpRequest&, HttpBroker &broker)
        {
            broker.setQObject(new QObject);
        }));
        REQUIRE(server.connectionCount() == 0);
        QSemaphore serverStartedSemaphore;
        QObject::connect(&server, &HttpServer::started, [&](){serverStartedSemaphore.release();});
        QSemaphore serverStoppedSemaphore;
        QObject::connect(&server, &HttpServer::stopped, [&](){serverStoppedSemaphore.release();});
        QObject::connect(&server, &HttpServer::failed, [](){FAIL("This code is supposed to be unreachable.");});
        REQUIRE(!server.isRunning());
        server.start(QHostAddress("127.0.0.1"), 0);
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStartedSemaphore, 10));
        REQUIRE(server.isRunning());
        TcpSocket clientSocket;
        QSemaphore clientConnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::connected, [&](){clientConnectedSemaphore.release();});
        QSemaphore clientDisconnectedSemaphore;
        Object::connect(&clientSocket, &TcpSocket::disconnected, [&](){clientDisconnectedSemaphore.release();});
        Object::connect(&clientSocket, &TcpSocket::error, [](){FAIL("This code is supposed to be unreachable.");});
        clientSocket.connect(server.serverAddress().toString().toStdString(), server.serverPort());
        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientConnectedSemaphore, 10));

        WHEN("client sends a request to server")
        {
            clientSocket.write("GET / HTTP/1.1\r\nHost: host\r\n\r\n");

            THEN("server keeps awaiting for handler to respond without timing out")
            {
                REQUIRE(!SemaphoreAwaiter::signalSlotAwareWait(clientDisconnectedSemaphore, 3));
                server.stop();
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStoppedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientDisconnectedSemaphore, 10));
            }
        }
    }
}
