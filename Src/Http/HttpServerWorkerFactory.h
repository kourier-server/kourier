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

#ifndef KOURIER_HTTP_SERVER_WORKER_FACTORY_H
#define KOURIER_HTTP_SERVER_WORKER_FACTORY_H

#include "HttpServerOptions.h"
#include "HttpRequestRouter.h"
#include "ErrorHandler.h"
#include "../Core/TlsConfiguration.h"
#include "../Server/ServerWorkerFactory.h"
#include <QHostAddress>


namespace Kourier
{

class HttpServerWorkerFactory : public ServerWorkerFactory
{
public:
    HttpServerWorkerFactory(const HttpServerOptions &httpServerOptions,
                            const HttpRequestRouter &httpRequestRouter,
                            const TlsConfiguration &tlsConfiguration,
                            std::shared_ptr<ErrorHandler> pErrorHandler = {});
    ~HttpServerWorkerFactory() override = default;
    std::shared_ptr<ServerWorker> create() override;

private:
    const HttpServerOptions m_options;
    const HttpRequestRouter m_requestRouter;
    const TlsConfiguration m_tlsConfiguration;
    const std::shared_ptr<ErrorHandler> m_pErrorHandler;
    Q_DISABLE_COPY_MOVE(HttpServerWorkerFactory);
};

}

#endif // KOURIER_HTTP_SERVER_WORKER_FACTORY_H
