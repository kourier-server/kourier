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

#ifndef KOURIER_HTTP_CONNECTION_HANDLER_H
#define KOURIER_HTTP_CONNECTION_HANDLER_H

#include "HttpRequestLimits.h"
#include "HttpRequestParser.h"
#include "HttpRequestRouter.h"
#include "HttpBrokerPrivate.h"
#include "HttpBroker.h"
#include "ErrorHandler.h"
#include "../Core/TcpSocket.h"
#include "../Core/Timer.h"
#include "../Server/ConnectionHandler.h"
#include <QObject>
#include <memory>


namespace Kourier
{

class HttpConnectionHandler : public ConnectionHandler
{
KOURIER_OBJECT(Kourier::HttpConnectionHandler)
public:
    HttpConnectionHandler(TcpSocket &socket,
                          std::shared_ptr<HttpRequestLimits> pHttpRequestLimits,
                          std::shared_ptr<HttpRequestRouter> pHttpRequestRouter,
                          int requestTimeoutInSecs,
                          int idleTimeoutInSecs,
                          std::shared_ptr<ErrorHandler> pErrorHandler = {});
    HttpConnectionHandler(HttpConnectionHandler&) = delete;
    HttpConnectionHandler &operator=(HttpConnectionHandler&) = delete;
    ~HttpConnectionHandler() override = default;
    void finish() override;

private:
    void reset();
    void onReceivedData();
    void onWroteResponse();
    void onTimeout();
    void onDisconnected();

private:
    Timer m_timer;
    std::unique_ptr<TcpSocket> m_pSocket;
    const qint64 m_requestTimeoutInMSecs;
    const qint64 m_idleTimeoutInMSecs;
    HttpRequestParser m_requestParser;
    HttpBrokerPrivate m_brokerPrivate;
    HttpBroker m_broker;
    std::shared_ptr<HttpRequestRouter> m_pHttpRequestRouter;
    std::shared_ptr<ErrorHandler> m_pErrorHandler;
    bool m_parsedRequestMetadata = false;
    bool m_receivedCompleteRequest = false;
    bool m_isInIdleTimeout = false;
};

}

#endif // KOURIER_HTTP_CONNECTION_HANDLER_H
