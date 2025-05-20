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

#include <Kourier>
#include "EmitterLibrary/Emitter.h"
#include "ReceiverLibrary/Receiver.h"
#include "../Resources/TlsTestCertificates.h"
#include "../Spectator/Spectator.h"
#include <QSemaphore>


using Kourier::Object;
using Kourier::Timer;
using Kourier::TcpSocket;
using Kourier::TlsConfiguration;
using Kourier::TlsSocket;
using Kourier::HttpRequest;
using Kourier::HttpBroker;
using Kourier::ErrorHandler;
using Kourier::HttpServer;
using Kourier::Signal;
using Kourier::TestResources::TlsTestCertificates;
using Spectator::SemaphoreAwaiter;


namespace Spec::Product
{

class LocalEmitter : public Object
{
    KOURIER_OBJECT(Spec::Product::LocalEmitter)
public:
    LocalEmitter() = default;
    ~LocalEmitter() override = default;
    Signal valueChanged(int value);
};

Signal LocalEmitter::valueChanged(int value) KOURIER_SIGNAL(&LocalEmitter::valueChanged, value)

class LocalReceiver : public Object
{
    KOURIER_OBJECT(Spec::Product::LocalReceiver)
public:
    LocalReceiver() = default;
    ~LocalReceiver() override = default;
    void onValueChanged(int value) {m_value = value;}
    int value() const {return m_value;}

private:
    int m_value = 0;
};

}

using namespace Spec::Product;


SCENARIO("Kourier library supports signal-slot connections")
{
    GIVEN("a signal-slot connection")
    {
        LocalEmitter emitter;
        LocalReceiver receiver;
        Object::connect(&emitter, &LocalEmitter::valueChanged, &receiver, &LocalReceiver::onValueChanged);

        WHEN("signal is emitted")
        {
            const auto value = GENERATE(AS(int), 0, 1, 3, -8, 1245);
            emitter.valueChanged(value);

            THEN("slot is called")
            {
                REQUIRE(receiver.value() == value);
            }
        }
    }
}


SCENARIO("Kourier library supports signal-slot connections accross shared library boundaries")
{
    GIVEN("a signal-slot connection")
    {
        Emitter emitter;
        Receiver receiver;
        Object::connect(&emitter, &Emitter::valueChanged, &receiver, &Receiver::onValueChanged);

        WHEN("signal is emitted")
        {
            const auto value = GENERATE(AS(int), 0, 1, 3, -8, 1245);
            emitter.valueChanged(value);

            THEN("slot is called")
            {
                REQUIRE(receiver.value() == value);
            }
        }
    }
}


SCENARIO("Kourier library supports timers")
{
    GIVEN("a timer set to timeout in 1 second")
    {
        Timer timer;
        timer.setInterval(1000);
        QSemaphore timeoutSemaphore;
        Object::connect(&timer, &Timer::timeout, [&](){timeoutSemaphore.release();});

        WHEN("timer is started")
        {
            timer.start(1000);

            THEN("timer expire in 1 second")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(timeoutSemaphore, 10));
            }
        }
    }
}


namespace Spec::Product
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

using namespace Spec::Product;


SCENARIO("Kourier library supports TcpServer, TlsServer and HttpServer and ErrorHandler")
{
    GIVEN("a running server")
    {
        HttpServer server;
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

        WHEN("a client tries to connect to server")
        {
            TcpSocket clientSocket;
            QSemaphore clientConnectedSemaphore;
            Object::connect(&clientSocket, &TcpSocket::connected, [&](){clientConnectedSemaphore.release();});
            QSemaphore clientDisconnectedSemaphore;
            Object::connect(&clientSocket, &TcpSocket::disconnected, [&](){clientDisconnectedSemaphore.release();});
            Object::connect(&clientSocket, &TcpSocket::error, [](){FAIL("This code is supposed to be unreachable.");});
            clientSocket.connect("127.0.0.1", server.serverPort());

            THEN("client establishes connection")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientConnectedSemaphore, 10));
                REQUIRE(!clientDisconnectedSemaphore.tryAcquire());
                while (server.connectionCount() != 1)
                {
                    QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents, 1);
                }
                server.stop();
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStoppedSemaphore, 10));
            }
        }
    }

    GIVEN("a running encrypted server")
    {
        HttpServer server;
        TlsConfiguration serverTlsConfiguration;
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

        WHEN("a client tries to connect to server")
        {
            TlsConfiguration clientTlsConfiguration;
            clientTlsConfiguration.setCaCertificates({caCertificateFile});
            TlsSocket clientSocket(clientTlsConfiguration);
            QSemaphore clientConnectedSemaphore;
            Object::connect(&clientSocket, &TlsSocket::connected, [&](){clientConnectedSemaphore.release();});
            QSemaphore clientEncryptedSemaphore;
            Object::connect(&clientSocket, &TlsSocket::encrypted, [&](){clientEncryptedSemaphore.release();});
            QSemaphore clientDisconnectedSemaphore;
            Object::connect(&clientSocket, &TlsSocket::disconnected, [&](){clientDisconnectedSemaphore.release();});
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
                server.stop();
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientDisconnectedSemaphore, 10));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStoppedSemaphore, 10));
            }
        }
    }

    GIVEN("a running server and a connected client")
    {
        HttpServer server;
        REQUIRE(server.addRoute(HttpRequest::Method::GET, "/", [](const HttpRequest&, HttpBroker &broker){broker.writeResponse({"Hello World!"});}));
        REQUIRE(server.connectionCount() == 0);
        std::shared_ptr<CustomErrorHandler> pErrorHandler(new CustomErrorHandler);
        server.setErrorHandler(pErrorHandler);
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

        WHEN("client sends a request targeting an unmapped resource")
        {
            clientSocket.write("POST / HTTP/1.1\r\nHost: host\r\n\r\n");

            THEN("server sends a 404 Not Found response, and closes the connection")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(clientDisconnectedSemaphore, 10));
                REQUIRE(clientSocket.readAll().starts_with("HTTP/1.1 404 Not Found\r\n"));
                while(pErrorHandler->reportedErrors().size() != 1)
                {
                    QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents, 1);
                }
                server.stop();
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(serverStoppedSemaphore, 10));
                const auto reportedErrors = pErrorHandler->reportedErrors();
                REQUIRE(reportedErrors.size() == 1);
                REQUIRE(reportedErrors[0].error == HttpServer::ServerError::MalformedRequest);
                REQUIRE(reportedErrors[0].clientIp == clientIp);
                REQUIRE(reportedErrors[0].clientPort == clientPort);
            }
        }
    }
}
