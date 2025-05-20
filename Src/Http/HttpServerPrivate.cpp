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

#include "HttpServerPrivate.h"
#include "HttpServerWorkerFactory.h"
#include "../Core/TlsContext.h"
#include <QTcpSocket>
#include <QVariantMap>
#include <QThread>
#include <memory>
#include <atomic>


namespace Kourier
{

HttpServerPrivate::HttpServerPrivate(HttpServer *pServer) :
    q_ptr(pServer),
    m_connectionCount(new std::atomic_size_t(0))
{
    assert(q_ptr);
}

bool HttpServerPrivate::isRunning() const
{
    return (m_pServer && m_pServer->state() != ExecutionState::Stopped);
}

bool HttpServerPrivate::addRoute(HttpRequest::Method method, std::string_view path, void (*pFcn)(const HttpRequest&, HttpBroker&))
{
    if (m_requestRouter.addRoute(method, path, pFcn))
        return true;
    else
    {
        m_errorMessage = m_requestRouter.errorMessage();
        return false;
    }
}

bool HttpServerPrivate::setOption(HttpServer::ServerOption option, int64_t value)
{
    if (m_options.setOption(option, value))
        return true;
    else
    {
        m_errorMessage = m_options.errorMessage();
        return false;
    }
}

int64_t HttpServerPrivate::getOption(HttpServer::ServerOption option) const
{
    return m_options.getOption(option);
}

void HttpServerPrivate::start(QHostAddress address, quint16 port)
{
    if (m_pServer)
    {
        setError("Failed to start server. Server is not stopped.");
        return;
    }
    else if (address.isNull() || address.toString().isEmpty())
    {
        setError("Failed to start server. Given address is null.");
        return;
    }
    else if (port == 0)
    {
        QTcpSocket socket;
        if (!socket.bind(address, port))
        {
            setError("Failed to start server. Failed to fetch available port.");
            return;
        }
        port = socket.localPort();
        socket.abort();
    }
    m_serverAddress = address;
    m_serverPort = port;
    m_pServer.reset(new Server(std::shared_ptr<ServerWorkerFactory>(new HttpServerWorkerFactory(m_options, m_requestRouter, m_tlsConfiguration, m_pErrorHandler))));
    m_pServer->setWorkerCount(m_options.getOption(HttpServer::ServerOption::WorkerCount));
    QObject::connect(m_pServer.get(), &Server::started, this, &HttpServerPrivate::onServerStarted);
    QObject::connect(m_pServer.get(), &Server::stopped, this, &HttpServerPrivate::onServerStopped);
    QObject::connect(m_pServer.get(), &Server::failed, this, &HttpServerPrivate::onServerFailed);
    m_pServer->start(generateServerData());
}

void HttpServerPrivate::stop()
{
    if (m_pServer)
        m_pServer->stop();
}

bool HttpServerPrivate::setTlsConfiguration(const TlsConfiguration &tlsConfiguration)
{
    auto response = TlsContext::validateTlsConfiguration(tlsConfiguration, TlsContext::Role::Server);
    if (response.first)
        m_tlsConfiguration = tlsConfiguration;
    else
        m_errorMessage = response.second;
    return response.first;
}

void Kourier::HttpServerPrivate::setErrorHandler(std::shared_ptr<ErrorHandler> pErrorHandler)
{
    m_pErrorHandler = pErrorHandler;
}

void HttpServerPrivate::setError(std::string_view errorMessage)
{
    m_errorMessage = errorMessage;
    if (q_ptr)
        emit q_ptr->failed();
}

void HttpServerPrivate::onServerStarted()
{
    if (q_ptr)
        emit q_ptr->started();
}

void HttpServerPrivate::onServerStopped()
{
    m_serverAddress = {};
    m_serverPort = 0;
    m_pServer->disconnect(this);
    m_pServer.release()->deleteLater();
    *m_connectionCount = 0;
    if (q_ptr)
        emit q_ptr->stopped();
}

void HttpServerPrivate::onServerFailed(std::string_view errorMessage)
{
    m_serverAddress = {};
    m_serverPort = 0;
    m_pServer->disconnect(this);
    m_pServer.release()->deleteLater();
    *m_connectionCount = 0;
    setError(errorMessage);
}

QVariant HttpServerPrivate::generateServerData()
{
    QVariantMap dataMap;
    // ConnectionListener
    // =============================
    // QByteArray: address
    // ushort: port
    // qintptr: socketDescriptor
    // int: backlogSize
    dataMap["address"] = QVariant::fromValue(m_serverAddress.toString().toLatin1());
    dataMap["port"] = QVariant::fromValue(m_serverPort);
    dataMap["backlogSize"] = QVariant::fromValue<int>(m_options.getOption(HttpServer::ServerOption::TcpServerBacklogSize));
    // ServerWorker
    // =============================
    // std::shared_ptr<std::atomic_size_t>: connectionCount
    // size_t: maxConnectionCount
    dataMap["connectionCount"] = QVariant::fromValue(m_connectionCount);
    dataMap["maxConnectionCount"] = QVariant::fromValue<size_t>(m_options.getOption(HttpServer::ServerOption::MaxConnectionCount));
    return QVariant(dataMap);
}

}
