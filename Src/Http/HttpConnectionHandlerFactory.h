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

#ifndef KOURIER_HTTP_CONNECTION_HANDLER_FACTORY_H
#define KOURIER_HTTP_CONNECTION_HANDLER_FACTORY_H

#include "HttpRequestLimits.h"
#include "HttpServerOptions.h"
#include "HttpRequestRouter.h"
#include "ErrorHandler.h"
#include "../Core/TlsConfiguration.h"
#include "../Core/TlsContext.h"
#include "../Server/ConnectionHandlerFactory.h"
#include <memory>


namespace Kourier
{

class HttpConnectionHandlerFactory : public ConnectionHandlerFactory
{
public:
    HttpConnectionHandlerFactory(const HttpServerOptions &httpServerOptions,
                                 const HttpRequestRouter &httpRequestRouter,
                                 const TlsConfiguration &tlsConfiguration,
                                 std::shared_ptr<ErrorHandler> pErrorHandler = {});
    ~HttpConnectionHandlerFactory() override = default;
    ConnectionHandler *create(qintptr socketDescriptor) override;

private:
    const HttpServerOptions m_httpServerOptions;
    const HttpRequestRouter m_httpRequestRouter;
    const TlsConfiguration m_tlsConfiguration;
    const TlsContext m_tlsContext;
    const std::shared_ptr<HttpRequestLimits> m_pHttpRequestLimits;
    const std::shared_ptr<HttpRequestRouter> m_pHttpRequestRouter;
    const std::shared_ptr<ErrorHandler> m_pErrorHandler;
    const int m_requestTimeoutInSecs;
    const int m_idleTimeoutInSecs;
    const bool m_isEncrypted;
};

}

#endif // KOURIER_HTTP_CONNECTION_HANDLER_FACTORY_H
