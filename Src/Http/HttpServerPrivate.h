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

#ifndef KOURIER_HTTP_SERVER_PRIVATE_H
#define KOURIER_HTTP_SERVER_PRIVATE_H

#include "HttpServer.h"
#include "HttpServerOptions.h"
#include "HttpRequestRouter.h"
#include "ErrorHandler.h"
#include "../Server/Server.h"
#include <QObject>
#include <QPointer>
#include <QVariant>
#include <atomic>
#include <memory>


namespace Kourier
{
class HttpServer;

class HttpServerPrivate : public QObject
{
Q_OBJECT
public:
    HttpServerPrivate(HttpServer *pServer);
    ~HttpServerPrivate() override = default;
    bool isRunning() const;
    bool addRoute(HttpRequest::Method method, std::string_view path, void(*pFcn)(const HttpRequest&, HttpBroker&));
    bool setOption(HttpServer::ServerOption option, int64_t value);
    int64_t getOption(HttpServer::ServerOption option) const;
    void start(QHostAddress address, quint16 port);
    void stop();
    std::string_view errorMessage() const {return m_errorMessage;}
    bool setTlsConfiguration(const TlsConfiguration &tlsConfiguration);
    void setErrorHandler(std::shared_ptr<ErrorHandler> pErrorHandler);
    QHostAddress serverAddress() const {return m_serverAddress;}
    quint16 serverPort() const {return m_serverPort;}
    size_t connectionCount() const {return m_connectionCount->load();}

private:
    void setError(std::string_view errorMessage);
    void onServerStarted();
    void onServerStopped();
    void onServerFailed(std::string_view errorMessage);
    QVariant generateServerData();

private:
    QPointer<HttpServer> q_ptr;
    std::shared_ptr<std::atomic_size_t> m_connectionCount;
    HttpServerOptions m_options;
    HttpRequestRouter m_requestRouter;
    std::shared_ptr<ErrorHandler> m_pErrorHandler;
    std::string m_errorMessage;
    std::unique_ptr<Server> m_pServer;
    TlsConfiguration m_tlsConfiguration;
    QHostAddress m_serverAddress;
    quint16 m_serverPort = 0;
    Q_DISABLE_COPY_MOVE(HttpServerPrivate)
};

}

#endif // KOURIER_HTTP_SERVER_PRIVATE_H
