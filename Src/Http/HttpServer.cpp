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
#include "HttpServerPrivate.h"
#include "ErrorHandler.h"


namespace Kourier
{

/*!
\class Kourier::HttpServer
\brief The HttpServer class provides the fastest HTTP server ever built.

HttpServer streamlines the creation of REST-based services. With Kourier, you only have to
[configure](@ref ConfiguringServer) and [add handlers](@ref AddingHandlers) to an HttpServer
instance before [starting](@ref Kourier::HttpServer::start) it to create a REST service.

HttpServer emits the [started](@ref Kourier::HttpServer::started) signal when all workers initialize successfully.
Otherwise, if any worker fails while the server starts, HttpServer stops all workers and emits the [failed](@ref Kourier::HttpServer::failed)
signal after the last running worker stops. You can call [errorMessage](@ref Kourier::HttpServer::errorMessage)
to get a description of the last error that occurred.

You can call [stop](@ref Kourier::HttpServer::stop) to stop a running server. HttpServer emits the
[stopped](@ref Kourier::HttpServer::stopped) signal after the last running worker stops.

To create a reliable service, you must be able to act whenever an error occurs. HttpServer provides the
[setErrorHandler](@ref Kourier::HttpServer::setErrorHandler) method that you can use to set an error handler that
HttpServer calls when an error occurs. HttpServer uses the set error handler to report what prevented it from
calling a mapped handler. You can use the HttpServer::ServerError argument that HttpServer passes to the error
handler to know why the server failed to call a mapped handler.
*/

/*!
 \enum HttpServer::ServerOption
 \brief This enum describes the available server options that you can configure. See [Configuring Server](@ref ConfiguringServer) for further details.
 \var HttpServer::ServerOption::WorkerCount
 \brief The number of workers the server should use to handle incoming requests. By default, HttpServer uses as many workers as available cores.
 \var HttpServer::ServerOption::TcpServerBacklogSize
 \brief The size of the backlog used to keep accepted connections.
 \var HttpServer::ServerOption::IdleTimeoutInSecs
 \brief The number of seconds a connection can idle before the server closes it.
 \var HttpServer::ServerOption::RequestTimeoutInSecs
 \brief The number of seconds the server can wait until the request is fully received.
 \var HttpServer::ServerOption::MaxUrlSize
 \brief Maximum size of request URL.
 \var HttpServer::ServerOption::MaxHeaderNameSize
 \brief Maximum size of header name.
 \var HttpServer::ServerOption::MaxHeaderValueSize
 \brief Maximum size of header value.
 \var HttpServer::ServerOption::MaxHeaderLineCount
 \brief Maximum number of field lines in the header block.
 \var HttpServer::ServerOption::MaxTrailerNameSize
 \brief Maximum size of trailer name.
 \var HttpServer::ServerOption::MaxTrailerValueSize
 \brief Maximum size of trailer value.
 \var HttpServer::ServerOption::MaxTrailerLineCount
 \brief Maximum number of field lines in the trailer section.
 \var HttpServer::ServerOption::MaxChunkMetadataSize
 \brief Maximum size of the chunk metadata.
 \var HttpServer::ServerOption::MaxRequestSize
 \brief Maximum request size.
 \var HttpServer::ServerOption::MaxBodySize
 \brief Maximum request body size.
 \var HttpServer::ServerOption::MaxConnectionCount
 \brief Maximum number of connections the server can keep.
*/

/*!
 \enum HttpServer::ServerError
 \brief This enum describes the server error that can occur while parsing an HTTP request.
 \var HttpServer::ServerError::NoError
 \brief No error has happened.
 \var HttpServer::ServerError::MalformedRequest
 \brief An invalid HTTP request or a valid request with no handler mapped to its method/path.
 \var HttpServer::ServerError::TooBigRequest
 \brief The request is larger than the server is allowed to support.
 \var HttpServer::ServerError::RequestTimeout
 \brief Either the request was not parsed within [RequestTimeoutInSecs](@ref Kourier::HttpServer::ServerOption::RequestTimeoutInSecs)
 or, after processing a request, no bytes from the next request were received within [IdleTimeoutInSecs](@ref Kourier::HttpServer::ServerOption::IdleTimeoutInSecs).
*/

/*!
 \fn HttpServer::HttpServer()
 Creates a HttpServer. You can call [addRoute](@ref Kourier::HttpServer::addRoute) to map paths to request handlers and
 [setServerOption](@ref Kourier::HttpServer::setServerOption) to configure the server.
*/

/*!
 \fn HttpServer::addRoute(HttpRequest::Method method, std::string_view path, void (*pFcn)(const HttpRequest&, HttpBroker&))
Maps the handler identified by \a pFcn to the given \a path for requests containing the given \a method.
HttpServer always picks the most specific path for handling a given request. You can call HttpBroker::setQObject on the HttpBroker
object that HttpServer passes to the handler function to postpone responding until after the handler finishes.
HttpServer monitors responses through the given broker, and after you write a complete response for the current request,
HttpServer destroys any QObject you set on the broker and processes the next request. See
[Adding Handlers](@ref AddingHandlers) for more details.
*/

/*!
 \fn HttpServer::setServerOption(ServerOption option, int64_t value)
 Sets the \a value for the given [option](@ref Kourier::HttpServer::ServerOption).
 See [Configuring Server](@ref ConfiguringServer) for more details.
*/

/*!
 \fn HttpServer::serverOption(ServerOption option) const
 Returns the set \a value for the given [option](@ref Kourier::HttpServer::ServerOption). See
 [Configuring Server](@ref ConfiguringServer) for more details.
*/

/*!
 \fn HttpServer::setErrorHandler(std::shared_ptr<ErrorHandler> pErrorHandler)
 Sets \a pErrorHandler as the error handler. HttpServer does not serialize access to the given error handler.
*/

/*!
 \fn HttpServer::errorMessage()
 Returns a textual description for the last error that occurred. If no error has occurred, HttpServer returns an empty string view.
*/

/*!
 \fn HttpServer::setTlsConfiguration(const TlsConfiguration &tlsConfiguration)
 Makes HttpServer encrypt connections according to the given \a tlsConfiguration.
*/

/*!
 \fn HttpServer::start(const QHostAddress &address, quint16 port)
 Starts HttpServer. HttpServer creates as many workers as set in the [worker count](@ref Kourier::HttpServer::ServerOption::WorkerCount)
 option value and makes them listen to the given \a address and \a port. HttpServer emits [started](@ref Kourier::HttpServer::started)
 when all workers start, or emits [failed](@ref Kourier::HttpServer::failed) if any error occurs. If the server fails to start, you
 can call [errorMessage](@ref Kourier::HttpServer::errorMessage) to get a textual description of the last error that occurred.
*/

/*!
 \fn HttpServer::stop()
 Stops HttpServer. HttpServer emits [stopped](@ref Kourier::HttpServer::stopped) when all workers stop.
*/

/*!
 \fn HttpServer::started()
 HttpServer emits this signal when all workers finish starting.
*/

/*!
 \fn HttpServer::stopped()
 HttpServer emits this signal when all workers stop.
*/

/*!
 \fn HttpServer::failed()
 HttpServer emits this signal if any worker fails to start.
*/

HttpServer::HttpServer() :
    d_ptr(new HttpServerPrivate(this))
{
}

HttpServer::~HttpServer()
{
    d_ptr->deleteLater();
}

bool HttpServer::isRunning() const
{
    Q_D(const HttpServer);
    return d->isRunning();
}

bool HttpServer::addRoute(HttpRequest::Method method, std::string_view path, void (*pFcn)(const HttpRequest&, HttpBroker&))
{
    Q_D(HttpServer);
    return d->addRoute(method, path, pFcn);
}

bool HttpServer::setServerOption(ServerOption option, int64_t value)
{
    Q_D(HttpServer);
    return d->setOption(option, value);
}

int64_t HttpServer::serverOption(ServerOption option) const
{
    Q_D(const HttpServer);
    return d->getOption(option);
}

std::string_view HttpServer::errorMessage() const
{
    Q_D(const HttpServer);
    return d->errorMessage();
}

bool HttpServer::setTlsConfiguration(const TlsConfiguration &tlsConfiguration)
{
    Q_D(HttpServer);
    return d->setTlsConfiguration(tlsConfiguration);
}

QHostAddress HttpServer::serverAddress() const
{
    Q_D(const HttpServer);
    return d->serverAddress();
}

quint16 HttpServer::serverPort() const
{
    Q_D(const HttpServer);
    return d->serverPort();
}

size_t HttpServer::connectionCount() const
{
    Q_D(const HttpServer);
    return d->connectionCount();
}

void HttpServer::start(QHostAddress address, quint16 port)
{
    Q_D(HttpServer);
    d->start(address, port);
}

void HttpServer::stop()
{
    Q_D(HttpServer);
    d->stop();
}

void Kourier::HttpServer::setErrorHandler(std::shared_ptr<ErrorHandler> pErrorHandler)
{
    Q_D(HttpServer);
    d->setErrorHandler(pErrorHandler);
}

}
