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
#include "HttpRequestRouter.h"
#include "HttpRequest.h"
#include "HttpBroker.h"
#include "HttpRequestLimits.h"
#include "../Core/TcpSocket.h"
#include <Spectator.h>
#include <QSemaphore>
#include <QElapsedTimer>
#include <utility>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>


using Kourier::HttpConnectionHandler;
using Kourier::ConnectionHandler;
using Kourier::HttpRequestRouter;
using Kourier::HttpRequest;
using Kourier::HttpRequestLimits;
using Kourier::HttpBroker;
using Kourier::HttpServer;
using Kourier::ErrorHandler;
using Kourier::TcpSocket;
using Kourier::Object;
using Spectator::SemaphoreAwaiter;

static std::pair<TcpSocket*, TcpSocket*> createConnectedSocketPair()
{
    auto listeningFd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    REQUIRE(listeningFd >= 0);
    struct sockaddr_storage addr;
    std::memset(&addr, 0, sizeof(addr));
    struct sockaddr_in  *addr4 = (struct sockaddr_in *) &addr;
    addr4->sin_family = AF_INET;
    addr4->sin_addr.s_addr = ::inet_addr("127.0.0.1");
    addr4->sin_port = 0;
    REQUIRE(::bind(listeningFd, (struct sockaddr *) &addr, sizeof(addr)) == 0);
    REQUIRE(::listen(listeningFd, 4) == 0);
    std::memset(&addr, 0, sizeof(addr));
    socklen_t len = sizeof(addr);
    REQUIRE(::getsockname(listeningFd, reinterpret_cast<struct sockaddr*>(&addr), &len) == 0);
    const auto serverPort = ::ntohs(addr4->sin_port);
    REQUIRE(serverPort > 0);
    auto clientFd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    REQUIRE(clientFd >= 0);
    std::memset(&addr, 0, sizeof(addr));
    addr4->sin_family = AF_INET;
    addr4->sin_addr.s_addr = ::inet_addr("127.0.0.1");
    addr4->sin_port = ::htons(serverPort);
    int result = 0;
    do
    {
        result = ::connect(clientFd, (sockaddr*)&addr, sizeof(addr));
    } while (-1 == result && EINTR == errno);
    REQUIRE(result == 0 || EINPROGRESS == errno);
    std::memset(&addr, 0, sizeof(addr));
    len = sizeof(addr);
    auto serverFd = ::accept(listeningFd, (sockaddr*)&addr, &len);
    REQUIRE(serverFd >= 0);
    ::close(listeningFd);
    return std::make_pair(new TcpSocket(clientFd), new TcpSocket(serverFd));
}


SCENARIO("HttpConnectionHandler calls handler mapped to request path")
{
    GIVEN("a connected client")
    {
        auto tcpSockets = createConnectedSocketPair();
        std::unique_ptr<TcpSocket> clientSocket(tcpSockets.first);
        REQUIRE(clientSocket->state() == TcpSocket::State::Connected);
        auto pHttpRequestRouter = std::make_shared<HttpRequestRouter>();
        static thread_local std::string calledRoute;
        static thread_local QSemaphore handlerSemaphore;
        const auto method = GENERATE(AS(HttpRequest::Method),
                                     HttpRequest::Method::GET,
                                     HttpRequest::Method::POST);
        REQUIRE(pHttpRequestRouter->addRoute(method, "/a", [](const HttpRequest &, HttpBroker&){calledRoute = "/a"; handlerSemaphore.release();}));
        REQUIRE(pHttpRequestRouter->addRoute(method, "/a/path", [](const HttpRequest &, HttpBroker&){calledRoute = "/a/path"; handlerSemaphore.release();}));
        REQUIRE(pHttpRequestRouter->addRoute(method, "/another/path", [](const HttpRequest &, HttpBroker&){calledRoute = "/another/path"; handlerSemaphore.release();}));
        HttpConnectionHandler httpConnectionHandler(*tcpSockets.second, std::make_shared<HttpRequestLimits>(), pHttpRequestRouter, 0, 0);

        WHEN("client sends a request")
        {
            const auto requestTarget = GENERATE(AS(std::string_view), "/a", "/a/path", "/another/path");
            std::string requestData;
            requestData.reserve(128);
            std::string_view methodAsString = [](HttpRequest::Method method)
            {
                switch(method)
                {
                    case Kourier::HttpRequest::Method::GET:
                        return "GET";
                    case Kourier::HttpRequest::Method::PUT:
                        return "PUT";
                    case Kourier::HttpRequest::Method::POST:
                        return "POST";
                    case Kourier::HttpRequest::Method::PATCH:
                        return "PATCH";
                    case Kourier::HttpRequest::Method::DELETE:
                        return "DELETE";
                    case Kourier::HttpRequest::Method::HEAD:
                        return "HEAD";
                    case Kourier::HttpRequest::Method::OPTIONS:
                        return "OPTIONS";
                }
            }(method);
            requestData.append(methodAsString).append(" ").append(requestTarget).append(" HTTP/1.1\r\nHost: example.com\r\n\r\n");
            clientSocket->write(requestData);

            THEN("request handler function mapped to path gets called")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(handlerSemaphore, 1));
                REQUIRE(calledRoute == requestTarget);
                for (auto i = 0; i < 5; ++i)
                {
                    QCoreApplication::processEvents();
                    REQUIRE(!handlerSemaphore.tryAcquire());
                }
            }
        }
    }
}


SCENARIO("HttpConnectionHandler sends pending request data through broker")
{
    GIVEN("a connected client")
    {
        auto tcpSockets = createConnectedSocketPair();
        std::unique_ptr<TcpSocket> clientSocket(tcpSockets.first);
        REQUIRE(clientSocket->state() == TcpSocket::State::Connected);
        auto pHttpRequestRouter = std::make_shared<HttpRequestRouter>();
        static thread_local std::string body;
        static thread_local QSemaphore handlerSemaphore;
        static thread_local QSemaphore bodySemaphore;
        static thread_local QSemaphore requestSemaphore;
        REQUIRE(pHttpRequestRouter->addRoute(HttpRequest::Method::POST, "/data", [](const HttpRequest &request, HttpBroker &broker)
        {
            REQUIRE(!request.isComplete());
            body = request.body();
            broker.setQObject(new QObject);
            QObject::connect(&broker, &HttpBroker::receivedBodyData, [&](std::string_view bodyPart, bool isLastPart)
            {
                body.append(bodyPart);
                bodySemaphore.release();
                if (isLastPart)
                    requestSemaphore.release();
            });
            handlerSemaphore.release();
        }));
        HttpConnectionHandler httpConnectionHandler(*tcpSockets.second, std::make_shared<HttpRequestLimits>(), pHttpRequestRouter, 0, 0);

        WHEN("client sends a partial request containing at least the request metadata (request line + headers)")
        {
            std::string_view request("POST /data HTTP/1.1\r\nHost: host\r\nContent-Length: 12\r\n\r\nHello World!");
            std::string_view requestBody = request.substr(request.size() - 12, 12);
            REQUIRE(requestBody == "Hello World!");
            const auto bytesPending = GENERATE(AS(size_t), 1, 5, 12);
            clientSocket->write(request.substr(0, request.size() - bytesPending));

            THEN("request handler function mapped to path gets called")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(handlerSemaphore, 1));
                QCoreApplication::processEvents();
                REQUIRE(!handlerSemaphore.tryAcquire());
                REQUIRE(!bodySemaphore.tryAcquire());
                REQUIRE(!requestSemaphore.tryAcquire());
                if (bytesPending == requestBody.size())
                {
                    REQUIRE(body.empty());
                }
                else
                {
                    REQUIRE(body == requestBody.substr(0, requestBody.size() - bytesPending));
                }

                AND_WHEN("rest of request body is sent")
                {
                    clientSocket->write(request.substr(request.size() - bytesPending, bytesPending));

                    THEN("remaining request data is sent through broker")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(requestSemaphore, 1));
                        REQUIRE(bodySemaphore.tryAcquire());
                        QCoreApplication::processEvents();
                        REQUIRE(!handlerSemaphore.tryAcquire());
                        REQUIRE(!bodySemaphore.tryAcquire());
                        REQUIRE(!requestSemaphore.tryAcquire());
                        REQUIRE(body == requestBody);
                    }
                }

                AND_WHEN("rest of request body is sent byte by byte")
                {
                    auto remainingRequestBodyToSend = request.substr(request.size() - bytesPending, bytesPending);

                    THEN("remaining request data is sent through broker byte by byte")
                    {
                        size_t counter = 0;
                        for (const char &ch : remainingRequestBodyToSend)
                        {
                            ++counter;
                            clientSocket->write(&ch, 1);
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(bodySemaphore, 1));
                            QCoreApplication::processEvents();
                            REQUIRE(!handlerSemaphore.tryAcquire());
                            REQUIRE(!bodySemaphore.tryAcquire());
                            REQUIRE(requestSemaphore.tryAcquire() == (counter == bytesPending));
                            REQUIRE(body == requestBody.substr(0, requestBody.size() - bytesPending + counter));
                        }
                    }
                }
            }
        }
    }
}


SCENARIO("HttpConnectionHandler processes next request after response for current request is fully written")
{
    GIVEN("a connected client")
    {
        auto tcpSockets = createConnectedSocketPair();
        std::unique_ptr<TcpSocket> clientSocket(tcpSockets.first);
        REQUIRE(clientSocket->state() == TcpSocket::State::Connected);
        auto pHttpRequestRouter = std::make_shared<HttpRequestRouter>();
        static thread_local std::string path;
        static thread_local QSemaphore handlerSemaphore;
        static thread_local HttpBroker *pBroker = nullptr;
        static constexpr auto pHandlerFcn = [](const HttpRequest &request, HttpBroker &broker)
        {
            REQUIRE(request.isComplete());
            path = request.targetPath();
            pBroker = &broker;
            broker.setQObject(new QObject);
            handlerSemaphore.release();
        };
        REQUIRE(pHttpRequestRouter->addRoute(HttpRequest::Method::GET, "/path1", pHandlerFcn));
        REQUIRE(pHttpRequestRouter->addRoute(HttpRequest::Method::POST, "/path2", pHandlerFcn));
        HttpConnectionHandler httpConnectionHandler(*tcpSockets.second, std::make_shared<HttpRequestLimits>(), pHttpRequestRouter, 0, 0);

        WHEN("client writes two complete requests")
        {
            std::string_view requests("GET /path1 HTTP/1.1\r\nHost: host.com\r\n\r\n"
                                      "POST /path2 HTTP/1.1\r\nHost: host.com\r\n\r\n");
            clientSocket->write(requests);

            THEN("handler for second request is called after code handling the first request writes a full response")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(handlerSemaphore, 1));
                QCoreApplication::processEvents();
                REQUIRE(!handlerSemaphore.tryAcquire());
                REQUIRE(path == "/path1");
                pBroker->writeResponse();
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(handlerSemaphore, 1));
                QCoreApplication::processEvents();
                REQUIRE(!handlerSemaphore.tryAcquire());
                REQUIRE(path == "/path2");
            }
        }
    }
}


SCENARIO("HttpConnectionHandler allows request handling code to respond before request is fully received")
{
    GIVEN("a connected client")
    {
        auto tcpSockets = createConnectedSocketPair();
        std::unique_ptr<TcpSocket> clientSocket(tcpSockets.first);
        REQUIRE(clientSocket->state() == TcpSocket::State::Connected);
        auto pHttpRequestRouter = std::make_shared<HttpRequestRouter>();
        static thread_local std::string path;
        static thread_local QSemaphore handlerSemaphore;
        static thread_local QSemaphore responseSemaphore;
        static thread_local HttpBroker *pBroker = nullptr;
        enum class ResponseType {InHandler, PartialInHandler, OutsideHandler};
        const auto currentResponseType = GENERATE(AS(ResponseType),
                                                  ResponseType::InHandler,
                                                  ResponseType::PartialInHandler,
                                                  ResponseType::OutsideHandler);
        static thread_local ResponseType responseType = currentResponseType;
        static constexpr auto pHandlerFcn = [](const HttpRequest &request, HttpBroker &broker)
        {
            switch (responseType)
            {
                case ResponseType::InHandler:
                    broker.writeResponse();
                    break;
                case ResponseType::PartialInHandler:
                    broker.writeChunkedResponse();
                    break;
                case ResponseType::OutsideHandler:
                    break;
            }
            QObject::connect(&broker, &HttpBroker::sentData, [&]()
            {
                if (broker.bytesToSend() == 0)
                    responseSemaphore.release();
            });
            pBroker = &broker;
            broker.setQObject(new QObject);
            handlerSemaphore.release();
        };
        REQUIRE(pHttpRequestRouter->addRoute(HttpRequest::Method::POST, "/", pHandlerFcn));
        REQUIRE(pHttpRequestRouter->addRoute(HttpRequest::Method::GET, "/another", pHandlerFcn));
        HttpConnectionHandler httpConnectionHandler(*tcpSockets.second, std::make_shared<HttpRequestLimits>(), pHttpRequestRouter, 0, 0);

        WHEN("client writes part of the first request containing at least the request metadata (request line + headers)")
        {
            clientSocket->write("POST / HTTP/1.1\r\nHost: host\r\nContent-Length: 5\r\n\r\n");

            THEN("handler mapped to request path is called")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(handlerSemaphore, 1));

                AND_WHEN("request handling code finishes writing response")
                {
                    switch (responseType)
                    {
                        case ResponseType::InHandler:
                            break;
                        case ResponseType::PartialInHandler:
                            pBroker->writeChunk("Hello");
                            pBroker->writeLastChunk();
                            break;
                        case ResponseType::OutsideHandler:
                            pBroker->writeResponse();
                            break;
                    }

                    THEN("HttpConnectionHandler awaits for client to send rest of request")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(responseSemaphore, 1));

                        AND_WHEN("client sends rest of the first request and metadata for second request")
                        {
                            clientSocket->write("HelloGET /another HTTP/1.1\r\nHost: host.com\r\n\r\n");

                            THEN("HttpConnectionHandler calls handler mapped to second request path")
                            {
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(handlerSemaphore, 1));
                            }
                        }
                    }
                }
            }
        }
    }
}


SCENARIO("HttpConnectionHandler processes all requests in buffer without the need for control "
         "to return to the event loop when response is fully written in handler function")
{
    GIVEN("a connected client")
    {
        auto tcpSockets = createConnectedSocketPair();
        std::unique_ptr<TcpSocket> clientSocket(tcpSockets.first);
        REQUIRE(clientSocket->state() == TcpSocket::State::Connected);
        auto pHttpRequestRouter = std::make_shared<HttpRequestRouter>();
        static thread_local std::string path;
        static thread_local QSemaphore handlerSemaphore;
        static thread_local QSemaphore bodySemaphore;
        static thread_local HttpBroker *pBroker = nullptr;
        static thread_local std::vector<std::pair<std::string, std::string>> handlerData;
        static constexpr auto pHandlerFcn = [](const HttpRequest &request, HttpBroker &broker)
        {
            broker.writeResponse();
            handlerData.push_back({std::string(request.targetPath()), std::string(request.body())});
            QObject::connect(&broker, &HttpBroker::receivedBodyData, [&](std::string_view bodyData, bool isLastPart)
            {
                handlerData.back().second.append(bodyData);
                bodySemaphore.release();
            });
            handlerSemaphore.release();
        };
        REQUIRE(pHttpRequestRouter->addRoute(HttpRequest::Method::POST, "/1", pHandlerFcn));
        REQUIRE(pHttpRequestRouter->addRoute(HttpRequest::Method::POST, "/2", pHandlerFcn));
        REQUIRE(pHttpRequestRouter->addRoute(HttpRequest::Method::POST, "/3", pHandlerFcn));
        HttpConnectionHandler httpConnectionHandler(*tcpSockets.second, std::make_shared<HttpRequestLimits>(), pHttpRequestRouter, 0, 0);

        WHEN("three requests are written")
        {
            clientSocket->write("POST /1 HTTP/1.1\r\nHost: host\r\nContent-Length: 5\r\n\r\nHello"
                                "POST /2 HTTP/1.1\r\nHost: host\r\nTransfer-Encoding: chunked\r\n\r\n9\r\nWonderful\r\n0\r\n\r\n"
                                "POST /3 HTTP/1.1\r\nHost: host\r\nContent-Length: 6\r\n\r\nWorld!"
                                "POST /1 HTTP/1.1\r\nHost: host\r\n\r\n"
                                "POST /2 HTTP/1.1\r\nHost: host\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n"
                                "POST /3 HTTP/1.1\r\nHost: host\r\nContent-Length: 0\r\n\r\n");
            const std::vector<std::pair<std::string, std::string>> expectedData{
                {"/1", "Hello"},
                {"/2", "Wonderful"},
                {"/3", "World!"},
                {"/1", ""},
                {"/2", ""},
                {"/3", ""},};

            THEN("all requests are processed without the need for control to return to the event loop")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(handlerSemaphore, 1));
                REQUIRE(handlerSemaphore.tryAcquire(5));
                REQUIRE(bodySemaphore.tryAcquire(3));
                REQUIRE(handlerData.size() == 6);
                REQUIRE(handlerData == expectedData);
            }
        }
    }
}


SCENARIO("HttpConnectionHandler sends 100-continue before calling handler when request contains Expect: continue header")
{
    GIVEN("a connected client")
    {
        auto tcpSockets = createConnectedSocketPair();
        std::unique_ptr<TcpSocket> clientSocket(tcpSockets.first);
        REQUIRE(clientSocket->state() == TcpSocket::State::Connected);
        auto pHttpRequestRouter = std::make_shared<HttpRequestRouter>();
        static thread_local std::string calledRoute;
        static thread_local QSemaphore handlerSemaphore;
        REQUIRE(pHttpRequestRouter->addRoute(HttpRequest::Method::POST, "/send_large_data", [](const HttpRequest &, HttpBroker &broker){broker.writeResponse(); handlerSemaphore.release();}));
        HttpConnectionHandler httpConnectionHandler(*tcpSockets.second, std::make_shared<HttpRequestLimits>(), pHttpRequestRouter, 0, 0);
        QSemaphore responseSemaphore;
        std::string response;
        Object::connect(clientSocket.get(), &TcpSocket::receivedData, [&]()
        {
            response.append(clientSocket->readAll());
            responseSemaphore.release();
        });

        WHEN("client sends a request containing a 'Expect: 100-continue' header line")
        {
            clientSocket->write("POST /send_large_data HTTP/1.1\r\nHost: host\r\nExpect: 100-continue\r\n\r\n");

            THEN("client receives a 100-continue response before response written by handler")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(handlerSemaphore, 1));
                QCoreApplication::processEvents();
                REQUIRE(!handlerSemaphore.tryAcquire());
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(responseSemaphore, 1));
                REQUIRE(response.starts_with("HTTP/1.1 100 Continue\r\n\r\n"));
            }
        }

        WHEN("client sends a request without a 'Expect: 100-continue' header line")
        {
            clientSocket->write("POST /send_large_data HTTP/1.1\r\nHost: host\r\n\r\n");

            THEN("client does not receive a 100-continue response")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(handlerSemaphore, 1));
                QCoreApplication::processEvents();
                REQUIRE(!handlerSemaphore.tryAcquire());
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(responseSemaphore, 1));
                REQUIRE(response.starts_with("HTTP/1.1 200 OK\r\n"));
            }
        }
    }
}


SCENARIO("HttpConnectionHandler supports server-wide options requests")
{
    GIVEN("a connected client")
    {
        auto tcpSockets = createConnectedSocketPair();
        std::unique_ptr<TcpSocket> clientSocket(tcpSockets.first);
        REQUIRE(clientSocket->state() == TcpSocket::State::Connected);
        auto pHttpRequestRouter = std::make_shared<HttpRequestRouter>();
        static thread_local std::string calledRoute;
        static thread_local QSemaphore handlerSemaphore;
        REQUIRE(pHttpRequestRouter->addRoute(HttpRequest::Method::OPTIONS, "*", [](const HttpRequest &, HttpBroker&){handlerSemaphore.release();}));
        HttpConnectionHandler httpConnectionHandler(*tcpSockets.second, std::make_shared<HttpRequestLimits>(), pHttpRequestRouter, 0, 0);

        WHEN("client sends a server-wide request")
        {
            clientSocket->write("OPTIONS * HTTP/1.1\r\nHost: host\r\n\r\n");

            THEN("request handler function mapped to server-wide options gets called")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(handlerSemaphore, 1));
                QCoreApplication::processEvents();
                REQUIRE(!handlerSemaphore.tryAcquire());
            }
        }
    }
}


SCENARIO("HttpConnectionHandler allows request handling code to close http connection")
{
    GIVEN("a connected client")
    {
        auto tcpSockets = createConnectedSocketPair();
        std::unique_ptr<TcpSocket> clientSocket(tcpSockets.first);
        REQUIRE(clientSocket->state() == TcpSocket::State::Connected);
        auto pHttpRequestRouter = std::make_shared<HttpRequestRouter>();
        static thread_local std::string calledRoute;
        static thread_local QSemaphore handlerSemaphore;
        REQUIRE(pHttpRequestRouter->addRoute(HttpRequest::Method::GET, "/", [](const HttpRequest &, HttpBroker &broker){broker.closeConnectionAfterResponding(); broker.writeResponse(); handlerSemaphore.release();}));
        HttpConnectionHandler httpConnectionHandler(*tcpSockets.second, std::make_shared<HttpRequestLimits>(), pHttpRequestRouter, 0, 0);
        QSemaphore responseSemaphore;
        std::string response;
        Object::connect(clientSocket.get(), &TcpSocket::receivedData, [&]()
        {
            response.append(clientSocket->readAll());
            responseSemaphore.release();
        });
        QSemaphore disconnectedSemaphore;
        Object::connect(clientSocket.get(), &TcpSocket::disconnected, [&]()
        {
            disconnectedSemaphore.release();
        });

        WHEN("client sends request")
        {
            clientSocket->write("GET / HTTP/1.1\r\nHost: host\r\n\r\n");

            THEN("client receives a 200-ok response before connection gets closed")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(handlerSemaphore, 1));
                QCoreApplication::processEvents();
                REQUIRE(!handlerSemaphore.tryAcquire());
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(responseSemaphore, 1));
                REQUIRE(response.starts_with("HTTP/1.1 200 OK\r\n"));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(disconnectedSemaphore, 1));
            }
        }
    }
}


SCENARIO("HttpConnectionHandler closes connection if handler function does not send a full response and does not set a QObject on broker")
{
    GIVEN("a connected client")
    {
        auto tcpSockets = createConnectedSocketPair();
        std::unique_ptr<TcpSocket> clientSocket(tcpSockets.first);
        REQUIRE(clientSocket->state() == TcpSocket::State::Connected);
        auto pHttpRequestRouter = std::make_shared<HttpRequestRouter>();
        static thread_local std::string path;
        static thread_local QSemaphore handlerSemaphore;
        static thread_local HttpBroker *pBroker = nullptr;
        static constexpr auto pHandlerFcn = [](const HttpRequest &request, HttpBroker &broker)
        {
            if (request.targetPath() == "/chunked")
                broker.writeChunkedResponse();
            handlerSemaphore.release();
        };
        REQUIRE(pHttpRequestRouter->addRoute(HttpRequest::Method::GET, "/", pHandlerFcn));
        REQUIRE(pHttpRequestRouter->addRoute(HttpRequest::Method::GET, "/chunked", pHandlerFcn));
        HttpConnectionHandler httpConnectionHandler(*tcpSockets.second, std::make_shared<HttpRequestLimits>(), pHttpRequestRouter, 0, 0);
        QSemaphore finishedSemaphore;
        Object::connect(&httpConnectionHandler, &HttpConnectionHandler::finished, [&](ConnectionHandler *pHandler)
        {
            REQUIRE(pHandler == &httpConnectionHandler);
            finishedSemaphore.release();
        });

        WHEN("client sends GET request to /")
        {
            QSemaphore disconnectedSemaphore;
            Object::connect(clientSocket.get(), &TcpSocket::disconnected, [&](){disconnectedSemaphore.release();});
            clientSocket->write("GET / HTTP/1.1\r\nHost: host.com\r\n\r\n");

            THEN("HttpConnectionHandler closes connection after handler finishes without fully responding or setting a QObject")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(handlerSemaphore, 1));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(disconnectedSemaphore, 1));
                QCoreApplication::processEvents();
                REQUIRE(!handlerSemaphore.tryAcquire());
                REQUIRE(!disconnectedSemaphore.tryAcquire());
                REQUIRE(clientSocket->readAll().empty());
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(finishedSemaphore, 1));
            }
        }

        WHEN("client sends GET request to /chunked")
        {
            QSemaphore disconnectedSemaphore;
            Object::connect(clientSocket.get(), &TcpSocket::disconnected, [&](){disconnectedSemaphore.release();});
            clientSocket->write("GET /chunked HTTP/1.1\r\nHost: host.com\r\n\r\n");

            THEN("HttpConnectionHandler closes connection after handler finishes without fully responding or setting a QObject")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(handlerSemaphore, 1));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(disconnectedSemaphore, 1));
                QCoreApplication::processEvents();
                REQUIRE(!handlerSemaphore.tryAcquire());
                REQUIRE(!disconnectedSemaphore.tryAcquire());
                REQUIRE(clientSocket->readAll().starts_with("HTTP/1.1 200 OK\r\n"));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(finishedSemaphore, 1));
            }
        }
    }
}


SCENARIO("HttpConnectionHandler allows request handling code to know how much data is pending to be sent to peer")
{
    GIVEN("a connected client")
    {
        auto tcpSockets = createConnectedSocketPair();
        std::unique_ptr<TcpSocket> clientSocket(tcpSockets.first);
        REQUIRE(clientSocket->state() == TcpSocket::State::Connected);
        auto pHttpRequestRouter = std::make_shared<HttpRequestRouter>();
        static thread_local std::string path;
        static thread_local QSemaphore handlerSemaphore;
        static thread_local size_t dataToBeSentToPeer = 0;
        static constexpr auto pHandlerFcn = [](const HttpRequest &request, HttpBroker &broker)
        {
            REQUIRE(broker.bytesToSend() == 0);
            broker.writeResponse();
            dataToBeSentToPeer = broker.bytesToSend();
            handlerSemaphore.release();
        };
        REQUIRE(pHttpRequestRouter->addRoute(HttpRequest::Method::GET, "/", pHandlerFcn));
        HttpConnectionHandler httpConnectionHandler(*tcpSockets.second, std::make_shared<HttpRequestLimits>(), pHttpRequestRouter, 0, 0);
        QSemaphore responseSemaphore;
        Object::connect(clientSocket.get(), &TcpSocket::receivedData, [&](){responseSemaphore.release();});

        WHEN("client sends request")
        {
            clientSocket->write("GET / HTTP/1.1\r\nHost: host\r\n\r\n");

            THEN("handler is called and data to be sent to peer is stored")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(handlerSemaphore, 1));
                REQUIRE(dataToBeSentToPeer > 0);

                AND_WHEN("client receives response")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(responseSemaphore, 1));

                    THEN("received response size equals data to be sent to peer")
                    {
                        REQUIRE(clientSocket->dataAvailable() == dataToBeSentToPeer);
                    }
                }
            }
        }
    }
}


SCENARIO("HttpConnectionHandler informs request handling code whenever data is sent to peer")
{
    GIVEN("a connected client")
    {
        auto tcpSockets = createConnectedSocketPair();
        std::unique_ptr<TcpSocket> clientSocket(tcpSockets.first);
        REQUIRE(clientSocket->state() == TcpSocket::State::Connected);
        auto pHttpRequestRouter = std::make_shared<HttpRequestRouter>();
        static thread_local std::string path;
        static thread_local QSemaphore handlerSemaphore;
        static thread_local size_t dataToBeSentToPeer = 0;
        static thread_local QSemaphore sentDataToPeerSemaphore;
        static thread_local size_t dataSentToPeer = 0;
        static constexpr auto pHandlerFcn = [](const HttpRequest &request, HttpBroker &broker)
        {
            REQUIRE(broker.bytesToSend() == 0);
            QObject::connect(&broker, &HttpBroker::sentData, [&](size_t count)
            {
                dataSentToPeer += count;
                if (broker.bytesToSend() == 0)
                    sentDataToPeerSemaphore.release();
            });
            broker.writeChunkedResponse();
            dataToBeSentToPeer = broker.bytesToSend();
            handlerSemaphore.release();
        };
        REQUIRE(pHttpRequestRouter->addRoute(HttpRequest::Method::GET, "/", pHandlerFcn));
        HttpConnectionHandler httpConnectionHandler(*tcpSockets.second, std::make_shared<HttpRequestLimits>(), pHttpRequestRouter, 0, 0);
        QSemaphore responseSemaphore;
        Object::connect(clientSocket.get(), &TcpSocket::receivedData, [&](){responseSemaphore.release();});

        WHEN("client sends request")
        {
            clientSocket->write("GET / HTTP/1.1\r\nHost: host\r\n\r\n");

            THEN("handler is called and data to be sent to peer is stored")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(handlerSemaphore, 1));
                REQUIRE(dataToBeSentToPeer > 0);

                AND_THEN("broker emits sentData until there is no more data to be sent to peer")
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(sentDataToPeerSemaphore, 1));

                    AND_THEN("data sent to peer equals data to be sent to peer")
                    {
                        REQUIRE(dataSentToPeer == dataToBeSentToPeer);
                    }
                }
            }
        }
    }
}


SCENARIO("HttpConnectionHandler informs if received body data is the last one")
{
    GIVEN("a connected client")
    {
        auto tcpSockets = createConnectedSocketPair();
        std::unique_ptr<TcpSocket> clientSocket(tcpSockets.first);
        REQUIRE(clientSocket->state() == TcpSocket::State::Connected);
        auto pHttpRequestRouter = std::make_shared<HttpRequestRouter>();
        static thread_local QSemaphore handlerSemaphore;
        REQUIRE(!handlerSemaphore.tryAcquire());
        static thread_local QSemaphore bodySemaphore;
        REQUIRE(!bodySemaphore.tryAcquire());
        static thread_local std::string receivedBodyData;
        receivedBodyData.clear();
        static thread_local bool isLastPart = false;
        isLastPart = false;
        static constexpr auto pHandlerFcn = [](const HttpRequest &request, HttpBroker &broker)
        {
            QObject::connect(&broker, &HttpBroker::receivedBodyData, [](std::string_view bodyData, bool isLastBodyPart)
            {
                receivedBodyData = bodyData;
                isLastPart = isLastBodyPart;
                bodySemaphore.release();
            });
            broker.setQObject(new QObject);
            handlerSemaphore.release();
        };
        REQUIRE(pHttpRequestRouter->addRoute(HttpRequest::Method::POST, "/", pHandlerFcn));
        HttpConnectionHandler httpConnectionHandler(*tcpSockets.second, std::make_shared<HttpRequestLimits>(), pHttpRequestRouter, 0, 0);

        WHEN("client sends request metadata")
        {
            clientSocket->write("POST / HTTP/1.1\r\nHost: host\r\nContent-Length: 23\r\n\r\n");

            THEN("handler gets called")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(handlerSemaphore, 1));

                AND_WHEN("client writes part of body")
                {
                    THEN("broker emits received data with sent body data and indicates if given part is the last one")
                    {
                        const std::vector<std::string_view> bodyParts{"Hello ", "Incredible ", "World!"};
                        for (auto bodyPart : bodyParts)
                        {
                            REQUIRE(!isLastPart);
                            clientSocket->write(bodyPart);
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(bodySemaphore, 1));
                            REQUIRE(receivedBodyData == bodyPart);
                        }
                        REQUIRE(isLastPart);
                    }
                }
            }
        }

        WHEN("client sends a chunked request metadata")
        {
            clientSocket->write("POST / HTTP/1.1\r\nHost: host\r\nTransfer-Encoding: chunked\r\n\r\n");

            THEN("handler gets called")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(handlerSemaphore, 1));

                AND_WHEN("client writes part of body")
                {
                    THEN("broker emits received data with sent body data and indicates if given part is the last one")
                    {
                        const std::vector<std::pair<std::string_view, std::string_view>> chunksAndBodies{{"6\r\nHello \r\n", "Hello "},
                                                                                                         {"b\r\nIncredible \r\n", "Incredible "},
                                                                                                         {"6\r\nWorld!\r\n", "World!"},
                                                                                                         {"0\r\n\r\n", ""}};
                        for (auto chunkAndBodyPart : chunksAndBodies)
                        {
                            REQUIRE(!isLastPart);
                            clientSocket->write(chunkAndBodyPart.first);
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(bodySemaphore, 1));
                            REQUIRE(receivedBodyData == chunkAndBodyPart.second);
                        }
                        REQUIRE(isLastPart);
                    }
                }
            }
        }

        WHEN("client sends a complete chunked request")
        {
            static thread_local std::vector<std::pair<std::string, bool>> emittedData;
            static constexpr auto pFullHandlerFcn = [](const HttpRequest &request, HttpBroker &broker)
            {
                QObject::connect(&broker, &HttpBroker::receivedBodyData, [](std::string_view bodyData, bool isLastBodyPart)
                {
                    emittedData.push_back({std::string(bodyData), isLastBodyPart});
                    bodySemaphore.release();
                });
                broker.setQObject(new QObject);
                handlerSemaphore.release();
            };
            REQUIRE(pHttpRequestRouter->addRoute(HttpRequest::Method::POST, "/full", pFullHandlerFcn));

            clientSocket->write("POST /full HTTP/1.1\r\nHost: host\r\nTransfer-Encoding: chunked\r\n\r\n"
                                "6\r\nHello \r\n"
                                "b\r\nIncredible \r\n"
                                "6\r\nWorld!\r\n"
                                "0\r\n\r\n");

            THEN("handler gets called and broker emits received data for every chunk in request")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(handlerSemaphore, 1));
                REQUIRE(bodySemaphore.tryAcquire(4));
                const std::vector<std::pair<std::string, bool>> expectedEmittedData{{"Hello ", false},
                                                                                    {"Incredible ", false},
                                                                                    {"World!", false},
                                                                                    {"", true}};
                REQUIRE(emittedData == expectedEmittedData);
            }
        }
    }
}


SCENARIO("HttpConnectionHandler informs if last chunk in chunked request contains trailers")
{
    GIVEN("a connected client")
    {
        auto tcpSockets = createConnectedSocketPair();
        std::unique_ptr<TcpSocket> clientSocket(tcpSockets.first);
        REQUIRE(clientSocket->state() == TcpSocket::State::Connected);
        auto pHttpRequestRouter = std::make_shared<HttpRequestRouter>();
        static thread_local QSemaphore handlerSemaphore;
        REQUIRE(!handlerSemaphore.tryAcquire());
        static thread_local QSemaphore bodySemaphore;
        REQUIRE(!bodySemaphore.tryAcquire());
        static thread_local std::vector<std::pair<bool, bool>> emittedIsLastPartHasTrailersData;
        emittedIsLastPartHasTrailersData.clear();
        static thread_local HttpBroker *pBroker = nullptr;
        pBroker = nullptr;
        static constexpr auto pHandlerFcn = [](const HttpRequest &request, HttpBroker &broker)
        {
            pBroker = &broker;
            QObject::connect(&broker, &HttpBroker::receivedBodyData, [&](std::string_view bodyData, bool isLastBodyPart)
            {
                emittedIsLastPartHasTrailersData.push_back({isLastBodyPart, broker.hasTrailers()});
                bodySemaphore.release();
            });
            broker.setQObject(new QObject);
            handlerSemaphore.release();
        };
        REQUIRE(pHttpRequestRouter->addRoute(HttpRequest::Method::POST, "/", pHandlerFcn));
        HttpConnectionHandler httpConnectionHandler(*tcpSockets.second, std::make_shared<HttpRequestLimits>(), pHttpRequestRouter, 0, 0);

        WHEN("client sends request metadata")
        {
            clientSocket->write("POST / HTTP/1.1\r\nHost: host\r\nContent-Length: 23\r\n\r\n");

            THEN("handler gets called")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(handlerSemaphore, 1));

                AND_WHEN("client writes part of body")
                {
                    THEN("broker emits received data whenever client writes a part of the request's body")
                    {
                        const std::vector<std::string_view> bodyParts{"Hello ", "Incredible ", "World!"};
                        for (auto bodyPart : bodyParts)
                        {
                            clientSocket->write(bodyPart);
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(bodySemaphore, 1));
                        }
                        const std::vector<std::pair<bool, bool>> expectedEmittedData{{false, false}, {false, false}, {true, false}};
                        REQUIRE(emittedIsLastPartHasTrailersData == expectedEmittedData);
                        REQUIRE(!pBroker->hasTrailers());
                        REQUIRE(pBroker->trailersCount() == 0);
                    }
                }
            }
        }


        WHEN("client sends a chunked request metadata")
        {
            clientSocket->write("POST / HTTP/1.1\r\nHost: host\r\nTransfer-Encoding: chunked\r\n\r\n");

            THEN("handler gets called")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(handlerSemaphore, 1));

                AND_WHEN("client writes chunks and maybe trailers")
                {
                    THEN("broker emits received data whenever client writes a chunk and informs if given part is the last one as well as if request has trailers")
                    {
                        const std::vector<std::string_view> chunks{"6\r\nHello \r\n",
                                                                   "b\r\nIncredible \r\n",
                                                                   "6\r\nWorld!\r\n"};
                        const auto trailers = GENERATE(AS(std::vector<std::pair<std::string_view, std::string_view>>),
                                                                                {},
                                                                                {{"name", "value"}},
                                                                                {{"name1", "value1"}, {"name2", "value2"}});
                        for (auto chunk : chunks)
                        {
                            clientSocket->write(chunk);
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(bodySemaphore, 1));
                        }
                        clientSocket->write("0\r\n");
                        for (auto [name, value] : trailers)
                        {
                            clientSocket->write(name);
                            clientSocket->write(": ");
                            clientSocket->write(value);
                            clientSocket->write("\r\n");
                        }
                        clientSocket->write("\r\n");
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(bodySemaphore, 1));
                        QCoreApplication::processEvents();
                        REQUIRE(!bodySemaphore.tryAcquire());
                        const std::vector<std::pair<bool, bool>> expectedEmittedData{{false, false}, {false, false}, {false, false}, {true, !trailers.empty()}};
                        REQUIRE(emittedIsLastPartHasTrailersData == expectedEmittedData);
                        REQUIRE(pBroker->hasTrailers() == !trailers.empty());
                        REQUIRE(pBroker->trailersCount() == trailers.size());
                        for (auto [name, value] : trailers)
                        {
                            REQUIRE(pBroker->hasTrailer(name));
                            REQUIRE(pBroker->trailerCount(name) == 1);
                            REQUIRE(pBroker->trailer(name) == value);
                        }
                    }
                }
            }
        }

        WHEN("client sends a full chunked request")
        {
            clientSocket->write("POST / HTTP/1.1\r\nHost: host\r\nTransfer-Encoding: chunked\r\n\r\n"
                                "6\r\nHello \r\n"
                                "b\r\nIncredible \r\n"
                                "6\r\nWorld!\r\n");
            const auto trailers = GENERATE(AS(std::vector<std::pair<std::string_view, std::string_view>>),
                                           {},
                                           {{"name", "value"}},
                                           {{"name1", "value1"}, {"name2", "value2"}});
            clientSocket->write("0\r\n");
            for (auto [name, value] : trailers)
            {
                clientSocket->write(name);
                clientSocket->write(": ");
                clientSocket->write(value);
                clientSocket->write("\r\n");
            }
            clientSocket->write("\r\n");

            THEN("handler gets called")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(handlerSemaphore, 1));

                AND_THEN("broker emits received data whenever client writes a chunk and informs if given part is the last one as well as if request has trailers")
                {
                    REQUIRE(bodySemaphore.tryAcquire(4));
                    QCoreApplication::processEvents();
                    REQUIRE(!bodySemaphore.tryAcquire());
                    const std::vector<std::pair<bool, bool>> expectedEmittedData{{false, false}, {false, false}, {false, false}, {true, !trailers.empty()}};
                    REQUIRE(emittedIsLastPartHasTrailersData == expectedEmittedData);
                    REQUIRE(pBroker->hasTrailers() == !trailers.empty());
                    REQUIRE(pBroker->trailersCount() == trailers.size());
                    for (auto [name, value] : trailers)
                    {
                        REQUIRE(pBroker->hasTrailer(name));
                        REQUIRE(pBroker->trailerCount(name) == 1);
                        REQUIRE(pBroker->trailer(name) == value);
                    }
                }
            }
        }
    }
}


SCENARIO("HttpRequest knows client IP/port")
{
    GIVEN("a connected client")
    {
        auto tcpSockets = createConnectedSocketPair();
        std::unique_ptr<TcpSocket> clientSocket(tcpSockets.first);
        REQUIRE(clientSocket->state() == TcpSocket::State::Connected);
        auto pHttpRequestRouter = std::make_shared<HttpRequestRouter>();
        static thread_local std::string clientIp;
        clientIp.clear();
        static thread_local uint16_t clientPort = 0;
        clientPort = 0;
        static thread_local QSemaphore handlerSemaphore;
        REQUIRE(pHttpRequestRouter->addRoute(HttpRequest::Method::GET, "/", [](const HttpRequest &request, HttpBroker &broker){clientIp = request.peerAddress(); clientPort = request.peerPort(); broker.writeResponse(); handlerSemaphore.release();}));
        HttpConnectionHandler httpConnectionHandler(*tcpSockets.second, std::make_shared<HttpRequestLimits>(), pHttpRequestRouter, 0, 0);

        WHEN("client writes a request")
        {
            clientSocket->write("GET / HTTP/1.1\r\nHost: host.com\r\n\r\n");

            THEN("handler is called and request gives correct client IP/Port")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(handlerSemaphore, 1));
                QCoreApplication::processEvents();
                REQUIRE(!handlerSemaphore.tryAcquire());
                REQUIRE(clientSocket->localAddress() == clientIp);
                REQUIRE(clientSocket->localPort() == clientPort);
            }
        }
    }
}

namespace Test::HttpConnectionHandler
{

class ErrorHandlerTest : public ErrorHandler
{
public:
    ErrorHandlerTest() = default;
    ~ErrorHandlerTest() override = default;
    void handleError(HttpServer::ServerError error, std::string_view clientIp, uint16_t clientPort) override
    {
        REQUIRE(!m_hasHandled);
        m_hasHandled = true;
        m_error = error;
        m_clientIp = clientIp;
        m_clientPort = clientPort;
    }
    inline bool hasHandled() const {return m_hasHandled;}
    inline HttpServer::ServerError error() const {return m_error;}
    inline std::string_view clientIp() const {return m_clientIp;}
    inline uint16_t clientPort() const {return m_clientPort;}

private:
    HttpServer::ServerError m_error = HttpServer::ServerError::NoError;
    std::string m_clientIp;
    uint16_t m_clientPort = 0;
    bool m_hasHandled = false;
};

}

using namespace Test::HttpConnectionHandler;


SCENARIO("HttpRequestHandler calls error handler on error")
{
    GIVEN("connected client that may have sent/received http messages")
    {
        auto tcpSockets = createConnectedSocketPair();
        std::unique_ptr<TcpSocket> clientSocket(tcpSockets.first);
        REQUIRE(clientSocket->state() == TcpSocket::State::Connected);
        const std::string clientIp(clientSocket->localAddress());
        const auto clientPort = clientSocket->localPort();
        auto pHttpRequestRouter = std::make_shared<HttpRequestRouter>();
        REQUIRE(pHttpRequestRouter->addRoute(HttpRequest::Method::GET, "/hello", [](const HttpRequest &request, HttpBroker &broker){broker.writeResponse("Hello World!");}));
        const int requestTimeoutInSecs = 1;
        const int idleTimeoutInSecs = 3;
        auto pErrorHandler = std::make_shared<ErrorHandlerTest>();
        auto pHttpRequestLimits = std::make_shared<HttpRequestLimits>();
        pHttpRequestLimits->maxUrlSize = 32;
        HttpConnectionHandler httpConnectionHandler(*tcpSockets.second, pHttpRequestLimits, pHttpRequestRouter, requestTimeoutInSecs, idleTimeoutInSecs, pErrorHandler);
        const auto sendReceiveCountPriorResuming = GENERATE(AS(int), 0, 1, 3);
        QSemaphore receivedResponseSemaphore;
        std::string receivedResponses;
        Object::connect(clientSocket.get(), &TcpSocket::receivedData, [&]()
        {
            receivedResponses.append(clientSocket->readAll());
            receivedResponseSemaphore.release();
        });
        QSemaphore disconnectedSemaphore;
        Object::connect(clientSocket.get(), &TcpSocket::disconnected, [&](){disconnectedSemaphore.release();});
        for (auto i = 0; i < sendReceiveCountPriorResuming; ++i)
        {
            clientSocket->write("GET /hello HTTP/1.1\r\nHost: host\r\n\r\n");
            do
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(receivedResponseSemaphore, 1));
            } while (!receivedResponses.ends_with("Hello World!"));
            receivedResponses.clear();
        }

        WHEN("client takes too long to start sending a request")
        {
            QElapsedTimer elapsedTimer;
            elapsedTimer.start();
            receivedResponses.clear();
            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(receivedResponseSemaphore, idleTimeoutInSecs + 2));
            const auto elapsedTime = elapsedTimer.elapsed();

            THEN("server sends to client a response with a 408 Request Timeout status code before "
                 "closing the connection, and calls error handler with client IP and RequestTimeoutError error code")
            {
                REQUIRE((idleTimeoutInSecs * 1000) <= elapsedTime && elapsedTime <= (idleTimeoutInSecs * 1000 + 1024));
                REQUIRE(receivedResponses.starts_with("HTTP/1.1 408 Request Timeout\r\n"));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(disconnectedSemaphore, 1));
                REQUIRE(pErrorHandler->hasHandled());
                REQUIRE(pErrorHandler->error() == HttpServer::ServerError::RequestTimeout);
                REQUIRE(pErrorHandler->clientIp() == clientIp);
                REQUIRE(pErrorHandler->clientPort() == clientPort);
            }
        }

        WHEN("client takes too long to send a complete request")
        {
            clientSocket->write("GET");
            QElapsedTimer elapsedTimer;
            elapsedTimer.start();
            receivedResponses.clear();
            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(receivedResponseSemaphore, idleTimeoutInSecs + 2));
            const auto elapsedTime = elapsedTimer.elapsed();

            THEN("server sends to client a response with a 408 Request Timeout status code before "
                 "closing the connection, and calls error handler with client IP and RequestTimeoutError error code")
            {
                REQUIRE((requestTimeoutInSecs * 1000) <= elapsedTime && elapsedTime <= (requestTimeoutInSecs * 1000 + 1024));
                REQUIRE(receivedResponses.starts_with("HTTP/1.1 408 Request Timeout\r\n"));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(disconnectedSemaphore, 1));
                REQUIRE(pErrorHandler->hasHandled());
                REQUIRE(pErrorHandler->error() == HttpServer::ServerError::RequestTimeout);
                REQUIRE(pErrorHandler->clientIp() == clientIp);
                REQUIRE(pErrorHandler->clientPort() == clientPort);
            }
        }

        WHEN("client sends a request with an unsupported http method")
        {
            clientSocket->write("MYMETHOD /hello HTTP/1.1\r\nHost: host\r\n\r\n");

            THEN("server sends to client a response with a 400 Bad Request status code before "
                 "closing the connection, and calls error handler with client IP and MalformedRequest error code")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(receivedResponseSemaphore, 1));
                REQUIRE(receivedResponses.starts_with("HTTP/1.1 400 Bad Request\r\n"));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(disconnectedSemaphore, 1));
                REQUIRE(pErrorHandler->hasHandled());
                REQUIRE(pErrorHandler->error() == HttpServer::ServerError::MalformedRequest);
                REQUIRE(pErrorHandler->clientIp() == clientIp);
                REQUIRE(pErrorHandler->clientPort() == clientPort);
            }
        }

        WHEN("client sends a request to an unmapped path")
        {
            clientSocket->write("GET /unmapped/path HTTP/1.1\r\nHost: host\r\n\r\n");

            THEN("server sends to client a response with a 404 Not Found status code before "
                 "closing the connection, and calls error handler with client IP and MalformedRequest error code")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(receivedResponseSemaphore, 1));
                REQUIRE(receivedResponses.starts_with("HTTP/1.1 404 Not Found\r\n"));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(disconnectedSemaphore, 1));
                REQUIRE(pErrorHandler->hasHandled());
                REQUIRE(pErrorHandler->error() == HttpServer::ServerError::MalformedRequest);
                REQUIRE(pErrorHandler->clientIp() == clientIp);
                REQUIRE(pErrorHandler->clientPort() == clientPort);
            }
        }

        WHEN("client sends a request with uri that is larger than server is allowed to accept")
        {
            clientSocket->write("GET /hello?we_just_need_more_than_32_characters_here HTTP/1.1\r\nHost: host\r\n\r\n");

            THEN("server sends to client a response with a 400 Bad Request status code before "
                 "closing the connection, and calls error handler with client IP and TooBigRequest error code")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(receivedResponseSemaphore, 1));
                REQUIRE(receivedResponses.starts_with("HTTP/1.1 400 Bad Request\r\n"));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(disconnectedSemaphore, 1));
                REQUIRE(pErrorHandler->hasHandled());
                REQUIRE(pErrorHandler->error() == HttpServer::ServerError::TooBigRequest);
                REQUIRE(pErrorHandler->clientIp() == clientIp);
                REQUIRE(pErrorHandler->clientPort() == clientPort);
            }
        }

        WHEN("client sends a malformed request")
        {
            clientSocket->write("GET /hello HTTP/1.1\r\nNoHost: host\r\n\r\n");

            THEN("server sends to client a response with a 400 Bad Request status code before "
                 "closing the connection, and calls error handler with client IP and MalformedRequest error code")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(receivedResponseSemaphore, 1));
                REQUIRE(receivedResponses.starts_with("HTTP/1.1 400 Bad Request\r\n"));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(disconnectedSemaphore, 1));
                REQUIRE(pErrorHandler->hasHandled());
                REQUIRE(pErrorHandler->error() == HttpServer::ServerError::MalformedRequest);
                REQUIRE(pErrorHandler->clientIp() == clientIp);
                REQUIRE(pErrorHandler->clientPort() == clientPort);
            }
        }
    }
}
