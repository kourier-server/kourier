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

#ifndef KOURIER_HTTP_SERVER_H
#define KOURIER_HTTP_SERVER_H

#include "HttpRequest.h"
#include "HttpBroker.h"
#include "../Core/TlsConfiguration.h"
#include <QHostAddress>
#include <QObject>
#include <memory>


namespace Kourier
{
class HttpServerPrivate;
class ErrorHandler;

class KOURIER_EXPORT HttpServer : public QObject
{
Q_OBJECT
public:
    HttpServer();
    ~HttpServer() override;
    bool isRunning() const;
    bool addRoute(HttpRequest::Method method, std::string_view path, void(*pFcn)(const HttpRequest&, HttpBroker&));
    enum class ServerOption
    {
        WorkerCount,
        TcpServerBacklogSize,
        IdleTimeoutInSecs,
        RequestTimeoutInSecs,
        MaxUrlSize,
        MaxHeaderNameSize,
        MaxHeaderValueSize,
        MaxHeaderLineCount,
        MaxTrailerNameSize,
        MaxTrailerValueSize,
        MaxTrailerLineCount,
        MaxChunkMetadataSize,
        MaxRequestSize,
        MaxBodySize,
        MaxConnectionCount
    };
    bool setServerOption(ServerOption option, int64_t value);
    int64_t serverOption(ServerOption option) const;
    enum class ServerError
    {
        NoError,
        MalformedRequest,
        TooBigRequest,
        RequestTimeout
    };
    void setErrorHandler(std::shared_ptr<ErrorHandler> pErrorHandler);
    std::string_view errorMessage() const;
    bool setTlsConfiguration(const TlsConfiguration &tlsConfiguration);
    QHostAddress serverAddress() const;
    quint16 serverPort() const;
    size_t connectionCount() const;

public Q_SLOTS:
    void start(QHostAddress address, quint16 port);
    void stop();

Q_SIGNALS:
    void started();
    void stopped();
    void failed();

private:
    HttpServerPrivate *d_ptr;
    Q_DECLARE_PRIVATE(HttpServer)
    Q_DISABLE_COPY_MOVE(HttpServer)
};

}

#endif // KOURIER_HTTP_SERVER_H
