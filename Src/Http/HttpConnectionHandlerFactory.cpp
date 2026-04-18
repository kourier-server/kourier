//
// Copyright (C) 2024 Glauco Pacheco <glaucopacheco@gmail.com>
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
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "HttpConnectionHandlerFactory.h"
#include "HttpConnectionHandler.h"
#include "../Core/TlsSocket.h"
#include <chrono>


namespace Kourier
{

HttpConnectionHandlerFactory::HttpConnectionHandlerFactory(const HttpServerOptions &httpServerOptions,
    const HttpRequestRouter &httpRequestRouter,
    const TlsConfiguration &tlsConfiguration,
    std::shared_ptr<ErrorHandler> pErrorHandler) :
    m_httpServerOptions(httpServerOptions),
    m_pHttpRequestRouter(std::make_shared<HttpRequestRouter>(httpRequestRouter)),
    m_pErrorHandler(pErrorHandler),
    m_tlsConfiguration(tlsConfiguration),
    m_tlsContext(TlsContext::Role::Server, m_tlsConfiguration),
    m_pHttpRequestLimits(new HttpRequestLimits{.maxUrlSize = static_cast<size_t>(m_httpServerOptions.getOption(HttpServer::ServerOption::MaxUrlSize)),
                                               .maxHeaderNameSize = static_cast<size_t>(m_httpServerOptions.getOption(HttpServer::ServerOption::MaxHeaderNameSize)),
                                               .maxHeaderValueSize = static_cast<size_t>(m_httpServerOptions.getOption(HttpServer::ServerOption::MaxHeaderValueSize)),
                                               .maxHeaderLineCount = static_cast<size_t>(m_httpServerOptions.getOption(HttpServer::ServerOption::MaxHeaderLineCount)),
                                               .maxTrailerNameSize = static_cast<size_t>(m_httpServerOptions.getOption(HttpServer::ServerOption::MaxTrailerNameSize)),
                                               .maxTrailerValueSize = static_cast<size_t>(m_httpServerOptions.getOption(HttpServer::ServerOption::MaxTrailerValueSize)),
                                               .maxTrailerLineCount = static_cast<size_t>(m_httpServerOptions.getOption(HttpServer::ServerOption::MaxTrailerLineCount)),
                                               .maxChunkMetadataSize = static_cast<size_t>(m_httpServerOptions.getOption(HttpServer::ServerOption::MaxChunkMetadataSize)),
                                               .maxRequestSize = static_cast<size_t>(m_httpServerOptions.getOption(HttpServer::ServerOption::MaxRequestSize)),
                                               .maxBodySize = static_cast<size_t>(m_httpServerOptions.getOption(HttpServer::ServerOption::MaxBodySize))}),
    m_requestTimeoutInMSecs(static_cast<int>(m_httpServerOptions.getOption(HttpServer::ServerOption::RequestTimeoutInMSecs))),
    m_idleTimeoutInMSecs(static_cast<int>(m_httpServerOptions.getOption(HttpServer::ServerOption::IdleTimeoutInMSecs))),
    m_isEncrypted(m_tlsConfiguration != TlsConfiguration())

{
}

ConnectionHandler *HttpConnectionHandlerFactory::create(qintptr socketDescriptor)
{
    TcpSocket *pSocket = m_isEncrypted ? new TlsSocket(socketDescriptor, m_tlsConfiguration) : new TcpSocket(socketDescriptor);
    if (pSocket->state() != TcpSocket::State::Connected)
    {
        delete pSocket;
        return nullptr;
    }
    else
        return new HttpConnectionHandler(*pSocket,
                                         m_pHttpRequestLimits,
                                         m_pHttpRequestRouter,
                                         std::chrono::milliseconds(m_requestTimeoutInMSecs),
                                         std::chrono::milliseconds(m_idleTimeoutInMSecs),
                                         m_pErrorHandler);
}

}
