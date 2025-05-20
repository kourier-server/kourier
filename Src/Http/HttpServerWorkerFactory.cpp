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

#include "HttpServerWorkerFactory.h"
#include "HttpServerWorker.h"
#include "../Server/AsyncServerWorker.h"


namespace Kourier
{

HttpServerWorkerFactory::HttpServerWorkerFactory(const HttpServerOptions &httpServerOptions,
                                                 const HttpRequestRouter &httpRequestRouter,
                                                 const TlsConfiguration &tlsConfiguration,
                                                 std::shared_ptr<ErrorHandler> pErrorHandler) :
    m_options(httpServerOptions),
    m_requestRouter(httpRequestRouter),
    m_tlsConfiguration(tlsConfiguration),
    m_pErrorHandler(pErrorHandler)
{
}

std::shared_ptr<ServerWorker> HttpServerWorkerFactory::create()
{
    return std::shared_ptr<ServerWorker>(new AsyncServerWorker<HttpServerWorker,
                                                               const HttpServerOptions &,
                                                               const HttpRequestRouter &,
                                                               const TlsConfiguration &,
                                                               std::shared_ptr<ErrorHandler>>(m_options, m_requestRouter, m_tlsConfiguration, m_pErrorHandler));
}

}
