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

#include "HttpConnectionHandler.h"
#include "../Core/TcpSocket.h"


namespace Kourier
{

HttpConnectionHandler::HttpConnectionHandler(TcpSocket &socket,
                                             std::shared_ptr<HttpRequestLimits> pHttpRequestLimits,
                                             std::shared_ptr<HttpRequestRouter> pHttpRequestRouter,
                                             int requestTimeoutInSecs,
                                             int idleTimeoutInSecs,
                                             std::shared_ptr<ErrorHandler> pErrorHandler) :
    m_pSocket(&socket),
    m_requestTimeoutInMSecs(1000*requestTimeoutInSecs),
    m_idleTimeoutInMSecs(1000*idleTimeoutInSecs),
    m_requestParser(socket, pHttpRequestLimits),
    m_pHttpRequestRouter(pHttpRequestRouter),
    m_pErrorHandler(pErrorHandler),
    m_brokerPrivate(&socket, &m_requestParser),
    m_broker(&m_brokerPrivate)
{
    m_timer.setSingleShot(true);
    Object::connect(&m_timer, &Timer::timeout, this, &HttpConnectionHandler::onTimeout);
    Object::connect(m_pSocket.get(), &TcpSocket::receivedData, this, &HttpConnectionHandler::onReceivedData);
    if (m_idleTimeoutInMSecs > 0)
    {
        m_isInIdleTimeout = true;
        m_timer.start(m_idleTimeoutInMSecs);
    }
    Object::connect(m_pSocket.get(), &TcpSocket::disconnected, this, &HttpConnectionHandler::onDisconnected);
    Object::connect(m_pSocket.get(), &TcpSocket::error, this, &HttpConnectionHandler::onDisconnected);
    Object::connect(&m_brokerPrivate, &HttpBrokerPrivate::wroteResponse, this, &HttpConnectionHandler::onWroteResponse);
}

void HttpConnectionHandler::finish()
{
    m_pSocket->disconnectFromPeer();
}

void HttpConnectionHandler::reset()
{
    m_parsedRequestMetadata = false;
    m_receivedCompleteRequest = false;
    m_brokerPrivate.resetResponseWriting();
    if (m_pSocket->dataAvailable() > 0)
    {
        if (m_requestTimeoutInMSecs > 0)
            m_timer.start(m_requestTimeoutInMSecs);
        else
            m_timer.stop();
    }
    else
    {
        if (m_idleTimeoutInMSecs > 0)
        {
            m_isInIdleTimeout = true;
            m_timer.start(m_idleTimeoutInMSecs);
        }
        else
            m_timer.stop();
    }
}

void HttpConnectionHandler::onReceivedData()
{
    if (m_receivedCompleteRequest)
    {
        m_timer.stop();
        return;
    }
    if (m_isInIdleTimeout)
    {
        m_isInIdleTimeout = false;
        m_timer.stop();
    }
    if (!m_timer.isActive() && m_requestTimeoutInMSecs > 0)
        m_timer.start(m_requestTimeoutInMSecs);
    while (true)
    {
        switch (m_requestParser.parse())
        {
            case HttpRequestParser::ParserStatus::ParsedRequest:
                if (!m_parsedRequestMetadata)
                {
                    m_parsedRequestMetadata = true;
                    auto pHandler = m_pHttpRequestRouter->getHandler(m_requestParser.request().method(), m_requestParser.request().targetPath());
                    if (pHandler)
                    {
                        try
                        {
                            pHandler(m_requestParser.request(), m_broker);
                            m_receivedCompleteRequest = m_requestParser.request().isComplete();
                            if (!m_brokerPrivate.responded() && !m_brokerPrivate.hasQObject())
                            {
                                m_timer.stop();
                                Object::disconnect(&m_timer, &Timer::timeout, this, &HttpConnectionHandler::onTimeout);
                                Object::disconnect(m_pSocket.get(), &IOChannel::receivedData, this, &HttpConnectionHandler::onReceivedData);
                                m_pSocket->disconnectFromPeer();
                                return;
                            }
                        }
                        catch (...)
                        {
                            m_timer.stop();
                            Object::disconnect(&m_timer, &Timer::timeout, this, &HttpConnectionHandler::onTimeout);
                            Object::disconnect(m_pSocket.get(), &IOChannel::receivedData, this, &HttpConnectionHandler::onReceivedData);
                            m_brokerPrivate.writeResponse(HttpStatusCode::InternalServerError);
                            m_pSocket->disconnectFromPeer();
                            return;
                        }
                    }
                    else
                    {
                        m_timer.stop();
                        Object::disconnect(&m_timer, &Timer::timeout, this, &HttpConnectionHandler::onTimeout);
                        Object::disconnect(m_pSocket.get(), &IOChannel::receivedData, this, &HttpConnectionHandler::onReceivedData);
                        m_brokerPrivate.writeResponse(HttpStatusCode::NotFound);
                        if (m_pErrorHandler)
                            m_pErrorHandler->handleError(HttpServer::ServerError::MalformedRequest, m_pSocket->peerAddress(), m_pSocket->peerPort());
                        m_pSocket->disconnectFromPeer();
                        return;
                    }
                }
                else
                {
                    m_receivedCompleteRequest = true;
                    emit m_broker.receivedBodyData({}, true);
                }
                if (m_receivedCompleteRequest)
                {
                    if (m_brokerPrivate.responded())
                    {
                        reset();
                        continue;
                    }
                    else
                    {
                        m_timer.stop();
                        return;
                    }
                }
                else
                    continue;
            case HttpRequestParser::ParserStatus::ParsedBody:
                m_receivedCompleteRequest = (!m_requestParser.request().chunked() && m_requestParser.request().pendingBodySize() == 0);
                emit m_broker.receivedBodyData(m_requestParser.request().body(), m_receivedCompleteRequest);
                if (m_receivedCompleteRequest)
                {
                    if (m_brokerPrivate.responded())
                    {
                        reset();
                        continue;
                    }
                    else
                    {
                        m_timer.stop();
                        return;
                    }
                }
                else
                    continue;
            case HttpRequestParser::ParserStatus::NeedsMoreData:
                if (!m_parsedRequestMetadata && m_pSocket->dataAvailable() == 0)
                {
                    if (m_idleTimeoutInMSecs > 0)
                    {
                        m_isInIdleTimeout = true;
                        m_timer.start(m_idleTimeoutInMSecs);
                    }
                    else
                        m_timer.stop();
                }
                return;
            case HttpRequestParser::ParserStatus::Failed:
                m_timer.stop();
                Object::disconnect(&m_timer, &Timer::timeout, this, &HttpConnectionHandler::onTimeout);
                Object::disconnect(m_pSocket.get(), &IOChannel::receivedData, this, &HttpConnectionHandler::onReceivedData);
                m_brokerPrivate.writeResponse(HttpStatusCode::BadRequest);
                if (m_pErrorHandler)
                    m_pErrorHandler->handleError(m_requestParser.error(), m_pSocket->peerAddress(), m_pSocket->peerPort());
                m_pSocket->disconnectFromPeer();
                return;
        }
    }
}

void HttpConnectionHandler::onWroteResponse()
{
    if (m_receivedCompleteRequest)
    {
        reset();
        onReceivedData();
    }
}

void HttpConnectionHandler::onTimeout()
{
    if (m_brokerPrivate.responded())
        m_brokerPrivate.resetResponseWriting();
    m_brokerPrivate.writeResponse(HttpStatusCode::RequestTimeout);
    if (m_pErrorHandler)
        m_pErrorHandler->handleError(HttpServer::ServerError::RequestTimeout, m_pSocket->peerAddress(), m_pSocket->peerPort());
    Object::disconnect(&m_timer, &Timer::timeout, this, &HttpConnectionHandler::onTimeout);
    Object::disconnect(m_pSocket.get(), &IOChannel::receivedData, this, &HttpConnectionHandler::onReceivedData);
    m_pSocket->disconnectFromPeer();
    return;
}

void HttpConnectionHandler::onDisconnected()
{
    finished(this);
}

}
