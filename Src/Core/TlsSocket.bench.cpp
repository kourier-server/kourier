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


namespace TlsSocketBenchmarks
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

using namespace TlsSocketBenchmarks;


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


namespace TlsSocketBenchmarks
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

#include "TlsSocket.bench.moc"
