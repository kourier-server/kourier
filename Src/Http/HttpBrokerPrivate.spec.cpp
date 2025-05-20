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

#include "HttpBrokerPrivate.h"
#include "Http/HttpRequestParser.h"
#include "../Core/IOChannel.h"
#include <Spectator.h>
#include <QDateTime>
#include <list>
#include <initializer_list>


using Kourier::HttpBrokerPrivate;
using Kourier::HttpStatusCode;
using Kourier::IOChannel;
using Kourier::RingBuffer;
using Kourier::DataSource;
using Kourier::DataSink;
using Kourier::HttpRequestParser;
using Kourier::HttpRequestLimits;
using Kourier::Object;
using Spectator::SemaphoreAwaiter;

namespace Test::HttpBrokerPrivate
{

class TestHttpBrokerPrivate
{
public:
    TestHttpBrokerPrivate(Kourier::HttpBrokerPrivate &broker) : m_broker(broker) {}
    void doWriteStatusLine(Kourier::HttpStatusCode statusCode) {m_broker.writeStatusLine(statusCode);}
    void doWriteContentLengthHeader(size_t size) {m_broker.writeContentLengthHeader(size);}
    void doWriteChunkMetadata(size_t size) {m_broker.writeChunkMetadata(size);}
    void doWriteDateHeader() {m_broker.writeDateHeader();}
    void doWriteServerHeader() {m_broker.writeServerHeader();}
    void doFinishWritingChunkedResponse() {m_broker.finishWritingChunkedResponse();}

private:
    Kourier::HttpBrokerPrivate &m_broker;
};

class DataSinkTest : public DataSink
{
public:
    DataSinkTest() = default;
    ~DataSinkTest() override = default;
    size_t write(const char *pData, size_t count) override {return 0;}
};

class IOChannelTest : public IOChannel
{
public:
    IOChannelTest() = default;
    ~IOChannelTest() override = default;
    RingBuffer &readBuffer() {return m_readBuffer;}
    RingBuffer &writeBuffer() {return m_writeBuffer;}
    bool &isReadNotificationEnabled() {return m_isReadNotificationEnabled;}
    bool &isWriteNotificationEnabled() {return m_isWriteNotificationEnabled;}

private:
    DataSource &dataSource() override {std::abort();}
    DataSink &dataSink() override {return m_dataSink;}
    void onReadNotificationChanged() override {}
    void onWriteNotificationChanged() override {}

private:
    DataSinkTest m_dataSink;
};

}

using namespace Test::HttpBrokerPrivate;


SCENARIO("HttpBrokerPrivate writes status line from status codes")
{
    GIVEN("a http status code")
    {
        const auto statusCode = GENERATE(AS(std::pair<HttpStatusCode, std::string_view>),
                                         {HttpStatusCode::Continue, "HTTP/1.1 100 Continue\r\n"}, // 100,
                                         {HttpStatusCode::SwitchingProtocols, "HTTP/1.1 101 Switching Protocols\r\n"}, // = 101,
                                         {HttpStatusCode::OK, "HTTP/1.1 200 OK\r\n"}, // = 200,
                                         {HttpStatusCode::Created, "HTTP/1.1 201 Created\r\n"}, // = 201,
                                         {HttpStatusCode::Accepted, "HTTP/1.1 202 Accepted\r\n"}, // = 202,
                                         {HttpStatusCode::NonAuthoritativeInformation, "HTTP/1.1 203 Non-Authoritative Information\r\n"}, // = 203,
                                         {HttpStatusCode::NoContent, "HTTP/1.1 204 No Content\r\n"}, // = 204,
                                         {HttpStatusCode::ResetContent, "HTTP/1.1 205 Reset Content\r\n"}, // = 205,
                                         {HttpStatusCode::PartialContent, "HTTP/1.1 206 Partial Content\r\n"}, // = 206,
                                         {HttpStatusCode::MultipleChoices, "HTTP/1.1 300 Multiple Choices\r\n"}, // = 300,
                                         {HttpStatusCode::MovedPermanently, "HTTP/1.1 301 Moved Permanently\r\n"}, // = 301,
                                         {HttpStatusCode::Found, "HTTP/1.1 302 Found\r\n"}, // = 302,
                                         {HttpStatusCode::SeeOther, "HTTP/1.1 303 See Other\r\n"}, // = 303,
                                         {HttpStatusCode::NotModified, "HTTP/1.1 304 Not Modified\r\n"}, // = 304,
                                         {HttpStatusCode::UseProxy, "HTTP/1.1 305 Use Proxy\r\n"}, // = 305,
                                         {HttpStatusCode::TemporaryRedirect, "HTTP/1.1 307 Temporary Redirect\r\n"}, // = 307,
                                         {HttpStatusCode::PermanentRedirect, "HTTP/1.1 308 Permanent Redirect\r\n"}, // = 308,
                                         {HttpStatusCode::BadRequest, "HTTP/1.1 400 Bad Request\r\n"}, // = 400,
                                         {HttpStatusCode::Unauthorized, "HTTP/1.1 401 Unauthorized\r\n"}, // = 401,
                                         {HttpStatusCode::PaymentRequired, "HTTP/1.1 402 Payment Required\r\n"}, // = 402,
                                         {HttpStatusCode::Forbidden, "HTTP/1.1 403 Forbidden\r\n"}, // = 403,
                                         {HttpStatusCode::NotFound, "HTTP/1.1 404 Not Found\r\n"}, // = 404,
                                         {HttpStatusCode::MethodNotAllowed, "HTTP/1.1 405 Method Not Allowed\r\n"}, // = 405,
                                         {HttpStatusCode::NotAcceptable, "HTTP/1.1 406 Not Acceptable\r\n"}, // = 406,
                                         {HttpStatusCode::ProxyAuthenticationRequired, "HTTP/1.1 407 Proxy Authentication Required\r\n"}, // = 407,
                                         {HttpStatusCode::RequestTimeout, "HTTP/1.1 408 Request Timeout\r\n"}, // = 408,
                                         {HttpStatusCode::Conflict, "HTTP/1.1 409 Conflict\r\n"}, // = 409,
                                         {HttpStatusCode::Gone, "HTTP/1.1 410 Gone\r\n"}, // = 410,
                                         {HttpStatusCode::LengthRequired, "HTTP/1.1 411 Length Required\r\n"}, // = 411,
                                         {HttpStatusCode::PreconditionFailed, "HTTP/1.1 412 Precondition Failed\r\n"}, // = 412,
                                         {HttpStatusCode::ContentTooLarge, "HTTP/1.1 413 Content Too Large\r\n"}, // = 413,
                                         {HttpStatusCode::URITooLong, "HTTP/1.1 414 URI Too Long\r\n"}, // = 414,
                                         {HttpStatusCode::UnsupportedMediaType, "HTTP/1.1 415 Unsupported Media Type\r\n"}, // = 415,
                                         {HttpStatusCode::RangeNotSatisfiable, "HTTP/1.1 416 Range Not Satisfiable\r\n"}, // = 416,
                                         {HttpStatusCode::ExpectationFailed, "HTTP/1.1 417 Expectation Failed\r\n"}, // = 417,
                                         {HttpStatusCode::MisdirectedRequest, "HTTP/1.1 421 Misdirected Request\r\n"}, // = 421,
                                         {HttpStatusCode::UnprocessableContent, "HTTP/1.1 422 Unprocessable Content\r\n"}, // = 422,
                                         {HttpStatusCode::UpgradeRequired, "HTTP/1.1 426 Upgrade Required\r\n"}, // = 426,
                                         {HttpStatusCode::InternalServerError, "HTTP/1.1 500 Internal Server Error\r\n"}, // = 500,
                                         {HttpStatusCode::NotImplemented, "HTTP/1.1 501 Not Implemented\r\n"}, // = 501,
                                         {HttpStatusCode::BadGateway, "HTTP/1.1 502 Bad Gateway\r\n"}, // = 502,
                                         {HttpStatusCode::ServiceUnavailable, "HTTP/1.1 503 Service Unavailable\r\n"}, // = 503,
                                         {HttpStatusCode::GatewayTimeout, "HTTP/1.1 504 Gateway Timeout\r\n"}, // = 504,
                                         {HttpStatusCode::HTTPVersionNotSupported, "HTTP/1.1 505 HTTP Version Not Supported\r\n"});

        WHEN("status line is written for given status code")
        {
            IOChannelTest ioChannel;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            HttpBrokerPrivate brokerPrivate(&ioChannel, &parser);
            TestHttpBrokerPrivate broker(brokerPrivate);
            broker.doWriteStatusLine(statusCode.first);


            THEN("correct status line is written for given status code")
            {
                REQUIRE(ioChannel.writeBuffer().peekAll() == statusCode.second);
            }
        }
    }
}


SCENARIO("HttpBrokerPrivate writes content-length header from size")
{
    GIVEN("a size")
    {
        const auto size = GENERATE(AS(std::pair<std::string_view, size_t>),
                                   {"Content-Length: 0\r\n", 0},
                                   {"Content-Length: 1\r\n", 1},
                                   {"Content-Length: 17\r\n", 17},
                                   {"Content-Length: 18446744073709551615\r\n", 18446744073709551615ull},
                                   {"Content-Length: 1844674407370955161\r\n", 1844674407370955161ull},
                                   {"Content-Length: 18446744073\r\n", 18446744073ull});
        static_assert(std::numeric_limits<size_t>::min() == 0);
        static_assert(std::numeric_limits<size_t>::max() == 18446744073709551615ull);
        IOChannelTest ioChannel;
        HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
        HttpBrokerPrivate brokerPrivate(&ioChannel, &parser);
        TestHttpBrokerPrivate broker(brokerPrivate);

        WHEN("HttpBrokerPrivate writes content-length header")
        {
            broker.doWriteContentLengthHeader(size.second);

            THEN("header is written correctly")
            {
                REQUIRE(ioChannel.writeBuffer().peekAll() == size.first);
            }
        }
    }
}


SCENARIO("HttpBrokerPrivate writes chunk metadata from size")
{
    GIVEN("a size")
    {
        const auto size = GENERATE(AS(std::pair<std::string_view, size_t>),
                                   {"0\r\n", 0},
                                   {"1\r\n", 1},
                                   {"11\r\n", 17},
                                   {"ffffffffffffffff\r\n", 18446744073709551615ull},
                                   {"1999999999999999\r\n", 1844674407370955161ull},
                                   {"44b82fa09\r\n", 18446744073ull});
        static_assert(std::numeric_limits<size_t>::min() == 0);
        static_assert(std::numeric_limits<size_t>::max() == 18446744073709551615ull);
        IOChannelTest ioChannel;
        HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
        HttpBrokerPrivate brokerPrivate(&ioChannel, &parser);
        TestHttpBrokerPrivate broker(brokerPrivate);

        WHEN("HttpBrokerPrivate writes chunk metadata")
        {
            broker.doWriteChunkMetadata(size.second);

            THEN("chunk metadata is written correctly")
            {
                REQUIRE(ioChannel.writeBuffer().peekAll() == size.first);
            }
        }
    }
}


SCENARIO("HttpBrokerPrivate knows how to write current date header")
{
    GIVEN("a broker")
    {
        IOChannelTest ioChannel;
        HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
        HttpBrokerPrivate brokerPrivate(&ioChannel, &parser);
        TestHttpBrokerPrivate broker(brokerPrivate);

        WHEN("date header is written")
        {
            broker.doWriteDateHeader();

            THEN("date header is written correctly")
            {
                std::string dateHeader(ioChannel.writeBuffer().peekAll());
                REQUIRE(dateHeader.starts_with("Date: "));
                dateHeader.erase(0, 6);
                REQUIRE(dateHeader.ends_with("GMT\r\n"));
                dateHeader.erase(dateHeader.size() - 5, 5);
                dateHeader.append("+0000");
                const auto dateFromHeader = QDateTime::fromString(QString::fromLatin1(dateHeader.c_str()), Qt::RFC2822Date);
                REQUIRE(dateFromHeader.isValid());
                REQUIRE(qAbs(dateFromHeader.toSecsSinceEpoch() - QDateTime::currentDateTimeUtc().toSecsSinceEpoch()) <= 5);
            }
        }
    }
}


SCENARIO("HttpBrokerPrivate knows how to write server header")
{
    GIVEN("a broker")
    {
        IOChannelTest ioChannel;
        HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
        HttpBrokerPrivate brokerPrivate(&ioChannel, &parser);
        TestHttpBrokerPrivate broker(brokerPrivate);

        WHEN("broker writes server header")
        {
            broker.doWriteServerHeader();

            THEN("broker writes server header correctly")
            {
                REQUIRE(ioChannel.writeBuffer().peekAll() == "Server: Kourier\r\n");
            }
        }
    }
}


SCENARIO("HttpBrokerPrivate knows how to finish writing a chunked response")
{
    GIVEN("a broker")
    {
        IOChannelTest ioChannel;
        HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
        HttpBrokerPrivate brokerPrivate(&ioChannel, &parser);
        TestHttpBrokerPrivate broker(brokerPrivate);
        const auto createQObject = GENERATE(AS(bool), true, false);
        QObject * const pObject = createQObject ? new QObject : nullptr;
        brokerPrivate.setQObject(pObject);
        QSemaphore destroyedQObjectSemaphore;
        if (pObject)
            QObject::connect(pObject, &QObject::destroyed, [&](){destroyedQObjectSemaphore.release();});
        bool emittedWroteResponse = false;
        Object::connect(&brokerPrivate, &HttpBrokerPrivate::wroteResponse, [&](){emittedWroteResponse = true;});

        WHEN("broker finishes writing a chunked response")
        {
            broker.doFinishWritingChunkedResponse();

            THEN("broker writes chunked response ending correctly, does not delete any QObject set and emits wroteResponse")
            {
                REQUIRE(ioChannel.writeBuffer().peekAll() == "0\r\n\r\n");
                if (pObject)
                {
                    QCoreApplication::processEvents();
                    QCoreApplication::sendPostedEvents(pObject, QEvent::DeferredDelete);
                    QCoreApplication::processEvents();
                    REQUIRE(!destroyedQObjectSemaphore.tryAcquire());
                }
                REQUIRE(emittedWroteResponse);

                AND_WHEN("broker is reset")
                {
                    if (pObject)
                    {
                        REQUIRE(!destroyedQObjectSemaphore.tryAcquire());
                    }
                    brokerPrivate.resetResponseWriting();

                    THEN("any QObject set is deleted when control returns to the event loop")
                    {
                        if (pObject)
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(destroyedQObjectSemaphore, 1));
                        }
                    }
                }
            }
        }
    }
}


SCENARIO("HttpServerBrokers knows how to write responses")
{
    GIVEN("a private broker and a status code")
    {
        IOChannelTest ioChannel;
        HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
        HttpBrokerPrivate brokerPrivate(&ioChannel, &parser);
        bool hasWrittenResponse = false;
        Object::connect(&brokerPrivate, &HttpBrokerPrivate::wroteResponse, [&hasWrittenResponse](){hasWrittenResponse = true;});
        auto dateHeader = []() -> std::string
        {
            IOChannelTest ioChannel;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            HttpBrokerPrivate brokerPrivate(&ioChannel, &parser);
            TestHttpBrokerPrivate broker(brokerPrivate);
            broker.doWriteDateHeader();
            std::string dateHeader(ioChannel.writeBuffer().peekAll());
            return dateHeader;
        }();
        const auto statusCodeAndLine = GENERATE(AS(std::pair<HttpStatusCode, std::string_view>),
                                                {HttpStatusCode::Continue, "HTTP/1.1 100 Continue\r\n"}, // 100,
                                                {HttpStatusCode::SwitchingProtocols, "HTTP/1.1 101 Switching Protocols\r\n"}, // = 101,
                                                {HttpStatusCode::OK, "HTTP/1.1 200 OK\r\n"}, // = 200,
                                                {HttpStatusCode::Created, "HTTP/1.1 201 Created\r\n"}, // = 201,
                                                {HttpStatusCode::Accepted, "HTTP/1.1 202 Accepted\r\n"}, // = 202,
                                                {HttpStatusCode::NonAuthoritativeInformation, "HTTP/1.1 203 Non-Authoritative Information\r\n"}, // = 203,
                                                {HttpStatusCode::NoContent, "HTTP/1.1 204 No Content\r\n"}, // = 204,
                                                {HttpStatusCode::ResetContent, "HTTP/1.1 205 Reset Content\r\n"}, // = 205,
                                                {HttpStatusCode::PartialContent, "HTTP/1.1 206 Partial Content\r\n"}, // = 206,
                                                {HttpStatusCode::MultipleChoices, "HTTP/1.1 300 Multiple Choices\r\n"}, // = 300,
                                                {HttpStatusCode::MovedPermanently, "HTTP/1.1 301 Moved Permanently\r\n"}, // = 301,
                                                {HttpStatusCode::Found, "HTTP/1.1 302 Found\r\n"}, // = 302,
                                                {HttpStatusCode::SeeOther, "HTTP/1.1 303 See Other\r\n"}, // = 303,
                                                {HttpStatusCode::NotModified, "HTTP/1.1 304 Not Modified\r\n"}, // = 304,
                                                {HttpStatusCode::UseProxy, "HTTP/1.1 305 Use Proxy\r\n"}, // = 305,
                                                {HttpStatusCode::TemporaryRedirect, "HTTP/1.1 307 Temporary Redirect\r\n"}, // = 307,
                                                {HttpStatusCode::PermanentRedirect, "HTTP/1.1 308 Permanent Redirect\r\n"}, // = 308,
                                                {HttpStatusCode::BadRequest, "HTTP/1.1 400 Bad Request\r\n"}, // = 400,
                                                {HttpStatusCode::Unauthorized, "HTTP/1.1 401 Unauthorized\r\n"}, // = 401,
                                                {HttpStatusCode::PaymentRequired, "HTTP/1.1 402 Payment Required\r\n"}, // = 402,
                                                {HttpStatusCode::Forbidden, "HTTP/1.1 403 Forbidden\r\n"}, // = 403,
                                                {HttpStatusCode::NotFound, "HTTP/1.1 404 Not Found\r\n"}, // = 404,
                                                {HttpStatusCode::MethodNotAllowed, "HTTP/1.1 405 Method Not Allowed\r\n"}, // = 405,
                                                {HttpStatusCode::NotAcceptable, "HTTP/1.1 406 Not Acceptable\r\n"}, // = 406,
                                                {HttpStatusCode::ProxyAuthenticationRequired, "HTTP/1.1 407 Proxy Authentication Required\r\n"}, // = 407,
                                                {HttpStatusCode::RequestTimeout, "HTTP/1.1 408 Request Timeout\r\n"}, // = 408,
                                                {HttpStatusCode::Conflict, "HTTP/1.1 409 Conflict\r\n"}, // = 409,
                                                {HttpStatusCode::Gone, "HTTP/1.1 410 Gone\r\n"}, // = 410,
                                                {HttpStatusCode::LengthRequired, "HTTP/1.1 411 Length Required\r\n"}, // = 411,
                                                {HttpStatusCode::PreconditionFailed, "HTTP/1.1 412 Precondition Failed\r\n"}, // = 412,
                                                {HttpStatusCode::ContentTooLarge, "HTTP/1.1 413 Content Too Large\r\n"}, // = 413,
                                                {HttpStatusCode::URITooLong, "HTTP/1.1 414 URI Too Long\r\n"}, // = 414,
                                                {HttpStatusCode::UnsupportedMediaType, "HTTP/1.1 415 Unsupported Media Type\r\n"}, // = 415,
                                                {HttpStatusCode::RangeNotSatisfiable, "HTTP/1.1 416 Range Not Satisfiable\r\n"}, // = 416,
                                                {HttpStatusCode::ExpectationFailed, "HTTP/1.1 417 Expectation Failed\r\n"}, // = 417,
                                                {HttpStatusCode::MisdirectedRequest, "HTTP/1.1 421 Misdirected Request\r\n"}, // = 421,
                                                {HttpStatusCode::UnprocessableContent, "HTTP/1.1 422 Unprocessable Content\r\n"}, // = 422,
                                                {HttpStatusCode::UpgradeRequired, "HTTP/1.1 426 Upgrade Required\r\n"}, // = 426,
                                                {HttpStatusCode::InternalServerError, "HTTP/1.1 500 Internal Server Error\r\n"}, // = 500,
                                                {HttpStatusCode::NotImplemented, "HTTP/1.1 501 Not Implemented\r\n"}, // = 501,
                                                {HttpStatusCode::BadGateway, "HTTP/1.1 502 Bad Gateway\r\n"}, // = 502,
                                                {HttpStatusCode::ServiceUnavailable, "HTTP/1.1 503 Service Unavailable\r\n"}, // = 503,
                                                {HttpStatusCode::GatewayTimeout, "HTTP/1.1 504 Gateway Timeout\r\n"}, // = 504,
                                                {HttpStatusCode::HTTPVersionNotSupported, "HTTP/1.1 505 HTTP Version Not Supported\r\n"});

        WHEN("a response for given status code is written without user-provided headers and body")
        {
            REQUIRE(!hasWrittenResponse);
            brokerPrivate.writeResponse(statusCodeAndLine.first);
            REQUIRE(hasWrittenResponse);
            const auto writtenResponse = ioChannel.writeBuffer().peekAll();

            THEN("broker writes response")
            {
                std::string expectedResponse;
                expectedResponse.reserve(128);
                expectedResponse.append(statusCodeAndLine.second)
                    .append("Server: Kourier\r\n")
                    .append(dateHeader)
                    .append("Content-Length: 0\r\n")
                    .append("\r\n");
                REQUIRE(writtenResponse == expectedResponse);
            }
        }

        WHEN("a response for given status code is written with user-provided headers as initializer list and no body")
        {
            const auto headers = GENERATE(AS(std::list<std::pair<std::string, std::string>>),
                                          {{"name", "value"}},
                                          {{"name1", "value1"}, {"name2", "value2"}});
            REQUIRE(!hasWrittenResponse);
            switch (headers.size())
            {
                case 1:
                    brokerPrivate.writeResponse(statusCodeAndLine.first, {{"name", "value"}});
                    break;
                case 2:
                    brokerPrivate.writeResponse(statusCodeAndLine.first, {{"name1", "value1"}, {"name2", "value2"}});
                    break;
                default:
                    FAIL("This code is supposed to be unreachable");
            }
            REQUIRE(hasWrittenResponse);
            const auto writtenResponse = ioChannel.writeBuffer().peekAll();

            THEN("broker writes response")
            {
                std::string expectedResponse;
                expectedResponse.reserve(128);
                expectedResponse.append(statusCodeAndLine.second)
                    .append("Server: Kourier\r\n")
                    .append(dateHeader)
                    .append("Content-Length: 0\r\n");
                for (auto &[name, value] : headers)
                    expectedResponse.append(name).append(": ").append(value).append("\r\n");
                expectedResponse.append("\r\n");
                REQUIRE(writtenResponse == expectedResponse);
            }
        }

        WHEN("a response for given status code is written with user-provided headers as vector and no body")
        {
            const auto headers = GENERATE(AS(std::vector<std::pair<std::string, std::string>>),
                                          {{"name", "value"}},
                                          {{"name1", "value1"}, {"name2", "value2"}});
            REQUIRE(!hasWrittenResponse);
            brokerPrivate.writeResponse(statusCodeAndLine.first, headers);
            REQUIRE(hasWrittenResponse);
            const auto writtenResponse = ioChannel.writeBuffer().peekAll();

            THEN("broker writes response")
            {
                std::string expectedResponse;
                expectedResponse.reserve(128);
                expectedResponse.append(statusCodeAndLine.second)
                    .append("Server: Kourier\r\n")
                    .append(dateHeader)
                    .append("Content-Length: 0\r\n");
                for (auto &[name, value] : headers)
                    expectedResponse.append(name).append(": ").append(value).append("\r\n");
                expectedResponse.append("\r\n");
                REQUIRE(writtenResponse == expectedResponse);
            }
        }

        WHEN("a response for given status code is written with user-provided headers as initializer list and body")
        {
            const auto headers = GENERATE(AS(std::list<std::pair<std::string, std::string>>),
                                          {{"name", "value"}},
                                          {{"name1", "value1"}, {"name2", "value2"}});
            const auto body = GENERATE(AS(std::pair<std::string_view, std::string_view>),
                                       {"Hello", ""},
                                       {"Hello", "text/plain"},
                                       {"Hello", "application/octet-stream"},
                                       {"Hello World!", ""},
                                       {"Hello World!", "text/plain"},
                                       {"Hello World!", "application/octet-stream"},
                                       {"", ""});
            REQUIRE(!hasWrittenResponse);
            if (body.second.empty())
            {
                switch (headers.size())
                {
                    case 1:
                        brokerPrivate.writeResponse(body.first, statusCodeAndLine.first, {{"name", "value"}});
                        break;
                    case 2:
                        brokerPrivate.writeResponse(body.first, statusCodeAndLine.first, {{"name1", "value1"}, {"name2", "value2"}});
                        break;
                    default:
                        FAIL("This code is supposed to be unreachable.");
                }
            }
            else
            {
                switch (headers.size())
                {
                    case 1:
                        brokerPrivate.writeResponse(body.first, body.second, statusCodeAndLine.first, {{"name", "value"}});
                        break;
                    case 2:
                        brokerPrivate.writeResponse(body.first, body.second, statusCodeAndLine.first, {{"name1", "value1"}, {"name2", "value2"}});
                        break;
                    default:
                        FAIL("This code is supposed to be unreachable.");
                }
            }
            REQUIRE(hasWrittenResponse);
            const auto writtenResponse = ioChannel.writeBuffer().peekAll();

            THEN("broker writes response")
            {
                std::string expectedResponse;
                expectedResponse.reserve(128);
                expectedResponse.append(statusCodeAndLine.second)
                    .append("Server: Kourier\r\n")
                    .append(dateHeader);
                if (!body.first.empty())
                    expectedResponse.append("Content-Length: ").append(std::to_string(body.first.size())).append("\r\n");
                else
                    expectedResponse.append("Content-Length: 0\r\n");
                if (!body.second.empty())
                    expectedResponse.append("Content-Type: ").append(body.second).append("\r\n");
                for (auto &[name, value] : headers)
                    expectedResponse.append(name).append(": ").append(value).append("\r\n");
                expectedResponse.append("\r\n");
                expectedResponse.append(body.first);
                REQUIRE(writtenResponse == expectedResponse);
            }
        }

        WHEN("a response for given status code is written with user-provided headers as vector and body")
        {
            const auto headers = GENERATE(AS(std::vector<std::pair<std::string, std::string>>),
                                          {{"name", "value"}},
                                          {{"name1", "value1"}, {"name2", "value2"}});
            const auto body = GENERATE(AS(std::pair<std::string_view, std::string_view>),
                                       {"Hello", ""},
                                       {"Hello", "text/plain"},
                                       {"Hello", "application/octet-stream"},
                                       {"Hello World!", ""},
                                       {"Hello World!", "text/plain"},
                                       {"Hello World!", "application/octet-stream"},
                                       {"", ""});
            REQUIRE(!hasWrittenResponse);
            if (body.second.empty())
                brokerPrivate.writeResponse(body.first, statusCodeAndLine.first, headers);
            else
                brokerPrivate.writeResponse(body.first, body.second, statusCodeAndLine.first, headers);
            REQUIRE(hasWrittenResponse);
            const auto writtenResponse = ioChannel.writeBuffer().peekAll();

            THEN("broker writes response")
            {
                std::string expectedResponse;
                expectedResponse.reserve(128);
                expectedResponse.append(statusCodeAndLine.second)
                    .append("Server: Kourier\r\n")
                    .append(dateHeader);
                if (!body.first.empty())
                    expectedResponse.append("Content-Length: ").append(std::to_string(body.first.size())).append("\r\n");
                else
                    expectedResponse.append("Content-Length: 0\r\n");
                if (!body.second.empty())
                    expectedResponse.append("Content-Type: ").append(body.second).append("\r\n");
                for (auto &[name, value] : headers)
                    expectedResponse.append(name).append(": ").append(value).append("\r\n");
                expectedResponse.append("\r\n");
                expectedResponse.append(body.first);
                REQUIRE(writtenResponse == expectedResponse);
            }
        }
    }
}


SCENARIO("HttpServerBrokers knows how to write chunked responses")
{
    GIVEN("a private broker and a status code")
    {
        IOChannelTest ioChannel;
        HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
        HttpBrokerPrivate brokerPrivate(&ioChannel, &parser);
        bool hasWrittenResponse = false;
        Object::connect(&brokerPrivate, &HttpBrokerPrivate::wroteResponse, [&hasWrittenResponse](){hasWrittenResponse = true;});
        auto dateHeader = []() -> std::string
        {
            IOChannelTest ioChannel;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            HttpBrokerPrivate brokerPrivate(&ioChannel, &parser);
            TestHttpBrokerPrivate broker(brokerPrivate);
            broker.doWriteDateHeader();
            std::string dateHeader(ioChannel.writeBuffer().peekAll());
            return dateHeader;
        }();
        const auto statusCodeAndLine = GENERATE(AS(std::pair<HttpStatusCode, std::string_view>),
                                                {HttpStatusCode::Continue, "HTTP/1.1 100 Continue\r\n"}, // 100,
                                                {HttpStatusCode::SwitchingProtocols, "HTTP/1.1 101 Switching Protocols\r\n"}, // = 101,
                                                {HttpStatusCode::OK, "HTTP/1.1 200 OK\r\n"}, // = 200,
                                                {HttpStatusCode::Created, "HTTP/1.1 201 Created\r\n"}, // = 201,
                                                {HttpStatusCode::Accepted, "HTTP/1.1 202 Accepted\r\n"}, // = 202,
                                                {HttpStatusCode::NonAuthoritativeInformation, "HTTP/1.1 203 Non-Authoritative Information\r\n"}, // = 203,
                                                {HttpStatusCode::NoContent, "HTTP/1.1 204 No Content\r\n"}, // = 204,
                                                {HttpStatusCode::ResetContent, "HTTP/1.1 205 Reset Content\r\n"}, // = 205,
                                                {HttpStatusCode::PartialContent, "HTTP/1.1 206 Partial Content\r\n"}, // = 206,
                                                {HttpStatusCode::MultipleChoices, "HTTP/1.1 300 Multiple Choices\r\n"}, // = 300,
                                                {HttpStatusCode::MovedPermanently, "HTTP/1.1 301 Moved Permanently\r\n"}, // = 301,
                                                {HttpStatusCode::Found, "HTTP/1.1 302 Found\r\n"}, // = 302,
                                                {HttpStatusCode::SeeOther, "HTTP/1.1 303 See Other\r\n"}, // = 303,
                                                {HttpStatusCode::NotModified, "HTTP/1.1 304 Not Modified\r\n"}, // = 304,
                                                {HttpStatusCode::UseProxy, "HTTP/1.1 305 Use Proxy\r\n"}, // = 305,
                                                {HttpStatusCode::TemporaryRedirect, "HTTP/1.1 307 Temporary Redirect\r\n"}, // = 307,
                                                {HttpStatusCode::PermanentRedirect, "HTTP/1.1 308 Permanent Redirect\r\n"}, // = 308,
                                                {HttpStatusCode::BadRequest, "HTTP/1.1 400 Bad Request\r\n"}, // = 400,
                                                {HttpStatusCode::Unauthorized, "HTTP/1.1 401 Unauthorized\r\n"}, // = 401,
                                                {HttpStatusCode::PaymentRequired, "HTTP/1.1 402 Payment Required\r\n"}, // = 402,
                                                {HttpStatusCode::Forbidden, "HTTP/1.1 403 Forbidden\r\n"}, // = 403,
                                                {HttpStatusCode::NotFound, "HTTP/1.1 404 Not Found\r\n"}, // = 404,
                                                {HttpStatusCode::MethodNotAllowed, "HTTP/1.1 405 Method Not Allowed\r\n"}, // = 405,
                                                {HttpStatusCode::NotAcceptable, "HTTP/1.1 406 Not Acceptable\r\n"}, // = 406,
                                                {HttpStatusCode::ProxyAuthenticationRequired, "HTTP/1.1 407 Proxy Authentication Required\r\n"}, // = 407,
                                                {HttpStatusCode::RequestTimeout, "HTTP/1.1 408 Request Timeout\r\n"}, // = 408,
                                                {HttpStatusCode::Conflict, "HTTP/1.1 409 Conflict\r\n"}, // = 409,
                                                {HttpStatusCode::Gone, "HTTP/1.1 410 Gone\r\n"}, // = 410,
                                                {HttpStatusCode::LengthRequired, "HTTP/1.1 411 Length Required\r\n"}, // = 411,
                                                {HttpStatusCode::PreconditionFailed, "HTTP/1.1 412 Precondition Failed\r\n"}, // = 412,
                                                {HttpStatusCode::ContentTooLarge, "HTTP/1.1 413 Content Too Large\r\n"}, // = 413,
                                                {HttpStatusCode::URITooLong, "HTTP/1.1 414 URI Too Long\r\n"}, // = 414,
                                                {HttpStatusCode::UnsupportedMediaType, "HTTP/1.1 415 Unsupported Media Type\r\n"}, // = 415,
                                                {HttpStatusCode::RangeNotSatisfiable, "HTTP/1.1 416 Range Not Satisfiable\r\n"}, // = 416,
                                                {HttpStatusCode::ExpectationFailed, "HTTP/1.1 417 Expectation Failed\r\n"}, // = 417,
                                                {HttpStatusCode::MisdirectedRequest, "HTTP/1.1 421 Misdirected Request\r\n"}, // = 421,
                                                {HttpStatusCode::UnprocessableContent, "HTTP/1.1 422 Unprocessable Content\r\n"}, // = 422,
                                                {HttpStatusCode::UpgradeRequired, "HTTP/1.1 426 Upgrade Required\r\n"}, // = 426,
                                                {HttpStatusCode::InternalServerError, "HTTP/1.1 500 Internal Server Error\r\n"}, // = 500,
                                                {HttpStatusCode::NotImplemented, "HTTP/1.1 501 Not Implemented\r\n"}, // = 501,
                                                {HttpStatusCode::BadGateway, "HTTP/1.1 502 Bad Gateway\r\n"}, // = 502,
                                                {HttpStatusCode::ServiceUnavailable, "HTTP/1.1 503 Service Unavailable\r\n"}, // = 503,
                                                {HttpStatusCode::GatewayTimeout, "HTTP/1.1 504 Gateway Timeout\r\n"}, // = 504,
                                                {HttpStatusCode::HTTPVersionNotSupported, "HTTP/1.1 505 HTTP Version Not Supported\r\n"});

        WHEN("a chunked response for given status code is written with user-provided headers/trailer names/trailers as initializer list")
        {
            const auto headers = GENERATE(AS(std::list<std::pair<std::string, std::string>>),
                                          {{"name", "value"}},
                                          {{"name1", "value1"}, {"name2", "value2"}});
            const auto trailerNames = GENERATE(AS(std::list<std::string>),
                                           {},
                                           {"trailer_name"},
                                           {"trailer_name1", "trailer_name2"});
            const auto trailers = GENERATE(AS(std::list<std::pair<std::string, std::string>>),
                                               {},
                                               {{"trailer_name", "trailer_value"}},
                                               {{"trailer_name1", "trailer_value1"}, {"trailer_name2", "trailer_value2"}});
            const auto mimeType = GENERATE(AS(std::string_view), "", "text/plain", "application/octet-stream");
            const auto chunks = GENERATE(AS(std::vector<std::string_view>),
                                         {},
                                         {"Hello"},
                                         {"Hello", "World!"});
            REQUIRE(!hasWrittenResponse);
            switch (headers.size())
            {
                case 1:
                    switch (trailerNames.size())
                    {
                        case 0:
                            if (mimeType.empty())
                                brokerPrivate.writeChunkedResponse(statusCodeAndLine.first, {{"name", "value"}}, {});
                            else
                                brokerPrivate.writeChunkedResponse(mimeType, statusCodeAndLine.first, {{"name", "value"}}, {});
                            break;
                        case 1:
                            if (mimeType.empty())
                                brokerPrivate.writeChunkedResponse(statusCodeAndLine.first, {{"name", "value"}}, {"trailer_name"});
                            else
                                brokerPrivate.writeChunkedResponse(mimeType, statusCodeAndLine.first, {{"name", "value"}}, {"trailer_name"});
                            break;
                        case 2:
                            if (mimeType.empty())
                                brokerPrivate.writeChunkedResponse(statusCodeAndLine.first, {{"name", "value"}}, {"trailer_name1", "trailer_name2"});
                            else
                                brokerPrivate.writeChunkedResponse(mimeType, statusCodeAndLine.first, {{"name", "value"}}, {"trailer_name1", "trailer_name2"});
                            break;
                        default:
                            FAIL("This code is supposed to be unreachable.");
                    }
                    break;
                case 2:
                    switch (trailerNames.size())
                    {
                        case 0:
                            if (mimeType.empty())
                                brokerPrivate.writeChunkedResponse(statusCodeAndLine.first, {{"name1", "value1"}, {"name2", "value2"}}, {});
                            else
                                brokerPrivate.writeChunkedResponse(mimeType, statusCodeAndLine.first, {{"name1", "value1"}, {"name2", "value2"}}, {});
                            break;
                        case 1:
                            if (mimeType.empty())
                                brokerPrivate.writeChunkedResponse(statusCodeAndLine.first, {{"name1", "value1"}, {"name2", "value2"}}, {"trailer_name"});
                            else
                                brokerPrivate.writeChunkedResponse(mimeType, statusCodeAndLine.first, {{"name1", "value1"}, {"name2", "value2"}}, {"trailer_name"});
                            break;
                        case 2:
                            if (mimeType.empty())
                                brokerPrivate.writeChunkedResponse(statusCodeAndLine.first, {{"name1", "value1"}, {"name2", "value2"}}, {"trailer_name1", "trailer_name2"});
                            else
                                brokerPrivate.writeChunkedResponse(mimeType, statusCodeAndLine.first, {{"name1", "value1"}, {"name2", "value2"}}, {"trailer_name1", "trailer_name2"});
                            break;
                        default:
                            FAIL("This code is supposed to be unreachable.");
                    }
                    break;
                default:
                    FAIL("This code is supposed to be unreachable.");
            }
            REQUIRE(!hasWrittenResponse);

            THEN("broker writes response up to headers")
            {
                const auto writtenResponse = ioChannel.writeBuffer().peekAll();
                std::string expectedResponse;
                expectedResponse.reserve(128);
                expectedResponse.append(statusCodeAndLine.second)
                    .append("Server: Kourier\r\n")
                    .append(dateHeader);
                if (!mimeType.empty())
                    expectedResponse.append("Content-Type: ").append(mimeType).append("\r\n");
                expectedResponse.append("Transfer-Encoding: chunked\r\n");
                auto it = trailerNames.begin();
                if (it != trailerNames.end())
                {
                    expectedResponse.append("Trailer: ").append(*it);
                    while (++it != trailerNames.end())
                        expectedResponse.append(", ").append(*it);
                    expectedResponse.append("\r\n");
                }
                for (auto &[name, value] : headers)
                    expectedResponse.append(name).append(": ").append(value).append("\r\n");
                expectedResponse.append("\r\n");
                REQUIRE(writtenResponse == expectedResponse);

                AND_WHEN("chunks are written up to the last one and trailers")
                {
                    for (const auto &chunk : chunks)
                    {
                        brokerPrivate.writeChunk(chunk);
                        REQUIRE(!hasWrittenResponse);
                        if (!chunk.empty())
                            expectedResponse.append(std::to_string(chunk.size())).append("\r\n").append(chunk).append("\r\n");
                        const auto writtenResponse = ioChannel.writeBuffer().peekAll();
                        REQUIRE(writtenResponse == expectedResponse);
                    }
                    if (trailers.size() > 0)
                        switch (trailers.size())
                        {
                            case 1:
                                brokerPrivate.writeLastChunk({{"trailer_name", "trailer_value"}});
                                break;
                            case 2:
                                brokerPrivate.writeLastChunk({{"trailer_name1", "trailer_value1"}, {"trailer_name2", "trailer_value2"}});
                                break;
                            default:
                                FAIL("This code is supposed to be unreachable.");
                        }
                    else
                        brokerPrivate.writeLastChunk();
                    REQUIRE(hasWrittenResponse);
                    expectedResponse.append("0\r\n");
                    for (const auto &trailer : trailers)
                        expectedResponse.append(trailer.first).append(": ").append(trailer.second).append("\r\n");
                    expectedResponse.append("\r\n");

                    THEN("broker writes chunked response")
                    {
                        const auto writtenResponse = ioChannel.writeBuffer().peekAll();
                        REQUIRE(writtenResponse == expectedResponse);
                    }
                }
            }
        }

        WHEN("a chunked response for given status code is written with user-provided headers/trailer names/trailers as vector")
        {
            const auto headers = GENERATE(AS(std::vector<std::pair<std::string, std::string>>),
                                          {{"name", "value"}},
                                          {{"name1", "value1"}, {"name2", "value2"}});
            const auto trailerNames = GENERATE(AS(std::vector<std::string>),
                                               {},
                                               {"trailer_name"},
                                               {"trailer_name1", "trailer_name2"});
            const auto trailers = GENERATE(AS(std::vector<std::pair<std::string, std::string>>),
                                           {},
                                           {{"trailer_name", "trailer_value"}},
                                           {{"trailer_name1", "trailer_value1"}, {"trailer_name2", "trailer_value2"}});
            const auto mimeType = GENERATE(AS(std::string_view), "", "text/plain", "application/octet-stream");
            const auto chunks = GENERATE(AS(std::vector<std::string_view>),
                                         {},
                                         {"Hello"},
                                         {"Hello", "World!"});
            REQUIRE(!hasWrittenResponse);
            if (mimeType.empty())
                brokerPrivate.writeChunkedResponse(statusCodeAndLine.first, headers, trailerNames);
            else
                brokerPrivate.writeChunkedResponse(mimeType, statusCodeAndLine.first, headers, trailerNames);
            REQUIRE(!hasWrittenResponse);

            THEN("broker writes response up to headers")
            {
                const auto writtenResponse = ioChannel.writeBuffer().peekAll();
                std::string expectedResponse;
                expectedResponse.reserve(128);
                expectedResponse.append(statusCodeAndLine.second)
                    .append("Server: Kourier\r\n")
                    .append(dateHeader);
                if (!mimeType.empty())
                    expectedResponse.append("Content-Type: ").append(mimeType).append("\r\n");
                expectedResponse.append("Transfer-Encoding: chunked\r\n");
                auto it = trailerNames.begin();
                if (it != trailerNames.end())
                {
                    expectedResponse.append("Trailer: ").append(*it);
                    while (++it != trailerNames.end())
                        expectedResponse.append(", ").append(*it);
                    expectedResponse.append("\r\n");
                }
                for (auto &[name, value] : headers)
                    expectedResponse.append(name).append(": ").append(value).append("\r\n");
                expectedResponse.append("\r\n");
                REQUIRE(writtenResponse == expectedResponse);

                AND_WHEN("chunks are written up to the last one and trailers")
                {
                    for (const auto &chunk : chunks)
                    {
                        brokerPrivate.writeChunk(chunk);
                        REQUIRE(!hasWrittenResponse);
                        if (!chunk.empty())
                            expectedResponse.append(std::to_string(chunk.size())).append("\r\n").append(chunk).append("\r\n");
                        const auto writtenResponse = ioChannel.writeBuffer().peekAll();
                        REQUIRE(writtenResponse == expectedResponse);
                    }
                    if (trailers.size() > 0)
                        brokerPrivate.writeLastChunk(trailers);
                    else
                        brokerPrivate.writeLastChunk();
                    REQUIRE(hasWrittenResponse);
                    expectedResponse.append("0\r\n");
                    for (const auto &trailer : trailers)
                        expectedResponse.append(trailer.first).append(": ").append(trailer.second).append("\r\n");
                    expectedResponse.append("\r\n");

                    THEN("broker writes chunked response")
                    {
                        const auto writtenResponse = ioChannel.writeBuffer().peekAll();
                        REQUIRE(writtenResponse == expectedResponse);
                    }
                }
            }
        }
    }
}


SCENARIO("HttpServerBrokers terminates current chunked response and refuses to begin writing another response")
{
    GIVEN("a private broker")
    {
        IOChannelTest ioChannel;
        HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
        HttpBrokerPrivate brokerPrivate(&ioChannel, &parser);
        size_t wroteResponseEmissionCounter = 0;
        Object::connect(&brokerPrivate, &HttpBrokerPrivate::wroteResponse, [&wroteResponseEmissionCounter](){++wroteResponseEmissionCounter;});
        auto dateHeader = []() -> std::string
        {
            IOChannelTest ioChannel;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            HttpBrokerPrivate brokerPrivate(&ioChannel, &parser);
            TestHttpBrokerPrivate broker(brokerPrivate);
            broker.doWriteDateHeader();
            std::string dateHeader(ioChannel.writeBuffer().peekAll());
            return dateHeader;
        }();

        WHEN("a chunked response is written")
        {
            REQUIRE(wroteResponseEmissionCounter == 0);
            brokerPrivate.writeChunkedResponse();
            REQUIRE(wroteResponseEmissionCounter == 0);

            THEN("broker writes chunk response up to headers")
            {
                const auto writtenResponse = ioChannel.writeBuffer().peekAll();
                std::string expectedResponse;
                expectedResponse.reserve(128);
                expectedResponse.append("HTTP/1.1 200 OK\r\n")
                    .append("Server: Kourier\r\n")
                    .append(dateHeader)
                    .append("Transfer-Encoding: chunked\r\n\r\n");
                REQUIRE(writtenResponse == expectedResponse);

                AND_WHEN("another response is written")
                {
                    brokerPrivate.writeResponse();
                    const auto writtenResponse = ioChannel.writeBuffer().peekAll();

                    THEN("broker finishes current chunked response and does not write another response")
                    {
                        REQUIRE(wroteResponseEmissionCounter == 1);
                        expectedResponse.append("0\r\n\r\n");
                        REQUIRE(writtenResponse == expectedResponse);
                    }
                }

                AND_WHEN("another chunked response is written")
                {
                    brokerPrivate.writeChunkedResponse();
                    const auto writtenResponse = ioChannel.writeBuffer().peekAll();

                    THEN("broker finishes current chunked response and does not start to write chunked response")
                    {
                        REQUIRE(wroteResponseEmissionCounter == 1);
                        expectedResponse.append("0\r\n\r\n");
                        REQUIRE(writtenResponse == expectedResponse);
                    }
                }

                AND_WHEN("a chunk is written")
                {
                    std::string_view chunkData("Hello World!");
                    brokerPrivate.writeChunk(chunkData);
                    REQUIRE(wroteResponseEmissionCounter == 0);

                    THEN("chunk is written")
                    {
                        expectedResponse.append("c\r\n")
                            .append(chunkData)
                            .append("\r\n");

                        AND_WHEN("another response is written")
                        {
                            brokerPrivate.writeResponse();
                            const auto writtenResponse = ioChannel.writeBuffer().peekAll();

                            THEN("broker finishes current chunked response and does not write another response")
                            {
                                REQUIRE(wroteResponseEmissionCounter == 1);
                                expectedResponse.append("0\r\n\r\n");
                                REQUIRE(writtenResponse == expectedResponse);
                            }
                        }

                        AND_WHEN("another chunked response is written")
                        {
                            brokerPrivate.writeChunkedResponse();
                            const auto writtenResponse = ioChannel.writeBuffer().peekAll();

                            THEN("broker finishes current chunked response and does not begin writing chunked response")
                            {
                                REQUIRE(wroteResponseEmissionCounter == 1);
                                expectedResponse.append("0\r\n\r\n");
                                REQUIRE(writtenResponse == expectedResponse);
                            }
                        }
                    }
                }
            }
        }
    }
}


SCENARIO("HttpServerBrokers only writes chunks when a chunked response is being written")
{
    GIVEN("a private broker")
    {
        IOChannelTest ioChannel;
        HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
        HttpBrokerPrivate brokerPrivate(&ioChannel, &parser);
        bool hasWrittenResponse = false;
        Object::connect(&brokerPrivate, &HttpBrokerPrivate::wroteResponse, [&hasWrittenResponse](){hasWrittenResponse = true;});

        WHEN("chunk data is written")
        {
            REQUIRE(!hasWrittenResponse);
            brokerPrivate.writeChunk("Hello World!");
            REQUIRE(!hasWrittenResponse);

            THEN("nothing is written")
            {
                REQUIRE(ioChannel.writeBuffer().isEmpty());
            }
        }

        WHEN("last chunk is written")
        {
            REQUIRE(!hasWrittenResponse);
            brokerPrivate.writeLastChunk();
            REQUIRE(!hasWrittenResponse);

            THEN("nothing is written")
            {
                REQUIRE(ioChannel.writeBuffer().isEmpty());
            }
        }
    }
}


SCENARIO("HttpServerBrokers only writes non-empty chunk data")
{
    GIVEN("a private broker")
    {
        IOChannelTest ioChannel;
        HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
        HttpBrokerPrivate brokerPrivate(&ioChannel, &parser);
        size_t wroteResponseEmissionCounter = 0;
        Object::connect(&brokerPrivate, &HttpBrokerPrivate::wroteResponse, [&wroteResponseEmissionCounter](){++wroteResponseEmissionCounter;});
        auto dateHeader = []() -> std::string
        {
            IOChannelTest ioChannel;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            HttpBrokerPrivate brokerPrivate(&ioChannel, &parser);
            TestHttpBrokerPrivate broker(brokerPrivate);
            broker.doWriteDateHeader();
            std::string dateHeader(ioChannel.writeBuffer().peekAll());
            return dateHeader;
        }();

        WHEN("a chunked response is written")
        {
            REQUIRE(wroteResponseEmissionCounter == 0);
            brokerPrivate.writeChunkedResponse();
            REQUIRE(wroteResponseEmissionCounter == 0);

            THEN("broker writes chunk response up to headers")
            {
                const auto writtenResponse = ioChannel.writeBuffer().peekAll();
                std::string expectedResponse;
                expectedResponse.reserve(128);
                expectedResponse.append("HTTP/1.1 200 OK\r\n")
                    .append("Server: Kourier\r\n")
                    .append(dateHeader)
                    .append("Transfer-Encoding: chunked\r\n\r\n");
                REQUIRE(writtenResponse == expectedResponse);

                AND_WHEN("a non-empty chunk data is written")
                {
                    std::string_view chunkData("Hello World!");
                    brokerPrivate.writeChunk(chunkData);
                    const auto writtenResponse = ioChannel.writeBuffer().peekAll();
                    REQUIRE(wroteResponseEmissionCounter == 0);

                    THEN("chunk is written")
                    {
                        expectedResponse.append("c\r\n")
                        .append(chunkData)
                            .append("\r\n");
                        REQUIRE(writtenResponse == expectedResponse);

                        AND_WHEN("an empty chunk is written")
                        {
                            const auto chunkData = GENERATE(AS(std::string_view), {}, "");
                            brokerPrivate.writeChunk(chunkData);

                            THEN("nothing is written")
                            {
                                REQUIRE(writtenResponse == expectedResponse);
                            }
                        }
                    }
                }

                AND_WHEN("an empty chunk is written")
                {
                    const auto chunkData = GENERATE(AS(std::string_view), {}, "");
                    brokerPrivate.writeChunk(chunkData);

                    THEN("nothing is written")
                    {
                        REQUIRE(writtenResponse == expectedResponse);
                    }
                }
            }
        }
    }
}


SCENARIO("HttpBrokerPrivate must be reset before writing next response")
{
    GIVEN("a private broker")
    {
        IOChannelTest ioChannel;
        HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
        HttpBrokerPrivate brokerPrivate(&ioChannel, &parser);
        size_t wroteResponseEmissionCounter = 0;
        Object::connect(&brokerPrivate, &HttpBrokerPrivate::wroteResponse, [&wroteResponseEmissionCounter](){++wroteResponseEmissionCounter;});
        auto dateHeader = []() -> std::string
        {
            IOChannelTest ioChannel;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            HttpBrokerPrivate brokerPrivate(&ioChannel, &parser);
            TestHttpBrokerPrivate broker(brokerPrivate);
            broker.doWriteDateHeader();
            std::string dateHeader(ioChannel.writeBuffer().peekAll());
            return dateHeader;
        }();

        WHEN("a response is written")
        {
            REQUIRE(wroteResponseEmissionCounter == 0);
            brokerPrivate.writeResponse();
            REQUIRE(wroteResponseEmissionCounter == 1);

            THEN("broker writes response")
            {
                const auto writtenResponse = ioChannel.writeBuffer().peekAll();
                std::string expectedResponse;
                expectedResponse.reserve(128);
                expectedResponse.append("HTTP/1.1 200 OK\r\n")
                    .append("Server: Kourier\r\n")
                    .append(dateHeader)
                    .append("Content-Length: 0\r\n")
                    .append("\r\n");
                REQUIRE(writtenResponse == expectedResponse);

                AND_WHEN("another response is written")
                {
                    brokerPrivate.writeResponse();

                    THEN("broker refuses to write another response")
                    {
                        REQUIRE(wroteResponseEmissionCounter == 1);
                        REQUIRE(writtenResponse == expectedResponse);
                    }
                }

                AND_WHEN("another response is written after reseting broker")
                {
                    brokerPrivate.resetResponseWriting();
                    brokerPrivate.writeResponse();

                    THEN("broker writes response")
                    {
                        REQUIRE(wroteResponseEmissionCounter == 2);
                        expectedResponse.append("HTTP/1.1 200 OK\r\n")
                            .append("Server: Kourier\r\n")
                            .append(dateHeader)
                            .append("Content-Length: 0\r\n")
                            .append("\r\n");
                        const auto writtenResponse = ioChannel.writeBuffer().peekAll();
                        REQUIRE(writtenResponse == expectedResponse);
                    }
                }

                AND_WHEN("another chunked response is written")
                {
                    brokerPrivate.writeChunkedResponse();

                    THEN("broker refuses to write response")
                    {
                        REQUIRE(wroteResponseEmissionCounter == 1);
                        REQUIRE(writtenResponse == expectedResponse);
                    }
                }

                AND_WHEN("another chunked response is written after reseting broker")
                {
                    brokerPrivate.resetResponseWriting();
                    brokerPrivate.writeChunkedResponse();
                    REQUIRE(wroteResponseEmissionCounter == 1);
                    brokerPrivate.writeChunk("Hello");
                    REQUIRE(wroteResponseEmissionCounter == 1);
                    brokerPrivate.writeLastChunk();
                    REQUIRE(wroteResponseEmissionCounter == 2);

                    THEN("broker writes response")
                    {
                        expectedResponse.append("HTTP/1.1 200 OK\r\n")
                        .append("Server: Kourier\r\n")
                            .append(dateHeader)
                            .append("Transfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n");
                        const auto writtenResponse = ioChannel.writeBuffer().peekAll();
                        REQUIRE(writtenResponse == expectedResponse);
                    }
                }

                AND_WHEN("chunk data is written")
                {
                    brokerPrivate.writeChunk("Hello");

                    THEN("broker refuses to write response")
                    {
                        REQUIRE(wroteResponseEmissionCounter == 1);
                        REQUIRE(writtenResponse == expectedResponse);
                    }
                }

                AND_WHEN("last chunk is written")
                {
                    brokerPrivate.writeLastChunk();

                    THEN("broker refuses to write response")
                    {
                        REQUIRE(wroteResponseEmissionCounter == 1);
                        REQUIRE(writtenResponse == expectedResponse);
                    }
                }
            }
        }

        WHEN("a chunked response with no chunk data is written")
        {
            REQUIRE(wroteResponseEmissionCounter == 0);
            brokerPrivate.writeChunkedResponse();
            REQUIRE(wroteResponseEmissionCounter == 0);
            brokerPrivate.writeLastChunk();
            REQUIRE(wroteResponseEmissionCounter == 1);

            THEN("broker writes chunk response without any chunk data")
            {
                const auto writtenResponse = ioChannel.writeBuffer().peekAll();
                std::string expectedResponse;
                expectedResponse.reserve(128);
                expectedResponse.append("HTTP/1.1 200 OK\r\n")
                    .append("Server: Kourier\r\n")
                    .append(dateHeader)
                    .append("Transfer-Encoding: chunked\r\n\r\n0\r\n\r\n");
                REQUIRE(writtenResponse == expectedResponse);

                AND_WHEN("another response is written")
                {
                    brokerPrivate.writeResponse();

                    THEN("broker refuses to write response")
                    {
                        REQUIRE(wroteResponseEmissionCounter == 1);
                        REQUIRE(writtenResponse == expectedResponse);
                    }
                }

                AND_WHEN("another response is written after reseting broker")
                {
                    brokerPrivate.resetResponseWriting();
                    brokerPrivate.writeResponse();

                    THEN("broker writes response")
                    {
                        REQUIRE(wroteResponseEmissionCounter == 2);
                        expectedResponse.append("HTTP/1.1 200 OK\r\n")
                            .append("Server: Kourier\r\n")
                            .append(dateHeader)
                            .append("Content-Length: 0\r\n")
                            .append("\r\n");
                        const auto writtenResponse = ioChannel.writeBuffer().peekAll();
                        REQUIRE(writtenResponse == expectedResponse);
                    }
                }

                AND_WHEN("another chunked response is written")
                {
                    brokerPrivate.writeChunkedResponse();

                    THEN("broker refuses to write response")
                    {
                        REQUIRE(wroteResponseEmissionCounter == 1);
                        REQUIRE(writtenResponse == expectedResponse);
                    }
                }

                AND_WHEN("another chunked response is written after reseting broker")
                {
                    brokerPrivate.resetResponseWriting();
                    brokerPrivate.writeChunkedResponse();
                    REQUIRE(wroteResponseEmissionCounter == 1);
                    brokerPrivate.writeChunk("Hello");
                    REQUIRE(wroteResponseEmissionCounter == 1);
                    brokerPrivate.writeLastChunk();
                    REQUIRE(wroteResponseEmissionCounter == 2);

                    THEN("broker writes response")
                    {
                        expectedResponse.append("HTTP/1.1 200 OK\r\n")
                            .append("Server: Kourier\r\n")
                            .append(dateHeader)
                            .append("Transfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n");
                        const auto writtenResponse = ioChannel.writeBuffer().peekAll();
                        REQUIRE(writtenResponse == expectedResponse);
                    }
                }

                AND_WHEN("chunk data is written")
                {
                    brokerPrivate.writeChunk("Hello");

                    THEN("broker refuses to write response")
                    {
                        REQUIRE(wroteResponseEmissionCounter == 1);
                        REQUIRE(writtenResponse == expectedResponse);
                    }
                }

                AND_WHEN("last chunk is written")
                {
                    brokerPrivate.writeLastChunk();

                    THEN("broker refuses to write response")
                    {
                        REQUIRE(wroteResponseEmissionCounter == 1);
                        REQUIRE(writtenResponse == expectedResponse);
                    }
                }
            }
        }
    }
}


SCENARIO("HttpBrokerPrivate deletes any QObject set when reseting writer")
{
    GIVEN("a broker with a QObject set")
    {
        IOChannelTest ioChannel;
        HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
        HttpBrokerPrivate brokerPrivate(&ioChannel, &parser);
        auto *pObject = new QObject;
        bool hasDestroyedQObject = false;
        QObject::connect(pObject, &QObject::destroyed, [&](){hasDestroyedQObject = true;});
        brokerPrivate.setQObject(pObject);

        WHEN("a response is written")
        {
            QCoreApplication::sendPostedEvents(pObject, QEvent::DeferredDelete);
            REQUIRE(!hasDestroyedQObject);
            brokerPrivate.writeResponse();

            THEN("QObject is not deleted")
            {
                QCoreApplication::sendPostedEvents(pObject, QEvent::DeferredDelete);
                REQUIRE(!hasDestroyedQObject);

                AND_WHEN("broker is reset")
                {
                    brokerPrivate.resetResponseWriting();

                    THEN("set QObject is deleted when control return to event loop")
                    {
                        REQUIRE(!hasDestroyedQObject);
                        QCoreApplication::sendPostedEvents(pObject, QEvent::DeferredDelete);
                        REQUIRE(hasDestroyedQObject);
                    }
                }
            }
        }

        WHEN("a chunked response is written")
        {
            QCoreApplication::sendPostedEvents(pObject, QEvent::DeferredDelete);
            REQUIRE(!hasDestroyedQObject);
            brokerPrivate.writeChunkedResponse();
            QCoreApplication::sendPostedEvents(pObject, QEvent::DeferredDelete);
            REQUIRE(!hasDestroyedQObject);
            brokerPrivate.writeChunk("Hello World!");
            QCoreApplication::sendPostedEvents(pObject, QEvent::DeferredDelete);

            AND_WHEN("last chunk is written")
            {
                REQUIRE(!hasDestroyedQObject);
                brokerPrivate.writeLastChunk();

                THEN("QObject is not deleted")
                {
                    QCoreApplication::sendPostedEvents(pObject, QEvent::DeferredDelete);
                    REQUIRE(!hasDestroyedQObject);

                    AND_WHEN("broker is reset")
                    {
                        brokerPrivate.resetResponseWriting();

                        THEN("set QObject is deleted when control return to event loop")
                        {
                            REQUIRE(!hasDestroyedQObject);
                            QCoreApplication::sendPostedEvents(pObject, QEvent::DeferredDelete);
                            REQUIRE(hasDestroyedQObject);
                        }
                    }
                }
            }

            AND_WHEN("another response is written")
            {
                REQUIRE(!hasDestroyedQObject);
                brokerPrivate.writeResponse();

                THEN("broker refuses to write response and does not delete QObject when control return to event loop")
                {
                    QCoreApplication::sendPostedEvents(pObject, QEvent::DeferredDelete);
                    REQUIRE(!hasDestroyedQObject);

                    AND_WHEN("broker is reset")
                    {
                        brokerPrivate.resetResponseWriting();

                        THEN("set QObject is deleted when control return to event loop")
                        {
                            REQUIRE(!hasDestroyedQObject);
                            QCoreApplication::sendPostedEvents(pObject, QEvent::DeferredDelete);
                            REQUIRE(hasDestroyedQObject);
                        }
                    }
                }
            }

            AND_WHEN("a chunked response is written")
            {
                REQUIRE(!hasDestroyedQObject);
                brokerPrivate.writeChunkedResponse();

                THEN("broker refuses to write response and does not delete QObject when control return to event loop")
                {
                    QCoreApplication::sendPostedEvents(pObject, QEvent::DeferredDelete);
                    REQUIRE(!hasDestroyedQObject);

                    AND_WHEN("broker is reset")
                    {
                        brokerPrivate.resetResponseWriting();

                        THEN("set QObject is deleted when control return to event loop")
                        {
                            REQUIRE(!hasDestroyedQObject);
                            QCoreApplication::sendPostedEvents(pObject, QEvent::DeferredDelete);
                            REQUIRE(hasDestroyedQObject);
                        }
                    }
                }
            }
        }
    }
}


SCENARIO("HttpBrokerPrivate deletes any previously set QObject when setting new QObject")
{
    GIVEN("a broker with a QObject set")
    {
        IOChannelTest ioChannel;
        HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
        auto pBrokerPrivate = std::make_unique<HttpBrokerPrivate>(&ioChannel, &parser);
        auto *pObject = new QObject;
        bool hasDestroyedQObject = false;
        QObject::connect(pObject, &QObject::destroyed, [&](){hasDestroyedQObject = true;});
        pBrokerPrivate->setQObject(pObject);

        WHEN("null pointer is set as QObject")
        {
            QCoreApplication::sendPostedEvents(pObject, QEvent::DeferredDelete);
            REQUIRE(!hasDestroyedQObject);
            pBrokerPrivate->setQObject(nullptr);

            THEN("previously set QObject is deleted when control returns to event loop")
            {
                REQUIRE(!hasDestroyedQObject);
                QCoreApplication::sendPostedEvents(pObject, QEvent::DeferredDelete);
                REQUIRE(hasDestroyedQObject);
            }
        }

        WHEN("non-null pointer is set as QObject")
        {
            QCoreApplication::sendPostedEvents(pObject, QEvent::DeferredDelete);
            REQUIRE(!hasDestroyedQObject);
            auto *pOtherObject = new QObject;
            bool hasDestroyedOtherQObject = false;
            QObject::connect(pOtherObject, &QObject::destroyed, [&](){hasDestroyedOtherQObject = true;});
            pBrokerPrivate->setQObject(pOtherObject);

            THEN("previously set QObject is deleted when control returns to event loop")
            {
                REQUIRE(!hasDestroyedQObject);
                REQUIRE(!hasDestroyedOtherQObject);
                QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                REQUIRE(hasDestroyedQObject);
                REQUIRE(!hasDestroyedOtherQObject);

                AND_WHEN("null pointer is set as QObject")
                {
                    pBrokerPrivate->setQObject(nullptr);

                    THEN("other QObject is deleted when control returns to event loop")
                    {
                        REQUIRE(!hasDestroyedOtherQObject);
                        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                        REQUIRE(hasDestroyedOtherQObject);
                    }
                }

                AND_WHEN("broker private is destroyed")
                {
                    pBrokerPrivate.reset(nullptr);

                    THEN("other QObject is deleted when control returns to event loop")
                    {
                        REQUIRE(!hasDestroyedOtherQObject);
                        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                        REQUIRE(hasDestroyedOtherQObject);
                    }
                }
            }
        }

        WHEN("broker private is destroyed")
        {
            pBrokerPrivate.reset(nullptr);

            THEN("previously set QObject is deleted when control returns to event loop")
            {
                REQUIRE(!hasDestroyedQObject);
                QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                REQUIRE(hasDestroyedQObject);
            }
        }
    }
}
