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

#ifndef KOURIER_HTTP_SERVER_WORKER_H
#define KOURIER_HTTP_SERVER_WORKER_H

#include "HttpConnectionHandlerFactory.h"
#include "ErrorHandler.h"
#include "../Core/TlsConfiguration.h"
#include "../Core/UnixSignalListener.h"
#include "../Server/ServerWorker.h"
#include "../Server/QTcpServerBasedConnectionListener.h"
#include "../Server/ConnectionHandlerRepository.h"

namespace Kourier
{

class HttpServerWorker : public ServerWorker
{
Q_OBJECT
public:
    HttpServerWorker(const HttpServerOptions &httpServerOptions,
                     const HttpRequestRouter &httpRequestRouter,
                     const TlsConfiguration &tlsConfiguration,
                     std::shared_ptr<ErrorHandler> pErrorHandler = {}) :
        ServerWorker(std::shared_ptr<ConnectionListener>(new QTcpServerBasedConnectionListener),
                     std::shared_ptr<ConnectionHandlerFactory>(new HttpConnectionHandlerFactory(httpServerOptions, httpRequestRouter, tlsConfiguration, pErrorHandler)),
                     std::shared_ptr<ConnectionHandlerRepository>(new ConnectionHandlerRepository))
    {
        UnixSignalListener::blockSignalProcessingForCurrentThread();
    }

    ~HttpServerWorker() override = default;

private:
    Q_DISABLE_COPY_MOVE(HttpServerWorker);
};

}

#endif // KOURIER_HTTP_SERVER_WORKER_H
