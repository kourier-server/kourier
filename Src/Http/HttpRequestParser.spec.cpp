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

#include "HttpRequestParser.h"
#include "../Core/IOChannel.h"
#include <Spectator.h>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

using Kourier::HttpRequestParser;
using Kourier::HttpRequest;
using Kourier::IOChannel;
using Kourier::DataSource;
using Kourier::DataSink;
using Kourier::RingBuffer;
using Kourier::HttpRequestLimits;
using Kourier::HttpServer;


namespace Test::HttpRequestParser
{

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
    IOChannelTest(std::string_view data) {m_readBuffer.write(data);}
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

using namespace Test::HttpRequestParser;


SCENARIO("HttpRequestParser parses http requests with only host header and no body")
{
    GIVEN("a single http request with only host header and no body")
    {
        const auto httpMethod = GENERATE(AS(std::string_view), "GET", "PUT", "PATCH", "POST", "DELETE", "HEAD", "OPTIONS");
        const auto urlPath = GENERATE(AS(std::string_view), "/", "/an", "/an/", "/an/absolute", "/an/absolute/", "/an/absolute/path");
        const auto urlQuery = GENERATE(AS(std::string_view),
                                       "",
                                       "a_query",
                                       "key=val",
                                       "date=2015-05-31&locations=Los%20Angeles%7CNew%20York&attendees=10%7C5&services=Housekeeping,Catering%7CHousekeeping&duration=60",
                                       "aid=304142&label=gen173nr-342396dbc1b331fab24&tmpl=searchresults&ac_click_type=b&ac_position=0&checkin_month=3&checkin_monthday=7&checkin_year=2019&checkout_month=3&checkout_monthday=10&checkout_year=2019&class_interval=1&dest_id=20015107&dest_type=city&dtdisc=0&from_sf=1&group_adults=1&group_children=0&inac=0&index_postcard=0&label_click=undef&no_rooms=1&postcard=0&raw_dest_type=city&room1=A&sb_price_type=total&sb_travel_purpose=business&search_selected=1&shw_aparth=1&slp_r_match=0&src=index&srpvid=e0267a2be8ef0020&ss=Pasadena%2C%20California%2C%20USA&ss_all=0&ss_raw=pasadena&ssb=empty&sshis=0&nflt=hotelfacility%3D107%3Bmealplan%3D1%3Bpri%3D4%3Bpri%3D3%3Bclass%3D4%3Bclass%3D5%3Bpopular_activities%3D55%3Bhr_24%3D8%3Btdb%3D3%3Breview_score%3D70%3Broomfacility%3D75%3B&rsf=blah");

        std::string requestLine;
        requestLine.reserve(256);
        requestLine.append(httpMethod)
            .append(" ")
            .append(urlPath).append(urlQuery.empty() ? "" : "?").append(urlQuery)
            .append(" ")
            .append("HTTP/1.1\r\n")
            .append("Host: host.com\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(requestLine);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("request is successfully parsed")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(requestLine.size() == parser.requestSize());

                AND_THEN("parser extracts the correct information from the request data")
                {
                    const auto &request = parser.request();
                    switch(request.method())
                    {
                        case HttpRequest::Method::GET:
                            REQUIRE(httpMethod == "GET");
                            break;
                        case HttpRequest::Method::PUT:
                            REQUIRE(httpMethod == "PUT");
                            break;
                        case HttpRequest::Method::POST:
                            REQUIRE(httpMethod == "POST");
                            break;
                        case HttpRequest::Method::PATCH:
                            REQUIRE(httpMethod == "PATCH");
                            break;
                        case HttpRequest::Method::DELETE:
                            REQUIRE(httpMethod == "DELETE");
                            break;
                        case HttpRequest::Method::HEAD:
                            REQUIRE(httpMethod == "HEAD");
                            break;
                        case HttpRequest::Method::OPTIONS:
                            REQUIRE(httpMethod == "OPTIONS");
                            break;
                        default:
                            FAIL("This code is supposed to be unreachable.");
                    }
                    REQUIRE(1 == request.headersCount());
                    REQUIRE(0 == request.headerCount("Content-Length"));
                    REQUIRE(1 == request.headerCount("Host"));
                    REQUIRE(0 == request.headerCount("Date"));
                    REQUIRE(0 == request.headerCount("Transfer-Encoding"));
                    REQUIRE(0 == request.headerCount("AValidHeaderName"));
                    REQUIRE(0 == request.headerCount("An!nvalid\tHeaderName"));
                    REQUIRE(urlPath == request.targetPath());
                    REQUIRE(urlQuery == request.targetQuery());
                    REQUIRE(request.isComplete());
                    REQUIRE(!request.chunked());
                    REQUIRE(request.requestBodySize() == 0);
                    REQUIRE(request.pendingBodySize() == 0);
                    REQUIRE(!request.hasBody());
                    REQUIRE(request.body().empty());
                    REQUIRE(request.bodyType() == HttpRequest::BodyType::NoBody);
                }
            }
        }

        WHEN("the request is parsed byte by byte")
        {
            IOChannelTest ioChannel(std::string_view(requestLine.data(), 1));
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            for (int i = 1; i < (requestLine.size() - 1); ++i)
            {
                ioChannel.readBuffer().write(&requestLine[i], 1);
                REQUIRE(HttpRequestParser::ParserStatus::NeedsMoreData == parser.parse());
            }
            ioChannel.readBuffer().write(&requestLine[requestLine.size() - 1], 1);
            const auto parserStatus = parser.parse();

            THEN("the request is successfully parsed")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(requestLine.size() == parser.requestSize());

                AND_THEN("the parser extracts the correct information from the request data")
                {
                    const auto &request = parser.request();
                    switch(request.method())
                    {
                        case HttpRequest::Method::GET:
                            REQUIRE(httpMethod == "GET");
                            break;
                        case HttpRequest::Method::PUT:
                            REQUIRE(httpMethod == "PUT");
                            break;
                        case HttpRequest::Method::POST:
                            REQUIRE(httpMethod == "POST");
                            break;
                        case HttpRequest::Method::PATCH:
                            REQUIRE(httpMethod == "PATCH");
                            break;
                        case HttpRequest::Method::DELETE:
                            REQUIRE(httpMethod == "DELETE");
                            break;
                        case HttpRequest::Method::HEAD:
                            REQUIRE(httpMethod == "HEAD");
                            break;
                        case HttpRequest::Method::OPTIONS:
                            REQUIRE(httpMethod == "OPTIONS");
                            break;
                        default:
                            FAIL("This code is supposed to be unreachable.");
                    }
                    REQUIRE(1 == request.headersCount());
                    REQUIRE(0 == request.headerCount("Content-Length"));
                    REQUIRE(1 == request.headerCount("Host"));
                    REQUIRE(0 == request.headerCount("Date"));
                    REQUIRE(0 == request.headerCount("Transfer-Encoding"));
                    REQUIRE(0 == request.headerCount("AValidHeaderName"));
                    REQUIRE(0 == request.headerCount("An!nvalid\tHeaderName"));
                    REQUIRE(urlPath == request.targetPath());
                    REQUIRE(urlQuery == request.targetQuery());
                    REQUIRE(request.isComplete());
                    REQUIRE(!request.chunked());
                    REQUIRE(request.requestBodySize() == 0);
                    REQUIRE(request.pendingBodySize() == 0);
                    REQUIRE(!request.hasBody());
                    REQUIRE(request.body().empty());
                    REQUIRE(request.bodyType() == HttpRequest::BodyType::NoBody);
                }
            }
        }
    }

    GIVEN("multiple http requests with only host header and no body")
    {
        const std::vector<std::string_view> httpMethods({"GET", "PUT", "PATCH", "POST", "DELETE", "HEAD"});
        const std::vector<std::string_view> urlPaths({"/", "/an", "/an/", "/an/absolute", "/an/absolute/", "/an/absolute/path"});
        const std::vector<std::string_view> urlQueries({"", "a_query", "key=val", "date=2015-05-31&locations=Los%20Angeles%7CNew%20York&attendees=10%7C5&services=Housekeeping,Catering%7CHousekeeping&duration=60"});
        std::string requests;
        requests.reserve(65536);
        for (const auto &httpMethod : httpMethods)
        {
            for (const auto &urlPath : urlPaths)
            {
                for (const auto &urlQuery : urlQueries)
                {
                    requests.append(httpMethod)
                            .append(" ")
                            .append(urlPath).append(urlQuery.empty() ? "" : "?").append(urlQuery)
                            .append(" ")
                            .append("HTTP/1.1\r\nHost: host.com\r\n\r\n");
                }
            }
        }

        WHEN("parser processes data from all requests at once")
        {
            IOChannelTest ioChannel(requests);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));

            THEN("all requests are successfully parsed")
            {
                for (const auto &httpMethod : httpMethods)
                {
                    for (const auto &urlPath : urlPaths)
                    {
                        for (const auto &urlQuery : urlQueries)
                        {
                            std::string currentRequest;
                            currentRequest.reserve(1024);
                            currentRequest.append(httpMethod)
                                          .append(" ")
                                          .append(urlPath).append(urlQuery.empty() ? "" : "?").append(urlQuery)
                                          .append(" ")
                                          .append("HTTP/1.1\r\nHost: host.com\r\n\r\n");
                            const auto parserStatus = parser.parse();
                            REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                            REQUIRE(currentRequest.size() == parser.requestSize());
                            const auto &request = parser.request();
                            switch(request.method())
                            {
                                case HttpRequest::Method::GET:
                                    REQUIRE(httpMethod == "GET");
                                    break;
                                case HttpRequest::Method::PUT:
                                    REQUIRE(httpMethod == "PUT");
                                    break;
                                case HttpRequest::Method::POST:
                                    REQUIRE(httpMethod == "POST");
                                    break;
                                case HttpRequest::Method::PATCH:
                                    REQUIRE(httpMethod == "PATCH");
                                    break;
                                case HttpRequest::Method::DELETE:
                                    REQUIRE(httpMethod == "DELETE");
                                    break;
                                case HttpRequest::Method::HEAD:
                                    REQUIRE(httpMethod == "HEAD");
                                    break;
                                case HttpRequest::Method::OPTIONS:
                                    REQUIRE(httpMethod == "OPTIONS");
                                    break;
                                default:
                                    FAIL("This code is supposed to be unreachable.");
                            }
                            REQUIRE(1 == request.headersCount());
                            REQUIRE(0 == request.headerCount("Content-Length"));
                            REQUIRE(1 == request.headerCount("Host"));
                            REQUIRE(0 == request.headerCount("Date"));
                            REQUIRE(0 == request.headerCount("Transfer-Encoding"));
                            REQUIRE(0 == request.headerCount("AValidHeaderName"));
                            REQUIRE(0 == request.headerCount("An!nvalid\tHeaderName"));
                            REQUIRE(urlPath == request.targetPath());
                            REQUIRE(urlQuery == request.targetQuery());
                            REQUIRE(request.isComplete());
                            REQUIRE(!request.chunked());
                            REQUIRE(request.requestBodySize() == 0);
                            REQUIRE(request.pendingBodySize() == 0);
                            REQUIRE(!request.hasBody());
                            REQUIRE(request.body().empty());
                            REQUIRE(request.bodyType() == HttpRequest::BodyType::NoBody);
                        }
                    }
                }
                const auto parserStatus = parser.parse();
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData);
            }
        }

        WHEN("parser processes data from all requests byte by byte")
        {
            int index = 0;
            IOChannelTest ioChannel(std::string_view(requests.data(), 1));
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));

            THEN("all requests are successfully parsed")
            {
                for (const auto &httpMethod : httpMethods)
                {
                    for (const auto &urlPath : urlPaths)
                    {
                        for (const auto &urlQuery : urlQueries)
                        {
                            std::string currentRequest;
                            currentRequest.reserve(1024);
                            currentRequest.append(httpMethod)
                                          .append(" ")
                                          .append(urlPath).append(urlQuery.empty() ? "" : "?").append(urlQuery)
                                          .append(" ")
                                          .append("HTTP/1.1\r\nHost: host.com\r\n\r\n");
                            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
                            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
                            {
                                ioChannel.readBuffer().write(&requests[++index], 1);
                                parserStatus = parser.parse();
                            }
                            REQUIRE(HttpRequestParser::ParserStatus::ParsedRequest == parserStatus);
                            REQUIRE(currentRequest.size() == parser.requestSize());
                            const auto &request = parser.request();
                            switch(request.method())
                            {
                                case HttpRequest::Method::GET:
                                    REQUIRE(httpMethod == "GET");
                                    break;
                                case HttpRequest::Method::PUT:
                                    REQUIRE(httpMethod == "PUT");
                                    break;
                                case HttpRequest::Method::POST:
                                    REQUIRE(httpMethod == "POST");
                                    break;
                                case HttpRequest::Method::PATCH:
                                    REQUIRE(httpMethod == "PATCH");
                                    break;
                                case HttpRequest::Method::DELETE:
                                    REQUIRE(httpMethod == "DELETE");
                                    break;
                                case HttpRequest::Method::HEAD:
                                    REQUIRE(httpMethod == "HEAD");
                                    break;
                                case HttpRequest::Method::OPTIONS:
                                    REQUIRE(httpMethod == "OPTIONS");
                                    break;
                                default:
                                    FAIL("This code is supposed to be unreachable.");
                            }
                            REQUIRE(1 == request.headersCount());
                            REQUIRE(0 == request.headerCount("Content-Length"));
                            REQUIRE(1 == request.headerCount("Host"));
                            REQUIRE(0 == request.headerCount("Date"));
                            REQUIRE(0 == request.headerCount("Transfer-Encoding"));
                            REQUIRE(0 == request.headerCount("AValidHeaderName"));
                            REQUIRE(0 == request.headerCount("An!nvalid\tHeaderName"));
                            REQUIRE(urlPath == request.targetPath());
                            REQUIRE(urlQuery == request.targetQuery());
                            REQUIRE(request.isComplete());
                            REQUIRE(!request.chunked());
                            REQUIRE(request.requestBodySize() == 0);
                            REQUIRE(request.pendingBodySize() == 0);
                            REQUIRE(!request.hasBody());
                            REQUIRE(request.body().empty());
                            REQUIRE(request.bodyType() == HttpRequest::BodyType::NoBody);
                        }
                    }
                }
                const auto parserStatus = parser.parse();
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData);
            }
        }
    }

    GIVEN("malformed requests lacking the host header")
    {
        const auto httpMethod = GENERATE(AS(std::string_view), "GET", "PUT", "PATCH", "POST", "DELETE", "HEAD", "OPTIONS");
        const auto urlPath = GENERATE(AS(std::string_view), "/", "/an", "/an/", "/an/absolute", "/an/absolute/", "/an/absolute/path");
        const auto urlQuery = GENERATE(AS(std::string_view), "", "a_query", "key=val", "date=2015-05-31&locations=Los%20Angeles%7CNew%20York&attendees=10%7C5&services=Housekeeping,Catering%7CHousekeeping&duration=60");

        std::string requestLine;
        requestLine.reserve(256);
        requestLine.append(httpMethod)
            .append(" ")
            .append(urlPath).append(urlQuery.empty() ? "" : "?").append(urlQuery)
            .append(" ")
            .append("HTTP/1.1\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(requestLine);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse the malformed requests")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("the request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                ioChannel.readBuffer().write(&requestLine[i++], 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser fails to parse the malformed requests")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }

    GIVEN("requests containing invalid methods")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "get / HTTP/1.1\r\nHost: host.com\r\n\r\n",
                                      "ERASE / HTTP/1.1\r\nHost: host.com\r\n\r\n",
                                      "PUTPOSTPATCH / HTTP/1.1\r\nHost: host.com\r\n\r\n",
                                      "GETT / HTTP/1.1\r\nHost: host.com\r\n\r\n",
                                      "Get / HTTP/1.1\r\nHost: host.com\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse the malformed requests")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                ioChannel.readBuffer().write(&request[i++], 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser fails to parse the malformed requests")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }

    GIVEN("request containing all valid characters in absolute path")
    {
        // pchar          = unreserved / pct-encoded / sub-delims / ":" / "@"
        // unreserved     = ALPHA / DIGIT / "-" / "." / "_" / "~"
        // pct-encoded    = "%" HEXDIG HEXDIG
        // sub-delims     = "!" / "$" / "&" / "'" / "(" / ")" / "*" / "+" / "," / ";" / "="
        std::string absolutePath;
        absolutePath.reserve(128);
        absolutePath.push_back('/');
        for (char ch = 0; ch < 127; ++ch)
        {
            if (('a' <= ch && ch <= 'z')
                || ('A' <= ch && ch <= 'Z')
                || ('0' <= ch && ch <= '9')
                || ch == '-' || ch == '.' || ch == '_' || ch == '~'
                || ch == '!' || ch == '$' || ch == '&' || ch == '\''
                || ch == '(' || ch == ')' || ch == '*' || ch == '+'
                || ch == ',' || ch == ';' || ch == '=' || ch == ':' || ch == '@')
                absolutePath.push_back(ch);
        }
        std::string request;
        request.reserve(256);
        request.append("GET ")
            .append(absolutePath)
            .append(" HTTP/1.1\r\nHost: host.com\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser parses the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());
                REQUIRE(parser.request().targetPath() == absolutePath);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                ioChannel.readBuffer().write(&request[i++], 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser parses the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());
                REQUIRE(parser.request().targetPath() == absolutePath);
            }
        }
    }

    GIVEN("request containing a percent-encoded hex char as absolute path")
    {
        std::string_view absolutePath("/%2F");
        std::string_view request("GET /%2F HTTP/1.1\r\nHost: host.com\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser parses the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());
                REQUIRE(parser.request().targetPath() == absolutePath);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                ioChannel.readBuffer().write(&request[i++], 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser parses the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());
                REQUIRE(parser.request().targetPath() == absolutePath);
            }
        }
    }

    GIVEN("request containing all valid percent-encoded hex chars in absolute path")
    {
        const std::vector<char> validHexChars({'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f','A','B','C','D','E','F'});
        std::string absolutePath;
        absolutePath.reserve(1 + 3 * validHexChars.size() * validHexChars.size());
        absolutePath.push_back('/');
        for (auto i = 0; i < validHexChars.size(); ++i)
        {
            for (auto j = 0; j < validHexChars.size(); ++j)
            {
                absolutePath.push_back('%');
                absolutePath.push_back(validHexChars[i]);
                absolutePath.push_back(validHexChars[j]);
            }
        }
        std::string request;
        request.reserve(256);
        request.append("GET ")
            .append(absolutePath)
            .append(" HTTP/1.1\r\nHost: host.com\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser parses the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());
                REQUIRE(parser.request().targetPath() == absolutePath);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                ioChannel.readBuffer().write(&request[i++], 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser parses the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());
                REQUIRE(parser.request().targetPath() == absolutePath);
            }
        }
    }

    GIVEN("request containing a percent-encoded hex char in absolute path")
    {
        auto delta = GENERATE_RANGE(AS(size_t), 0, 128);
        std::string request;
        std::string absolutePath = '/' + ((delta > 0) ? std::string(delta, 'a') : std::string{})
                                   + "%20"
                                   + (((128 - delta) > 0) ? std::string(128 - delta, 'a') : std::string{});
        request.reserve(256);
        request.append("GET ")
            .append(absolutePath)
            .append(" HTTP/1.1\r\nHost: host.com\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser parses the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());
                REQUIRE(parser.request().targetPath() == absolutePath);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                ioChannel.readBuffer().write(&request[i++], 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser parses the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());
                REQUIRE(parser.request().targetPath() == absolutePath);
            }
        }
    }

    GIVEN("request containing an invalid char as absolute path")
    {
        std::string_view request("GET /\t HTTP/1.1\r\nHost: host.com\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                ioChannel.readBuffer().write(&request[i++], 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }

    GIVEN("request containing all invalid chars in absolute path")
    {
        // pchar          = unreserved / pct-encoded / sub-delims / ":" / "@"
        // unreserved     = ALPHA / DIGIT / "-" / "." / "_" / "~"
        // pct-encoded    = "%" HEXDIG HEXDIG
        // sub-delims     = "!" / "$" / "&" / "'" / "(" / ")" / "*" / "+" / "," / ";" / "="
        constexpr static size_t invalidCharsCount = (256 - (26 + 26 + 10 + 17 + 2));
        const static auto invalidChars = []() -> std::string
        {
            std::string temp;
            temp.reserve(256);
            for (int16_t ascii = std::numeric_limits<char>::min(); ascii <= std::numeric_limits<char>::max(); ++ascii)
            {
                char ch = static_cast<char>(ascii);
                if (('a' <= ch && ch <= 'z')
                    || ('A' <= ch && ch <= 'Z')
                    || ('0' <= ch && ch <= '9')
                    || ch == '-' || ch == '.' || ch == '_' || ch == '~'
                    || ch == '!' || ch == '$' || ch == '&' || ch == '\''
                    || ch == '(' || ch == ')' || ch == '*' || ch == '+'
                    || ch == ',' || ch == ';' || ch == '=' || ch == ':' || ch == '@'
                     || ch == '/' || ch == '?')
                    continue;
                else
                    temp.push_back(ch);
            }
            REQUIRE(temp.size() == invalidCharsCount);
            return temp;
        }();
        const auto idx = GENERATE_RANGE(AS(size_t), 0, invalidCharsCount - 1);
        std::string request;
        request.reserve(32);
        request.append("GET /aeiou");
        request.push_back(invalidChars[idx]);
        request.append("blah HTTP/1.1\r\nHost: host.com\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                ioChannel.readBuffer().write(&request[i++], 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }

    GIVEN("request containing an invalid char in absolute path")
    {
        auto delta = GENERATE_RANGE(AS(size_t), 0, 128);
        std::string request;
        std::string absolutePath = '/' + ((delta > 0) ? std::string(delta, 'a') : std::string{})
                                   + "\t"
                                   + (((128 - delta) > 0) ? std::string(128 - delta, 'a') : std::string{});
        request.reserve(256);
        request.append("GET ")
            .append(absolutePath)
            .append(" HTTP/1.1\r\nHost: host.com\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                ioChannel.readBuffer().write(&request[i++], 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }

    GIVEN("request containing all valid characters in query")
    {
        // query          = *( pchar / "/" / "?" )
        // pchar          = unreserved / pct-encoded / sub-delims / ":" / "@"
        // unreserved     = ALPHA / DIGIT / "-" / "." / "_" / "~"
        // pct-encoded    = "%" HEXDIG HEXDIG
        // sub-delims     = "!" / "$" / "&" / "'" / "(" / ")" / "*" / "+" / "," / ";" / "="
        const auto absolutePath = GENERATE(AS(std::string_view), "/", "/blah");
        std::string query;
        query.reserve(128);
        for (char ch = 0; ch < 127; ++ch)
        {
            if (('a' <= ch && ch <= 'z')
                || ('A' <= ch && ch <= 'Z')
                || ('0' <= ch && ch <= '9')
                || ch == '-' || ch == '.' || ch == '_' || ch == '~'
                || ch == '!' || ch == '$' || ch == '&' || ch == '\''
                || ch == '(' || ch == ')' || ch == '*' || ch == '+'
                || ch == ',' || ch == ';' || ch == '=' || ch == ':'
                || ch == '@' || ch == '/' || ch == '?')
                query.push_back(ch);
        }
        std::string request;
        request.reserve(256);
        request.append("GET ")
            .append(absolutePath)
            .append("?")
            .append(query)
            .append(" HTTP/1.1\r\nHost: host.com\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser parses the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());
                REQUIRE(parser.request().targetPath() == absolutePath);
                REQUIRE(parser.request().targetQuery() == query);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                ioChannel.readBuffer().write(&request[i++], 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser parses the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());
                REQUIRE(parser.request().targetPath() == absolutePath);
                REQUIRE(parser.request().targetQuery() == query);
            }
        }
    }

    GIVEN("request containing a percent-encoded hex char as query")
    {
        const auto absolutePath = GENERATE(AS(std::string_view), "/", "/blah");
        std::string_view query("%2F");
        std::string request;
        request.reserve(128);
        request.append("GET ")
               .append(absolutePath)
               .append("?")
               .append(query)
               .append(" HTTP/1.1\r\nHost: host.com\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser parses the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());
                REQUIRE(parser.request().targetPath() == absolutePath);
                REQUIRE(parser.request().targetQuery() == query);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                ioChannel.readBuffer().write(&request[i++], 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser parses the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());
                REQUIRE(parser.request().targetPath() == absolutePath);
            }
        }
    }

    GIVEN("request containing all valid percent-encoded hex chars in query")
    {
        const auto absolutePath = GENERATE(AS(std::string_view), "/", "/blah");
        const std::vector<char> validHexChars({'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f','A','B','C','D','E','F'});
        std::string query;
        query.reserve(1 + 3 * validHexChars.size() * validHexChars.size());
        for (auto i = 0; i < validHexChars.size(); ++i)
        {
            for (auto j = 0; j < validHexChars.size(); ++j)
            {
                query.push_back('%');
                query.push_back(validHexChars[i]);
                query.push_back(validHexChars[j]);
            }
        }
        std::string request;
        request.reserve(256);
        request.append("GET ")
            .append(absolutePath)
            .append("?")
            .append(query)
            .append(" HTTP/1.1\r\nHost: host.com\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser parses the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());
                REQUIRE(parser.request().targetPath() == absolutePath);
                REQUIRE(parser.request().targetQuery() == query);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                ioChannel.readBuffer().write(&request[i++], 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser parses the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());
                REQUIRE(parser.request().targetPath() == absolutePath);
                REQUIRE(parser.request().targetQuery() == query);
            }
        }
    }

    GIVEN("request containing a percent-encoded hex char in query")
    {
        const auto absolutePath = GENERATE(AS(std::string_view), "/", "/blah");
        auto delta = GENERATE_RANGE(AS(size_t), 0, 128);
        std::string request;
        std::string query = ((delta > 0) ? std::string(delta, 'a') : std::string{})
                            + "%20"
                            + (((128 - delta) > 0) ? std::string(128 - delta, 'a') : std::string{});
        request.reserve(256);
        request.append("GET ")
            .append(absolutePath)
            .append("?")
            .append(query)
            .append(" HTTP/1.1\r\nHost: host.com\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser parses the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());
                REQUIRE(parser.request().targetPath() == absolutePath);
                REQUIRE(parser.request().targetQuery() == query);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                ioChannel.readBuffer().write(&request[i++], 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser parses the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());
                REQUIRE(parser.request().targetPath() == absolutePath);
                REQUIRE(parser.request().targetQuery() == query);
            }
        }
    }

    GIVEN("request containing an invalid char as query")
    {
        std::string_view request("GET /?\t HTTP/1.1\r\nHost: host.com\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                ioChannel.readBuffer().write(&request[i++], 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }

    GIVEN("request containing all invalid chars in query")
    {
        // query          = *( pchar / "/" / "?" )
        // pchar          = unreserved / pct-encoded / sub-delims / ":" / "@"
        // unreserved     = ALPHA / DIGIT / "-" / "." / "_" / "~"
        // pct-encoded    = "%" HEXDIG HEXDIG
        // sub-delims     = "!" / "$" / "&" / "'" / "(" / ")" / "*" / "+" / "," / ";" / "="
        constexpr static size_t invalidCharsCount = (256 - (26 + 26 + 10 + 17 + 2));
        const static auto invalidChars = []() -> std::string
        {
            std::string temp;
            temp.reserve(256);
            for (int16_t ascii = std::numeric_limits<char>::min(); ascii <= std::numeric_limits<char>::max(); ++ascii)
            {
                char ch = static_cast<char>(ascii);
                if (('a' <= ch && ch <= 'z')
                    || ('A' <= ch && ch <= 'Z')
                    || ('0' <= ch && ch <= '9')
                    || ch == '-' || ch == '.' || ch == '_' || ch == '~'
                    || ch == '!' || ch == '$' || ch == '&' || ch == '\''
                    || ch == '(' || ch == ')' || ch == '*' || ch == '+'
                    || ch == ',' || ch == ';' || ch == '=' || ch == ':' || ch == '@'
                    || ch == '/' || ch == '?')
                    continue;
                else
                    temp.push_back(ch);
            }
            REQUIRE(temp.size() == invalidCharsCount);
            return temp;
        }();
        const auto idx = GENERATE_RANGE(AS(size_t), 0, invalidCharsCount - 1);
        const auto absolutePath = GENERATE(AS(std::string_view), "/", "/blah");
        std::string request;
        request.reserve(32);
        request.append("GET ")
               .append(absolutePath)
               .append("?");
        request.push_back(invalidChars[idx]);
        request.append(" HTTP/1.1\r\nHost: host.com\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                ioChannel.readBuffer().write(&request[i++], 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }

    GIVEN("request containing an invalid char in query")
    {
        auto delta = GENERATE_RANGE(AS(size_t), 0, 128);
        const auto absolutePath = GENERATE(AS(std::string_view), "/", "/blah");
        std::string request;
        std::string query = '/' + ((delta > 0) ? std::string(delta, 'a') : std::string{})
                            + "\t"
                            + (((128 - delta) > 0) ? std::string(128 - delta, 'a') : std::string{});
        request.reserve(256);
        request.append("GET ")
            .append(absolutePath)
            .append("?")
            .append(query)
            .append(" HTTP/1.1\r\nHost: host.com\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                ioChannel.readBuffer().write(&request[i++], 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }

    GIVEN("requests with empty queries with only host header and no body")
    {
        const auto request = GENERATE(AS(std::string_view), "GET /? HTTP/1.1\r\nHost: host.com\r\n\r\n", "GET /an_absolute_path? HTTP/1.1\r\nHost: host.com\r\n\r\n", "GET /an_absolute_path/with/sub/a/sub/path? HTTP/1.1\r\nHost: host.com\r\n\r\n");

        WHEN("the request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser parses the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());
                const auto &request = parser.request();
                REQUIRE(request.targetQuery().empty());
            }
        }

        WHEN("the request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                const auto ch = request[i++];
                ioChannel.readBuffer().write(&ch, 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser parses the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());
                const auto &request = parser.request();
                REQUIRE(request.targetQuery().empty());
            }
        }
    }

    GIVEN("requests with uncommon queries with only host header and no body")
    {
        auto absolutePath = GENERATE(AS(std::string_view), "/", "/an_absolute_path", "/a/path/");
        auto query = GENERATE(AS(std::string_view), "?", "//?/?");
        std::string request;
        request.reserve(64);
        request.append("GET ")
            .append(absolutePath)
            .append("?")
            .append(query)
            .append(" HTTP/1.1\r\nHost: host.com\r\n\r\n");

        WHEN("the request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser parses the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());
                const auto &request = parser.request();
                REQUIRE(request.targetPath() == absolutePath);
                REQUIRE(request.targetQuery() == query);
            }
        }

        WHEN("the request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                const auto ch = request[i++];
                ioChannel.readBuffer().write(&ch, 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser parses the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());
                const auto &request = parser.request();
                REQUIRE(request.targetPath() == absolutePath);
                REQUIRE(request.targetQuery() == query);
            }
        }
    }

    GIVEN("requests with invalid http versions with only host header and no body")
    {
        const auto request = GENERATE(AS(std::string_view), "GET /path HTTP/2.0\r\nHost: host.com\r\n\r\n", "GET /path http/1.1\r\nHost: host.com\r\n\r\n", "GET /path HTTP_VERSION/1.1\r\nHost: host.com\r\n\r\n");

        WHEN("the request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse the invalid request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("the request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                const auto ch = request[i++];
                ioChannel.readBuffer().write(&ch, 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser fails to parse the invalid request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }

    GIVEN("requests with invalid spaces with only host header and no body")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "GET  /path HTTP/1.1\r\nHost: host.com\r\n\r\n",
                                      "GET /path  HTTP/1.1\r\nHost: host.com\r\n\r\n",
                                      "GET /path HTTP/1.1 \r\nHost: host.com\r\n\r\n",
                                      " GET /path HTTP/1.1\r\nHost: host.com\r\n\r\n",
                                      "GET /path HTTP/1.1\r \nHost: host.com\r\n\r\n",
                                      "GET /path HTTP/1.1\r\n Host: host.com\r\n\r\n",
                                      "GET /path HTTP/1.1\r\nHost: host.com\r\n\r \n",
                                      "GET /path HT TP/1.1\r\nHost: host.com\r\n\r\n",
                                      "GET /path HTTP/1.1\r\nHost: host.com\r \n\r\n",
                                      "GET /path HTTP/1.1\r\nHost: host.com\r\n \r\n");

        WHEN("the request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse the invalid request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("the request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                const auto ch = request[i++];
                ioChannel.readBuffer().write(&ch, 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser fails to parse the invalid request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }

    GIVEN("invalid requests with invalid request lines with only the host header and no body")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "GET HTTP/1.1\r\nHost: host.com\r\n\r\n",
                                      "GET ? HTTP/1.1\r\nHost: host.com\r\n\r\n",
                                      "GET ?a_query HTTP/1.1\r\nHost: host.com\r\n\r\n",
                                      "/ HTTP/1.1\r\nHost: host.com\r\n\r\n",
                                      "/? HTTP/1.1\r\nHost: host.com\r\n\r\n",
                                      "/?a_query HTTP/1.1\r\nHost: host.com\r\n\r\n",
                                      "GET / \r\nHost: host.com\r\n\r\n",
                                      "GET /? \r\nHost: host.com\r\n\r\n",
                                      "GET /?a_query \r\nHost: host.com\r\n\r\n");

        WHEN("the request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("the request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                const auto ch = request[i++];
                ioChannel.readBuffer().write(&ch, 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }
}


SCENARIO("HttpRequestParser parses http requests with headers and no body")
{
    GIVEN("a single http request with headers and no body")
    {
        const auto httpMethod = GENERATE(AS(std::string_view), "GET", "HEAD", "OPTIONS");
        const auto urlPath = GENERATE(AS(std::string_view), "/", "/an/absolute/path");
        const auto urlQuery = GENERATE(AS(std::string_view), "", "a_query");
        const auto headersBlock = GENERATE(AS(std::vector<std::pair<std::string_view, std::string_view>>),
                                           {{"host", " example.com"}},
                                           {{"name", " value"}, {"host", " example.com"}},
                                           {{"host", " example.com"}, {"name1", " value1"}, {"name2", "  value2 "}, {"name3", " va l \t\t ue\t3"}},
                                           {{"Host", " www.example.com"},
                                            {"Referer", " vulnerable.host.net"},
                                            {"Connection", " keep-alive"},
                                            {"Upgrade-Insecure-Requests", " 1"},
                                            {"User-Agent", " Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/52.0.2743.116 Safari/537.36"},
                                            {"Accept", " text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8"},
                                            {"Accept-Encoding", " gzip, deflate, sdch"},
                                            {"Accept-Language", " en-US,en;q=0.8,ru;q=0.6"},
                                            {"Cookie", " a=sdfasd; sdf=3242u389erfhhs; djcnjhe=sdfsdafsdjfb324te1267dd; sdaf=mo2u8943478t67437461746rfdgfcdc; ityu=9u489573484duifhd; GTYFT=nsdjhcbyq3te76ewgfcZ; uityut=23Y746756247856425784657; GA=URHUFVHHVSDNFDHGYSDGF; a=%45345%dfdfg %4656%4534sdfjhsdb.sdfsg.sdfgsf.; aa=4583478; aaaaa=34435345; rrr=iy7t67t6tsdf; ggg=234i5y24785y78ry534785; sdf=3242u389erfhhs; ityu=9u489573484duifhd; GTYFT=nsdjhcbyq3te76ewgfcZ; uityut=23Y746756247856425784657; GA=URHUFVHHVSDNFDHGYSDGF; a=%45345%dfdfg %4656%4534sdfjhsdb.sdfsg.sdfgsf.; nsdjhfb=4358345y; jkbsdff=aaaa; aa=4583478; ggg=234i5y24785y78ry534785; mmm=23uy47fbhdsfbgh; bsdfhbhfgdqqwew=883476757%345345; jksdfb=2348y; ndfsgsfdg=235trHHVGHFGC; erertrt=3242342343423324234; g=888888888788"}});

        std::string request;
        request.reserve(256);
        request.append(httpMethod)
            .append(" ")
            .append(urlPath).append(urlQuery.empty() ? "" : "?").append(urlQuery)
            .append(" HTTP/1.1\r\n");
        for (const auto &field : headersBlock)
            request.append(field.first).append(":").append(field.second).append("\r\n");
        request.append("\r\n");

        WHEN("the request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("the request is successfully parsed")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());

                AND_THEN("the parser extracts the correct information from the request data")
                {
                    const auto &request = parser.request();
                    switch(request.method())
                    {
                        case HttpRequest::Method::GET:
                            REQUIRE(httpMethod == "GET");
                            break;
                        case HttpRequest::Method::PUT:
                            REQUIRE(httpMethod == "PUT");
                            break;
                        case HttpRequest::Method::POST:
                            REQUIRE(httpMethod == "POST");
                            break;
                        case HttpRequest::Method::PATCH:
                            REQUIRE(httpMethod == "PATCH");
                            break;
                        case HttpRequest::Method::DELETE:
                            REQUIRE(httpMethod == "DELETE");
                            break;
                        case HttpRequest::Method::HEAD:
                            REQUIRE(httpMethod == "HEAD");
                            break;
                        case HttpRequest::Method::OPTIONS:
                            REQUIRE(httpMethod == "OPTIONS");
                            break;
                        default:
                            FAIL("This code is supposed to be unreachable.");
                    }
                    REQUIRE(urlPath == request.targetPath());
                    REQUIRE(urlQuery == request.targetQuery());
                    REQUIRE(0 == request.headerCount(""));
                    REQUIRE(1 == request.headerCount("Host"));
                    REQUIRE(0 == request.headerCount("AValidHeaderName"));
                    REQUIRE(0 == request.headerCount("An!nvalid\tHeaderName"));
                    REQUIRE(request.headersCount() == headersBlock.size());
                    for (const auto &field : headersBlock)
                    {
                        REQUIRE(request.hasHeader(field.first));
                        REQUIRE(request.headerCount(field.first) == 1);
                        const auto headerValue = request.header(field.first);
                        REQUIRE(QByteArray(headerValue.data(), headerValue.size()) == QByteArray(field.second.data(), field.second.size()).trimmed());
                    }
                    REQUIRE(request.isComplete());
                    REQUIRE(!request.chunked());
                    REQUIRE(request.requestBodySize() == 0);
                    REQUIRE(request.pendingBodySize() == 0);
                    REQUIRE(!request.hasBody());
                    REQUIRE(request.body().empty());
                    REQUIRE(request.bodyType() == HttpRequest::BodyType::NoBody);
                }
            }
        }

        WHEN("the request is parsed byte by byte")
        {
            IOChannelTest ioChannel(std::string_view(request.data(), 1));
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            for (int i = 1; i < (request.size() - 1); ++i)
            {
                ioChannel.readBuffer().write(&request[i], 1);
                REQUIRE(HttpRequestParser::ParserStatus::NeedsMoreData == parser.parse());
            }
            ioChannel.readBuffer().write(&request[request.size() - 1], 1);
            const auto parserStatus = parser.parse();

            THEN("the request is successfully parsed")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());

                AND_THEN("the parser extracts the correct information from the request data")
                {
                    const auto &request = parser.request();
                    switch(request.method())
                    {
                        case HttpRequest::Method::GET:
                            REQUIRE(httpMethod == "GET");
                            break;
                        case HttpRequest::Method::PUT:
                            REQUIRE(httpMethod == "PUT");
                            break;
                        case HttpRequest::Method::POST:
                            REQUIRE(httpMethod == "POST");
                            break;
                        case HttpRequest::Method::PATCH:
                            REQUIRE(httpMethod == "PATCH");
                            break;
                        case HttpRequest::Method::DELETE:
                            REQUIRE(httpMethod == "DELETE");
                            break;
                        case HttpRequest::Method::HEAD:
                            REQUIRE(httpMethod == "HEAD");
                            break;
                        case HttpRequest::Method::OPTIONS:
                            REQUIRE(httpMethod == "OPTIONS");
                            break;
                        default:
                            FAIL("This code is supposed to be unreachable.");
                    }
                    REQUIRE(urlPath == request.targetPath());
                    REQUIRE(urlQuery == request.targetQuery());
                    REQUIRE(0 == request.headerCount(""));
                    REQUIRE(1 == request.headerCount("Host"));
                    REQUIRE(0 == request.headerCount("AValidHeaderName"));
                    REQUIRE(0 == request.headerCount("An!nvalid\tHeaderName"));
                    REQUIRE(request.headersCount() == headersBlock.size());
                    for (const auto &field : headersBlock)
                    {
                        REQUIRE(request.hasHeader(field.first));
                        REQUIRE(request.headerCount(field.first) == 1);
                        const auto headerValue = request.header(field.first);
                        REQUIRE(QByteArray(headerValue.data(), headerValue.size()) == QByteArray(field.second.data(), field.second.size()).trimmed());
                    }
                    REQUIRE(request.isComplete());
                    REQUIRE(!request.chunked());
                    REQUIRE(request.requestBodySize() == 0);
                    REQUIRE(request.pendingBodySize() == 0);
                    REQUIRE(!request.hasBody());
                    REQUIRE(request.body().empty());
                    REQUIRE(request.bodyType() == HttpRequest::BodyType::NoBody);
                }
            }
        }
    }

    GIVEN("multiple http requests with headers and no body")
    {
        const std::vector<std::string_view> httpMethods({"GET", "HEAD", "OPTIONS"});
        const std::vector<std::string_view> urlPaths({"/", "/an/absolute/path"});
        const std::vector<std::string_view> urlQueries({"", "a_query"});
        const std::vector<std::vector<std::pair<std::string_view, std::string_view>>> headersBlocks({{{"host", " example.com"}},
                                                                                                     {{"name", " value"}, {"host", " example.com"}},
                                                                                                     {{"host", " example.com"}, {"name1", " value1"}, {"name2", "  value2 "}, {"name3", " va l \t\t ue\t3"}},
                                                                                                     {{"Host", " www.example.com"},
                                                                                                      {"Referer", " vulnerable.host.net"},
                                                                                                      {"Connection", " keep-alive"},
                                                                                                      {"Upgrade-Insecure-Requests", " 1"},
                                                                                                      {"User-Agent", " Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/52.0.2743.116 Safari/537.36"},
                                                                                                      {"Accept", " text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8"},
                                                                                                      {"Accept-Encoding", " gzip, deflate, sdch"},
                                                                                                      {"Accept-Language", " en-US,en;q=0.8,ru;q=0.6"},
                                                                                                      {"Cookie", " a=sdfasd; sdf=3242u389erfhhs; djcnjhe=sdfsdafsdjfb324te1267dd; sdaf=mo2u8943478t67437461746rfdgfcdc; ityu=9u489573484duifhd; GTYFT=nsdjhcbyq3te76ewgfcZ; uityut=23Y746756247856425784657; GA=URHUFVHHVSDNFDHGYSDGF; a=%45345%dfdfg %4656%4534sdfjhsdb.sdfsg.sdfgsf.; aa=4583478; aaaaa=34435345; rrr=iy7t67t6tsdf; ggg=234i5y24785y78ry534785; sdf=3242u389erfhhs; ityu=9u489573484duifhd; GTYFT=nsdjhcbyq3te76ewgfcZ; uityut=23Y746756247856425784657; GA=URHUFVHHVSDNFDHGYSDGF; a=%45345%dfdfg %4656%4534sdfjhsdb.sdfsg.sdfgsf.; nsdjhfb=4358345y; jkbsdff=aaaa; aa=4583478; ggg=234i5y24785y78ry534785; mmm=23uy47fbhdsfbgh; bsdfhbhfgdqqwew=883476757%345345; jksdfb=2348y; ndfsgsfdg=235trHHVGHFGC; erertrt=3242342343423324234; g=888888888788"}}});
        std::string requests;
        requests.reserve(65536);
        for (const auto &httpMethod : httpMethods)
        {
            for (const auto &urlPath : urlPaths)
            {
                for (const auto &urlQuery : urlQueries)
                {
                    for (const auto &headersBlock : headersBlocks)
                    {
                        requests.append(httpMethod)
                        .append(" ")
                            .append(urlPath).append(urlQuery.empty() ? "" : "?").append(urlQuery)
                            .append(" HTTP/1.1\r\n");
                        for (const auto &field : headersBlock)
                            requests.append(field.first).append(":").append(field.second).append("\r\n");
                        requests.append("\r\n");
                    }
                }
            }
        }

        WHEN("parser processes data from all requests at once")
        {
            IOChannelTest ioChannel(requests);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));

            THEN("all requests are successfully parsed")
            {
                for (const auto &httpMethod : httpMethods)
                {
                    for (const auto &urlPath : urlPaths)
                    {
                        for (const auto &urlQuery : urlQueries)
                        {
                            for (const auto &headersBlock : headersBlocks)
                            {
                                std::string currentRequest;
                                currentRequest.reserve(2048);
                                currentRequest.append(httpMethod)
                                    .append(" ")
                                    .append(urlPath).append(urlQuery.empty() ? "" : "?").append(urlQuery)
                                    .append(" HTTP/1.1\r\n");
                                for (const auto &field : headersBlock)
                                    currentRequest.append(field.first).append(":").append(field.second).append("\r\n");
                                currentRequest.append("\r\n");
                                const auto parserStatus = parser.parse();
                                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                                REQUIRE(currentRequest.size() == parser.requestSize());
                                const auto &request = parser.request();
                                switch(request.method())
                                {
                                    case HttpRequest::Method::GET:
                                        REQUIRE(httpMethod == "GET");
                                        break;
                                    case HttpRequest::Method::PUT:
                                        REQUIRE(httpMethod == "PUT");
                                        break;
                                    case HttpRequest::Method::POST:
                                        REQUIRE(httpMethod == "POST");
                                        break;
                                    case HttpRequest::Method::PATCH:
                                        REQUIRE(httpMethod == "PATCH");
                                        break;
                                    case HttpRequest::Method::DELETE:
                                        REQUIRE(httpMethod == "DELETE");
                                        break;
                                    case HttpRequest::Method::HEAD:
                                        REQUIRE(httpMethod == "HEAD");
                                        break;
                                    case HttpRequest::Method::OPTIONS:
                                        REQUIRE(httpMethod == "OPTIONS");
                                        break;
                                    default:
                                        FAIL("This code is supposed to be unreachable.");
                                }
                                REQUIRE(urlPath == request.targetPath());
                                REQUIRE(urlQuery == request.targetQuery());
                                REQUIRE(0 == request.headerCount(""));
                                REQUIRE(1 == request.headerCount("Host"));
                                REQUIRE(0 == request.headerCount("AValidHeaderName"));
                                REQUIRE(0 == request.headerCount("An!nvalid\tHeaderName"));
                                REQUIRE(request.headersCount() == headersBlock.size());
                                for (const auto &field : headersBlock)
                                {
                                    REQUIRE(request.hasHeader(field.first));
                                    REQUIRE(request.headerCount(field.first) == 1);
                                    const auto headerValue = request.header(field.first);
                                    REQUIRE(QByteArray(headerValue.data(), headerValue.size()) == QByteArray(field.second.data(), field.second.size()).trimmed());
                                }
                                REQUIRE(request.isComplete());
                                REQUIRE(!request.chunked());
                                REQUIRE(request.requestBodySize() == 0);
                                REQUIRE(request.pendingBodySize() == 0);
                                REQUIRE(!request.hasBody());
                                REQUIRE(request.body().empty());
                                REQUIRE(request.bodyType() == HttpRequest::BodyType::NoBody);
                            }
                        }
                    }
                }
                const auto parserStatus = parser.parse();
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData);
            }
        }

        WHEN("parser processes data from all requests byte by byte")
        {
            size_t index = 0;
            IOChannelTest ioChannel(std::string_view(requests.data(), 1));
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));

            THEN("all requests are successfully parsed")
            {
                for (const auto &httpMethod : httpMethods)
                {
                    for (const auto &urlPath : urlPaths)
                    {
                        for (const auto &urlQuery : urlQueries)
                        {
                            for (const auto &headersBlock : headersBlocks)
                            {
                                std::string currentRequest;
                                currentRequest.reserve(2048);
                                currentRequest.append(httpMethod)
                                    .append(" ")
                                    .append(urlPath).append(urlQuery.empty() ? "" : "?").append(urlQuery)
                                    .append(" HTTP/1.1\r\n");
                                for (const auto &field : headersBlock)
                                    currentRequest.append(field.first).append(":").append(field.second).append("\r\n");
                                currentRequest.append("\r\n");
                                auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
                                while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
                                {
                                    ioChannel.readBuffer().write(&requests[++index], 1);
                                    parserStatus = parser.parse();
                                }
                                REQUIRE(HttpRequestParser::ParserStatus::ParsedRequest == parserStatus);
                                REQUIRE(currentRequest.size() == parser.requestSize());
                                const auto &request = parser.request();
                                switch(request.method())
                                {
                                    case HttpRequest::Method::GET:
                                        REQUIRE(httpMethod == "GET");
                                        break;
                                    case HttpRequest::Method::PUT:
                                        REQUIRE(httpMethod == "PUT");
                                        break;
                                    case HttpRequest::Method::POST:
                                        REQUIRE(httpMethod == "POST");
                                        break;
                                    case HttpRequest::Method::PATCH:
                                        REQUIRE(httpMethod == "PATCH");
                                        break;
                                    case HttpRequest::Method::DELETE:
                                        REQUIRE(httpMethod == "DELETE");
                                        break;
                                    case HttpRequest::Method::HEAD:
                                        REQUIRE(httpMethod == "HEAD");
                                        break;
                                    case HttpRequest::Method::OPTIONS:
                                        REQUIRE(httpMethod == "OPTIONS");
                                        break;
                                    default:
                                        FAIL("This code is supposed to be unreachable.");
                                }
                                REQUIRE(urlPath == request.targetPath());
                                REQUIRE(urlQuery == request.targetQuery());
                                REQUIRE(0 == request.headerCount(""));
                                REQUIRE(1 == request.headerCount("Host"));
                                REQUIRE(0 == request.headerCount("AValidHeaderName"));
                                REQUIRE(0 == request.headerCount("An!nvalid\tHeaderName"));
                                REQUIRE(request.headersCount() == headersBlock.size());
                                for (const auto &field : headersBlock)
                                {
                                    REQUIRE(request.hasHeader(field.first));
                                    REQUIRE(request.headerCount(field.first) == 1);
                                    const auto headerValue = request.header(field.first);
                                    REQUIRE(QByteArray(headerValue.data(), headerValue.size()) == QByteArray(field.second.data(), field.second.size()).trimmed());
                                }
                                REQUIRE(request.isComplete());
                                REQUIRE(!request.chunked());
                                REQUIRE(request.requestBodySize() == 0);
                                REQUIRE(request.pendingBodySize() == 0);
                                REQUIRE(!request.hasBody());
                                REQUIRE(request.body().empty());
                                REQUIRE(request.bodyType() == HttpRequest::BodyType::NoBody);
                            }
                        }
                    }
                }
                const auto parserStatus = parser.parse();
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData);
            }
        }
    }

    GIVEN("invalid requests without header name")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "GET / HTTP/1.1\r\n: value\r\n\r\n",
                                      "GET / HTTP/1.1\r\nHost: example.com\r\n: value\r\n\r\n");

        WHEN("the request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("the request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                const auto ch = request[i++];
                ioChannel.readBuffer().write(&ch, 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }

    GIVEN("requests without header value")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "GET / HTTP/1.1\r\nHost: example.com\r\nname:\r\n\r\n",
                                      "GET / HTTP/1.1\r\nHost: example.com\r\nname: \r\n\r\n",
                                      "GET / HTTP/1.1\r\nHost: example.com\r\nname:     \r\n\r\n");

        WHEN("the request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("request is successfully parsed")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());

                AND_THEN("parser extracts correct information from request data")
                {
                    const auto &request = parser.request();
                    REQUIRE(request.method() == HttpRequest::Method::GET);
                    REQUIRE(request.targetPath() == "/");
                    REQUIRE(request.targetQuery().empty());
                    REQUIRE(request.headerCount("Host") == 1);
                    REQUIRE(request.hasHeader("host"));
                    REQUIRE(request.headerCount("name") == 1);
                    REQUIRE(request.hasHeader("name"));
                    REQUIRE(request.header("name").empty());

                }
            }
        }

        WHEN("the request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                const auto ch = request[i++];
                ioChannel.readBuffer().write(&ch, 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("request is successfully parsed")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());

                AND_THEN("parser extracts correct information from request data")
                {
                    const auto &request = parser.request();
                    REQUIRE(request.method() == HttpRequest::Method::GET);
                    REQUIRE(request.targetPath() == "/");
                    REQUIRE(request.targetQuery().empty());
                    REQUIRE(request.headerCount("Host") == 1);
                    REQUIRE(request.hasHeader("host"));
                    REQUIRE(request.headerCount("name") == 1);
                    REQUIRE(request.hasHeader("name"));
                    REQUIRE(request.header("name").empty());

                }
            }
        }
    }

    GIVEN("invalid requests lacking the host header field")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "GET / HTTP/1.1\r\n\r\n",
                                      "GET / HTTP/1.1\r\nname:value\r\n\r\n",
                                      "GET / HTTP/1.1\r\nname1: value1\r\nname2: value2\r\n\r\n");

        WHEN("the request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("the request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                const auto ch = request[i++];
                ioChannel.readBuffer().write(&ch, 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }

    GIVEN("invalid requests with more than one host header field")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "GET / HTTP/1.1\r\nHost: example.com\r\nname: value\r\nhost: example.com\r\n\r\n",
                                      "GET / HTTP/1.1\r\nHost: example.com\r\nHost: example.com\r\n\r\n");

        WHEN("the request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("the request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                const auto ch = request[i++];
                ioChannel.readBuffer().write(&ch, 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }

    GIVEN("request containing all valid characters in header field name")
    {
        // field-name     = token (RFC9110, section 5.1)
        // token          = 1*tchar
        // tchar          = "!" / "#" / "$" / "%" / "&" / "'" / "*"
        //                  "+" / "-" / "." / "^" / "_" / "`" / "|" / "~"
        //                  DIGIT / ALPHA
        std::string headerFieldName;
        headerFieldName.reserve(128);
        for (char ch = 0; ch < 127; ++ch)
        {
            if (('a' <= ch && ch <= 'z')
                || ('A' <= ch && ch <= 'Z')
                || ('0' <= ch && ch <= '9')
                || ch == '!' || ch == '#' || ch == '$' || ch == '%' || ch == '&'
                || ch == '\'' || ch == '*' || ch == '+' || ch == '-' || ch == '.'
                || ch == '^' || ch == '_' || ch == '`' || ch == '|' || ch == '~')
                headerFieldName.push_back(ch);
        }
        std::string request;
        request.reserve(256);
        request.append("GET / HTTP/1.1\r\nHost: host.com\r\n")
            .append(headerFieldName)
            .append(": value\r\n")
            .append(headerFieldName)
            .append(":\r\n")
            .append(headerFieldName)
            .append(":        \r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser parses the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());
                const auto &request = parser.request();
                REQUIRE(request.hasHeader(headerFieldName));
                REQUIRE(request.headerCount(headerFieldName) == 3);
                REQUIRE(request.header(headerFieldName) == "value");
                REQUIRE(request.header(headerFieldName, 1) == "value");
                REQUIRE(request.header(headerFieldName, 2).empty());
                REQUIRE(request.header(headerFieldName, 3).empty());
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                ioChannel.readBuffer().write(&request[i++], 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser parses the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());
                const auto &request = parser.request();
                REQUIRE(request.hasHeader(headerFieldName));
                REQUIRE(request.headerCount(headerFieldName) == 3);
                REQUIRE(request.header(headerFieldName) == "value");
                REQUIRE(request.header(headerFieldName, 1) == "value");
                REQUIRE(request.header(headerFieldName, 2).empty());
                REQUIRE(request.header(headerFieldName, 3).empty());
            }
        }
    }

    GIVEN("request containing an invalid char as header field name")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "GET / HTTP/1.1\r\nHost: host.com\r\n\t: value\r\n\r\n",
                                      "GET / HTTP/1.1\r\nHost: host.com\r\n name: value\r\n\r\n",
                                      "GET / HTTP/1.1\r\nHost: host.com\r\nname : value\r\n\r\n",
                                      "GET / HTTP/1.1\r\nHost: host.com\r\n name : value\r\n\r\n",
                                      "GET / HTTP/1.1\r\nHost: host.com\r\n  name   : value\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                ioChannel.readBuffer().write(&request[i++], 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }

    GIVEN("request containing all invalid chars in header field name")
    {
        // field-name     = token (RFC9110, section 5.1)
        // token          = 1*tchar
        // tchar          = "!" / "#" / "$" / "%" / "&" / "'" / "*"
        //                  "+" / "-" / "." / "^" / "_" / "`" / "|" / "~" / ":"
        //                  DIGIT / ALPHA
        constexpr static size_t invalidCharsCount = (256 - (26 + 26 + 10 + 15 + 1));
        const static auto invalidChars = []() -> std::string
        {
            std::string temp;
            temp.reserve(256);
            for (int16_t ascii = std::numeric_limits<char>::min(); ascii <= std::numeric_limits<char>::max(); ++ascii)
            {
                char ch = static_cast<char>(ascii);
                if (('a' <= ch && ch <= 'z')
                    || ('A' <= ch && ch <= 'Z')
                    || ('0' <= ch && ch <= '9')
                    || ch == '!' || ch == '#' || ch == '$' || ch == '%' || ch == '&'
                    || ch == '\'' || ch == '*' || ch == '+' || ch == '-' || ch == '.'
                    || ch == '^' || ch == '_' || ch == '`' || ch == '|' || ch == '~' || ch == ':')
                    continue;
                else
                    temp.push_back(ch);
            }
            REQUIRE(temp.size() == invalidCharsCount);
            return temp;
        }();
        const auto idx = GENERATE_RANGE(AS(size_t), 0, invalidCharsCount - 1);
        std::string request;
        request.reserve(32);
        request.append("GET / HTTP/1.1\r\nHost: host.com\r\nna");
        request.push_back(invalidChars[idx]);
        request.append("me: value\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                ioChannel.readBuffer().write(&request[i++], 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }

    GIVEN("request containing an invalid char in header field name")
    {
        auto delta = GENERATE_RANGE(AS(size_t), 0, 128);
        std::string request;
        std::string headerFieldName = ((delta > 0) ? std::string(delta, 'a') : std::string{})
                                   + "\t"
                                   + (((128 - delta) > 0) ? std::string(128 - delta, 'a') : std::string{});
        request.reserve(256);
        request.append("GET / HTTP/1.1\r\nHost: host.com\r\n")
            .append(headerFieldName)
            .append(": value\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                ioChannel.readBuffer().write(&request[i++], 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }

    GIVEN("request containing all valid characters in header field value")
    {
        // field-value    = *field-content (RFC9110, section 5.5)
        // field-content  = field-vchar[ 1*( SP / HTAB / field-vchar ) field-vchar ] (RFC9110, section 5.5)
        // field-vchar    = VCHAR / obs-text (RFC9110, section 5.5)
        // obs-text       = %x80-FF (RFC9110, section 5.5)
        std::string headerFieldValue;
        headerFieldValue.reserve(256);
        headerFieldValue.push_back('a');
        for (int16_t ascii = std::numeric_limits<char>::min(); ascii <= std::numeric_limits<char>::max(); ++ascii)
        {
            if ((0 <= ascii && ascii <= 32) || ascii == 127)
                continue;
            headerFieldValue.push_back(static_cast<char>(ascii));
        }
        headerFieldValue.append("\t a");
        std::string request;
        request.reserve(256);
        request.append("GET / HTTP/1.1\r\nHost: host.com\r\n")
            .append("name:")
            .append(headerFieldValue)
            .append("\r\nname: ")
            .append(headerFieldValue)
            .append("\r\nname:")
            .append(headerFieldValue)
            .append(" \r\nname:  ")
            .append(headerFieldValue)
            .append("        \r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser parses the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());
                const auto &request = parser.request();
                REQUIRE(request.hasHeader("name"));
                REQUIRE(request.headerCount("name") == 4);
                REQUIRE(request.header("name") == headerFieldValue);
                REQUIRE(request.header("name", 1) == headerFieldValue);
                REQUIRE(request.header("name", 2) == headerFieldValue);
                REQUIRE(request.header("name", 3) == headerFieldValue);
                REQUIRE(request.header("name", 4) == headerFieldValue);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                ioChannel.readBuffer().write(&request[i++], 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser parses the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());
                const auto &request = parser.request();
                REQUIRE(request.hasHeader("name"));
                REQUIRE(request.headerCount("name") == 4);
                REQUIRE(request.header("name") == headerFieldValue);
                REQUIRE(request.header("name", 1) == headerFieldValue);
                REQUIRE(request.header("name", 2) == headerFieldValue);
                REQUIRE(request.header("name", 3) == headerFieldValue);
                REQUIRE(request.header("name", 4) == headerFieldValue);
            }
        }
    }

    GIVEN("request containing an invalid char as header field value")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "GET / HTTP/1.1\r\nHost: host.com\r\nname:\1\r\n\r\n",
                                      "GET / HTTP/1.1\r\nHost: host.com\r\nname: \1\r\n\r\n",
                                      "GET / HTTP/1.1\r\nHost: host.com\r\nname:\1 \r\n\r\n",
                                      "GET / HTTP/1.1\r\nHost: host.com\r\nname:   \1  \r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                ioChannel.readBuffer().write(&request[i++], 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }

    GIVEN("request containing all invalid chars in header field value")
    {
        // field-value    = *field-content (RFC9110, section 5.5)
        // field-content  = field-vchar[ 1*( SP / HTAB / field-vchar ) field-vchar ] (RFC9110, section 5.5)
        // field-vchar    = VCHAR / obs-text (RFC9110, section 5.5)
        // obs-text       = %x80-FF (RFC9110, section 5.5)
        constexpr static size_t invalidCharsCount = 31;
        const static auto invalidChars = []() -> std::string
        {
            std::string temp;
            temp.reserve(256);
            for (int16_t ascii = std::numeric_limits<char>::min(); ascii <= std::numeric_limits<char>::max(); ++ascii)
            {
                if ((0 <= ascii && ascii < 32 && ascii != 9 && ascii != 13) || ascii == 127)
                    temp.push_back(static_cast<char>(ascii));
            }
            REQUIRE(temp.size() == invalidCharsCount);
            return temp;
        }();
        const auto idx = GENERATE_RANGE(AS(size_t), 0, invalidCharsCount - 1);
        std::string request;
        request.reserve(32);
        request.append("GET / HTTP/1.1\r\nHost: host.com\r\nname: va");
        request.push_back(invalidChars[idx]);
        request.append("lue\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                ioChannel.readBuffer().write(&request[i++], 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }

    GIVEN("request containing an invalid char in header field value")
    {
        auto delta = GENERATE_RANGE(AS(size_t), 0, 128);
        std::string request;
        std::string headerFieldValue = ((delta > 0) ? std::string(delta, 'a') : std::string{})
                                      + "\1"
                                      + (((128 - delta) > 0) ? std::string(128 - delta, 'a') : std::string{});
        request.reserve(256);
        request.append("GET / HTTP/1.1\r\nHost: host.com\r\nname: ")
            .append(headerFieldValue)
            .append("\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser::ParserStatus parserStatus;
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            int i = 0;
            do
            {
                ioChannel.readBuffer().write(&request[i++], 1);
                parserStatus = parser.parse();
            } while (HttpRequestParser::ParserStatus::NeedsMoreData == parserStatus);

            THEN("parser fails to parse the request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }
}


SCENARIO("HttpRequestParser allows spaces around content-length value")
{
    GIVEN("a request with content-length header value containing spaces before/after the value")
    {
        const auto contentLengthValue = GENERATE(AS(std::pair<size_t, std::string_view>),
                                                  {2305, "2305"}, {5847, " 5847"}, {17, "17 "}, {65535, "      65535   "},
                                                  {1773455, "\t1773455"}, {0, "0\t"}, {72, "  \t \t  72\t\t\t   \t"});
        std::string request;
        request.reserve(32);
        request.append("POST / HTTP/1.1\r\nHost: example.com\r\nContent-Length:").append(contentLengthValue.second).append("\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("request is successfully parsed")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());

                AND_THEN("parser extracts the correct information from the request data")
                {
                    const auto &httpRequest = parser.request();
                    REQUIRE(httpRequest.hasHeader("Content-Length"));
                    REQUIRE(httpRequest.headerCount("Content-Length") == 1);
                    REQUIRE(httpRequest.header("Content-Length") == std::to_string(contentLengthValue.first));
                }
            }
        }

        WHEN("the request is parsed byte by byte")
        {
            IOChannelTest ioChannel(std::string_view(request.data(), 1));
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            for (int i = 1; i < (request.size() - 1); ++i)
            {
                ioChannel.readBuffer().write(&request[i], 1);
                REQUIRE(HttpRequestParser::ParserStatus::NeedsMoreData == parser.parse());
            }
            ioChannel.readBuffer().write(&request[request.size() - 1], 1);
            const auto parserStatus = parser.parse();

            THEN("the request is successfully parsed")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());

                AND_THEN("the parser extracts the correct information from the request data")
                {
                    const auto &httpRequest = parser.request();
                    REQUIRE(httpRequest.hasHeader("Content-Length"));
                    REQUIRE(httpRequest.headerCount("Content-Length") == 1);
                    REQUIRE(httpRequest.header("Content-Length") == std::to_string(contentLengthValue.first));
                }
            }
        }
    }
}


SCENARIO("HttpRequestParser only accepts digits in trimmed content-length value")
{
    GIVEN("a request with content-length header value containing trimmed value with non digit characters")
    {
        const auto contentLengthValue = GENERATE(AS(std::string_view), "+10", "0xFF", "five", "-3", "0x11111");
        std::string request;
        request.reserve(32);
        request.append("POST / HTTP/1.1\r\nHost: example.com\r\nContent-Length:").append(contentLengthValue).append("\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("the request is parsed byte by byte")
        {
            IOChannelTest ioChannel(std::string_view(request.data(), 1));
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            for (int i = 1; i < (request.size() - 1); ++i)
            {
                ioChannel.readBuffer().write(&request[i], 1);
                REQUIRE(HttpRequestParser::ParserStatus::NeedsMoreData == parser.parse());
            }
            ioChannel.readBuffer().write(&request[request.size() - 1], 1);
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }
}


SCENARIO("HttpRequestParser does not accept content-length values with more than 19 digits")
{
    GIVEN("a request with content-length header value larger than 19 digits")
    {
        const auto contentLengthValue = GENERATE(AS(std::string_view), "10000000000000000000", "98765432123456789098");
        std::string request;
        request.reserve(32);
        request.append("POST / HTTP/1.1\r\nHost: example.com\r\nContent-Length:").append(contentLengthValue).append("\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("the request is parsed byte by byte")
        {
            IOChannelTest ioChannel(std::string_view(request.data(), 1));
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            for (int i = 1; i < (request.size() - 1); ++i)
            {
                ioChannel.readBuffer().write(&request[i], 1);
                REQUIRE(HttpRequestParser::ParserStatus::NeedsMoreData == parser.parse());
            }
            ioChannel.readBuffer().write(&request[request.size() - 1], 1);
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }
}


SCENARIO("HttpRequestParser does not accept empty content-length values or values containing only spaces")
{
    GIVEN("a request with content-length header values that are empty or only containing spaces")
    {
        const auto contentLengthValue = GENERATE(AS(std::string_view), "", " ", "\t", " \t", "\t ", "    ", "\t\t\t\t\t\t", "  \t \t\t    \t");
        std::string request;
        request.reserve(32);
        request.append("POST / HTTP/1.1\r\nHost: example.com\r\nContent-Length:").append(contentLengthValue).append("\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("the request is parsed byte by byte")
        {
            IOChannelTest ioChannel(std::string_view(request.data(), 1));
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            for (int i = 1; i < (request.size() - 1); ++i)
            {
                ioChannel.readBuffer().write(&request[i], 1);
                REQUIRE(HttpRequestParser::ParserStatus::NeedsMoreData == parser.parse());
            }
            ioChannel.readBuffer().write(&request[request.size() - 1], 1);
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }
}


SCENARIO("HttpRequestParser only accepts multiple content-length field lines when all of them have the same trimmed value")
{
    GIVEN("a request with multiple content-length field lines all with same value")
    {
        const auto contentLengthValue = GENERATE(AS(std::string), "2305", "5847", "17", "65535", "1773455", "0", "72");
        std::string request;
        request.reserve(32);
        request.append("POST / HTTP/1.1\r\nHost: example.com\r\n")
            .append("Content-Length:").append(contentLengthValue).append("\r\n")
            .append("Content-Length: ").append(contentLengthValue).append("\r\n")
            .append("Content-Length:").append(contentLengthValue).append(" \r\n")
            .append("Content-Length: ").append(contentLengthValue).append(" \r\n")
            .append("Content-Length:\t").append(contentLengthValue).append("\r\n")
            .append("Content-Length:").append(contentLengthValue).append("\t\r\n")
            .append("Content-Length:\t").append(contentLengthValue).append("\t\r\n")
            .append("Content-Length: \t\t ").append(contentLengthValue).append("\t\t  \t \t \r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("request is successfully parsed")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());

                AND_THEN("parser extracts the correct information from the request data")
                {
                    const auto &httpRequest = parser.request();
                    REQUIRE(httpRequest.hasHeader("Content-Length"));
                    REQUIRE(httpRequest.headerCount("Content-Length") == 8);
                    REQUIRE(httpRequest.header("Content-Length") == contentLengthValue);
                    REQUIRE(httpRequest.requestBodySize() == std::stoull(contentLengthValue));
                }
            }
        }

        WHEN("the request is parsed byte by byte")
        {
            IOChannelTest ioChannel(std::string_view(request.data(), 1));
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            for (int i = 1; i < (request.size() - 1); ++i)
            {
                ioChannel.readBuffer().write(&request[i], 1);
                REQUIRE(HttpRequestParser::ParserStatus::NeedsMoreData == parser.parse());
            }
            ioChannel.readBuffer().write(&request[request.size() - 1], 1);
            const auto parserStatus = parser.parse();

            THEN("the request is successfully parsed")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());

                AND_THEN("the parser extracts the correct information from the request data")
                {
                    const auto &httpRequest = parser.request();
                    REQUIRE(httpRequest.hasHeader("Content-Length"));
                    REQUIRE(httpRequest.headerCount("Content-Length") == 8);
                    REQUIRE(httpRequest.header("Content-Length") == contentLengthValue);
                    REQUIRE(httpRequest.requestBodySize() == std::stoull(contentLengthValue));
                }
            }
        }
    }

    GIVEN("a request containing multiple content-length entries with different trimmed values")
    {
        std::string_view request("POST / HTTP/1.1\r\nHost: example.com\r\nContent-Length: 22\r\nContent-Length: 25\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("the request is parsed byte by byte")
        {
            IOChannelTest ioChannel(std::string_view(request.data(), 1));
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            for (int i = 1; i < (request.size() - 1); ++i)
            {
                ioChannel.readBuffer().write(&request[i], 1);
                REQUIRE(HttpRequestParser::ParserStatus::NeedsMoreData == parser.parse());
            }
            ioChannel.readBuffer().write(&request[request.size() - 1], 1);
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }
}


SCENARIO("HttpRequestParser parses http requests with headers and body")
{
    GIVEN("a single http request with headers and body")
    {
        const auto httpMethod = GENERATE(AS(std::string_view), "POST", "PUT");
        const auto urlPath = GENERATE(AS(std::string_view), "/", "/an/absolute/path");
        const auto urlQuery = GENERATE(AS(std::string_view), "", "a_query");
        const auto headersBlock = GENERATE(AS(std::vector<std::pair<std::string_view, std::string_view>>),
                                           {{"host", " example.com"}},
                                           {{"name", " value"}, {"host", " example.com"}},
                                           {{"host", " example.com"}, {"name1", " value1"}, {"name2", "  value2 "}, {"name3", " va l \t\t ue\t3"}},
                                           {{"Host", " www.example.com"},
                                            {"Referer", " vulnerable.host.net"},
                                            {"Connection", " keep-alive"},
                                            {"Upgrade-Insecure-Requests", " 1"},
                                            {"User-Agent", " Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/52.0.2743.116 Safari/537.36"},
                                            {"Accept", " text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8"},
                                            {"Accept-Encoding", " gzip, deflate, sdch"},
                                            {"Accept-Language", " en-US,en;q=0.8,ru;q=0.6"},
                                            {"Cookie", " a=sdfasd; sdf=3242u389erfhhs; djcnjhe=sdfsdafsdjfb324te1267dd; sdaf=mo2u8943478t67437461746rfdgfcdc; ityu=9u489573484duifhd; GTYFT=nsdjhcbyq3te76ewgfcZ; uityut=23Y746756247856425784657; GA=URHUFVHHVSDNFDHGYSDGF; a=%45345%dfdfg %4656%4534sdfjhsdb.sdfsg.sdfgsf.; aa=4583478; aaaaa=34435345; rrr=iy7t67t6tsdf; ggg=234i5y24785y78ry534785; sdf=3242u389erfhhs; ityu=9u489573484duifhd; GTYFT=nsdjhcbyq3te76ewgfcZ; uityut=23Y746756247856425784657; GA=URHUFVHHVSDNFDHGYSDGF; a=%45345%dfdfg %4656%4534sdfjhsdb.sdfsg.sdfgsf.; nsdjhfb=4358345y; jkbsdff=aaaa; aa=4583478; ggg=234i5y24785y78ry534785; mmm=23uy47fbhdsfbgh; bsdfhbhfgdqqwew=883476757%345345; jksdfb=2348y; ndfsgsfdg=235trHHVGHFGC; erertrt=3242342343423324234; g=888888888788"}});
        const auto body = GENERATE(AS(std::string_view), "This is the body data.", "name=\"Jhon Doe\";age=27;height=1.79m");

        std::string request;
        request.reserve(256);
        request.append(httpMethod)
            .append(" ")
            .append(urlPath).append(urlQuery.empty() ? "" : "?").append(urlQuery)
            .append(" HTTP/1.1\r\n");
        for (const auto &field : headersBlock)
            request.append(field.first).append(":").append(field.second).append("\r\n");
        request.append("Content-Length: ").append(std::to_string(body.size())).append("\r\n\r\n").append(body);

        WHEN("the request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("the request is successfully parsed")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());

                AND_THEN("the parser extracts the correct information from the request data")
                {
                    const auto &request = parser.request();
                    switch(request.method())
                    {
                        case HttpRequest::Method::GET:
                            REQUIRE(httpMethod == "GET");
                            break;
                        case HttpRequest::Method::PUT:
                            REQUIRE(httpMethod == "PUT");
                            break;
                        case HttpRequest::Method::POST:
                            REQUIRE(httpMethod == "POST");
                            break;
                        case HttpRequest::Method::PATCH:
                            REQUIRE(httpMethod == "PATCH");
                            break;
                        case HttpRequest::Method::DELETE:
                            REQUIRE(httpMethod == "DELETE");
                            break;
                        case HttpRequest::Method::HEAD:
                            REQUIRE(httpMethod == "HEAD");
                            break;
                        case HttpRequest::Method::OPTIONS:
                            REQUIRE(httpMethod == "OPTIONS");
                            break;
                        default:
                            FAIL("This code is supposed to be unreachable.");
                    }
                    REQUIRE(urlPath == request.targetPath());
                    REQUIRE(urlQuery == request.targetQuery());
                    REQUIRE(0 == request.headerCount(""));
                    REQUIRE(1 == request.headerCount("Host"));
                    REQUIRE(0 == request.headerCount("AValidHeaderName"));
                    REQUIRE(0 == request.headerCount("An!nvalid\tHeaderName"));
                    REQUIRE(request.headersCount() == (headersBlock.size() + 1));
                    for (const auto &field : headersBlock)
                    {
                        REQUIRE(request.hasHeader(field.first));
                        REQUIRE(request.headerCount(field.first) == 1);
                        const auto headerValue = request.header(field.first);
                        REQUIRE(QByteArray(headerValue.data(), headerValue.size()) == QByteArray(field.second.data(), field.second.size()).trimmed());
                    }
                    REQUIRE(request.hasHeader("Content-Length"));
                    REQUIRE(request.headerCount("Content-Length") == 1);
                    REQUIRE(request.header("Content-Length") == std::to_string(body.size()));
                    REQUIRE(request.isComplete());
                    REQUIRE(!request.chunked());
                    REQUIRE(request.requestBodySize() == body.size());
                    REQUIRE(request.pendingBodySize() == 0);
                    REQUIRE(request.hasBody());
                    REQUIRE(request.body() == body);
                    REQUIRE(request.bodyType() == HttpRequest::BodyType::NotChunked);
                }
            }
        }

        WHEN("the request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            size_t index = 0;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[index++], 1);
                parserStatus = parser.parse();
            }

            THEN("the request is successfully parsed")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE((request.size() - body.size()) == parser.requestSize());

                AND_THEN("the parser extracts the correct information from the request data")
                {
                    const auto &httpRequest = parser.request();
                    switch(httpRequest.method())
                    {
                        case HttpRequest::Method::GET:
                            REQUIRE(httpMethod == "GET");
                            break;
                        case HttpRequest::Method::PUT:
                            REQUIRE(httpMethod == "PUT");
                            break;
                        case HttpRequest::Method::POST:
                            REQUIRE(httpMethod == "POST");
                            break;
                        case HttpRequest::Method::PATCH:
                            REQUIRE(httpMethod == "PATCH");
                            break;
                        case HttpRequest::Method::DELETE:
                            REQUIRE(httpMethod == "DELETE");
                            break;
                        case HttpRequest::Method::HEAD:
                            REQUIRE(httpMethod == "HEAD");
                            break;
                        case HttpRequest::Method::OPTIONS:
                            REQUIRE(httpMethod == "OPTIONS");
                            break;
                        default:
                            FAIL("This code is supposed to be unreachable.");
                    }
                    REQUIRE(urlPath == httpRequest.targetPath());
                    REQUIRE(urlQuery == httpRequest.targetQuery());
                    REQUIRE(0 == httpRequest.headerCount(""));
                    REQUIRE(1 == httpRequest.headerCount("Host"));
                    REQUIRE(0 == httpRequest.headerCount("AValidHeaderName"));
                    REQUIRE(0 == httpRequest.headerCount("An!nvalid\tHeaderName"));
                    REQUIRE(httpRequest.headersCount() == (headersBlock.size() + 1));
                    for (const auto &field : headersBlock)
                    {
                        REQUIRE(httpRequest.hasHeader(field.first));
                        REQUIRE(httpRequest.headerCount(field.first) == 1);
                        const auto headerValue = httpRequest.header(field.first);
                        REQUIRE(QByteArray(headerValue.data(), headerValue.size()) == QByteArray(field.second.data(), field.second.size()).trimmed());
                    }
                    REQUIRE(httpRequest.hasHeader("Content-Length"));
                    REQUIRE(httpRequest.headerCount("Content-Length") == 1);
                    REQUIRE(httpRequest.header("Content-Length") == std::to_string(body.size()));
                    REQUIRE(!httpRequest.isComplete());
                    REQUIRE(!httpRequest.chunked());
                    REQUIRE(httpRequest.requestBodySize() == body.size());
                    REQUIRE(httpRequest.pendingBodySize() == body.size());
                    REQUIRE(!httpRequest.hasBody());
                    REQUIRE(httpRequest.body().empty());
                    REQUIRE(httpRequest.bodyType() == HttpRequest::BodyType::NotChunked);
                    for (size_t bodyIdx = 0; bodyIdx < body.size(); ++bodyIdx)
                    {
                        REQUIRE(!httpRequest.isComplete());
                        char ch = request[index + bodyIdx];
                        ioChannel.readBuffer().write(&ch, 1);
                        REQUIRE(HttpRequestParser::ParserStatus::ParsedBody == parser.parse());
                        REQUIRE(httpRequest.pendingBodySize() == (body.size() - bodyIdx - 1));
                        REQUIRE(httpRequest.hasBody());
                        REQUIRE(httpRequest.body().size() == 1);
                        REQUIRE(httpRequest.body()[0] == ch);
                    }
                    REQUIRE(httpRequest.isComplete());
                    REQUIRE(parser.parse() == HttpRequestParser::ParserStatus::NeedsMoreData);
                }
            }
        }
    }

    GIVEN("multiple http requests with headers and body")
    {
        const std::vector<std::string_view> httpMethods({"POST", "PATCH"});
        const std::vector<std::string_view> urlPaths({"/", "/an/absolute/path"});
        const std::vector<std::string_view> urlQueries({"", "a_query"});
        const std::vector<std::vector<std::pair<std::string_view, std::string_view>>> headersBlocks({{{"host", " example.com"}},
                                                                                                     {{"name", " value"}, {"host", " example.com"}},
                                                                                                     {{"host", " example.com"}, {"name1", " value1"}, {"name2", "  value2 "}, {"name3", " va l \t\t ue\t3"}},
                                                                                                     {{"Host", " www.example.com"},
                                                                                                      {"Referer", " vulnerable.host.net"},
                                                                                                      {"Connection", " keep-alive"},
                                                                                                      {"Upgrade-Insecure-Requests", " 1"},
                                                                                                      {"User-Agent", " Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/52.0.2743.116 Safari/537.36"},
                                                                                                      {"Accept", " text/html,application/xhtml+xml,application/xml;q=0.9,image/webp"},
                                                                                                      {"Accept-Encoding", " gzip, deflate, sdch"},
                                                                                                      {"Accept-Language", " en-US,en;q=0.8,ru;q=0.6"},
                                                                                                      {"Cookie", " a=sdfasd; sdf=3242u389erfhhs; djcnjhe=sdfsdafsdjfb324te1267dd; sdaf=mo2u8943478t67437461746rfdgfcdc; ityu=9u489573484duifhd; GTYFT=nsdjhcbyq3te76ewgfcZ; uityut=23Y746756247856425784657; GA=URHUFVHHVSDNFDHGYSDGF; a=%45345%dfdfg %4656%4534sdfjhsdb.sdfsg.sdfgsf.; aa=4583478; aaaaa=34435345; rrr=iy7t67t6tsdf; ggg=234i5y24785y78ry534785; sdf=3242u389erfhhs; ityu=9u489573484duifhd; GTYFT=nsdjhcbyq3te76ewgfcZ; uityut=23Y746756247856425784657; GA=URHUFVHHVSDNFDHGYSDGF; a=%45345%dfdfg %4656%4534sdfjhsdb.sdfsg.sdfgsf.; nsdjhfb=4358345y; jkbsdff=aaaa; aa=4583478; ggg=234i5y24785y78ry534785; mmm=23uy47fbhdsfbgh; bsdfhbhfgdqqwew=883476757%345345; jksdfb=2348y; ndfsgsfdg=235trHHVGHFGC; erertrt=3242342343423324234; g=888888888788"}}});
        const std::vector<std::string_view> bodies({"This is the body data.", "name=\"Jhon Doe\";age=27;height=1.79m"});
        std::string requests;
        requests.reserve(65536);
        for (const auto &httpMethod : httpMethods)
        {
            for (const auto &urlPath : urlPaths)
            {
                for (const auto &urlQuery : urlQueries)
                {
                    for (const auto &headersBlock : headersBlocks)
                    {
                        for (const auto &body : bodies)
                        {
                            requests.append(httpMethod)
                                    .append(" ")
                                    .append(urlPath).append(urlQuery.empty() ? "" : "?").append(urlQuery)
                                    .append(" HTTP/1.1\r\n");
                            for (const auto &field : headersBlock)
                                requests.append(field.first).append(":").append(field.second).append("\r\n");
                            requests.append("Content-Length: ").append(std::to_string(body.size())).append("\r\n\r\n").append(body);
                        }
                    }
                }
            }
        }

        WHEN("parser processes data from all requests at once")
        {
            IOChannelTest ioChannel(requests);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));

            THEN("all requests are successfully parsed")
            {
                for (const auto &httpMethod : httpMethods)
                {
                    for (const auto &urlPath : urlPaths)
                    {
                        for (const auto &urlQuery : urlQueries)
                        {
                            for (const auto &headersBlock : headersBlocks)
                            {
                                for (const auto &body : bodies)
                                {
                                    std::string currentRequest;
                                    currentRequest.reserve(2048);
                                    currentRequest.append(httpMethod)
                                        .append(" ")
                                        .append(urlPath).append(urlQuery.empty() ? "" : "?").append(urlQuery)
                                        .append(" HTTP/1.1\r\n");
                                    for (const auto &field : headersBlock)
                                        currentRequest.append(field.first).append(":").append(field.second).append("\r\n");
                                    currentRequest.append("Content-Length: ").append(std::to_string(body.size())).append("\r\n\r\n").append(body);
                                    const auto parserStatus = parser.parse();
                                    REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                                    REQUIRE(currentRequest.size() == parser.requestSize());
                                    const auto &request = parser.request();
                                    switch(request.method())
                                    {
                                        case HttpRequest::Method::GET:
                                            REQUIRE(httpMethod == "GET");
                                            break;
                                        case HttpRequest::Method::PUT:
                                            REQUIRE(httpMethod == "PUT");
                                            break;
                                        case HttpRequest::Method::POST:
                                            REQUIRE(httpMethod == "POST");
                                            break;
                                        case HttpRequest::Method::PATCH:
                                            REQUIRE(httpMethod == "PATCH");
                                            break;
                                        case HttpRequest::Method::DELETE:
                                            REQUIRE(httpMethod == "DELETE");
                                            break;
                                        case HttpRequest::Method::HEAD:
                                            REQUIRE(httpMethod == "HEAD");
                                            break;
                                        case HttpRequest::Method::OPTIONS:
                                            REQUIRE(httpMethod == "OPTIONS");
                                            break;
                                        default:
                                            FAIL("This code is supposed to be unreachable.");
                                    }
                                    REQUIRE(urlPath == request.targetPath());
                                    REQUIRE(urlQuery == request.targetQuery());
                                    REQUIRE(0 == request.headerCount(""));
                                    REQUIRE(1 == request.headerCount("Host"));
                                    REQUIRE(0 == request.headerCount("AValidHeaderName"));
                                    REQUIRE(0 == request.headerCount("An!nvalid\tHeaderName"));
                                    REQUIRE(request.headersCount() == (headersBlock.size() + 1));
                                    for (const auto &field : headersBlock)
                                    {
                                        REQUIRE(request.hasHeader(field.first));
                                        REQUIRE(request.headerCount(field.first) == 1);
                                        const auto headerValue = request.header(field.first);
                                        REQUIRE(QByteArray(headerValue.data(), headerValue.size()) == QByteArray(field.second.data(), field.second.size()).trimmed());
                                    }
                                    REQUIRE(request.hasHeader("Content-Length"));
                                    REQUIRE(request.headerCount("Content-Length") == 1);
                                    REQUIRE(request.header("Content-Length") == std::to_string(body.size()));
                                    REQUIRE(request.isComplete());
                                    REQUIRE(!request.chunked());
                                    REQUIRE(request.requestBodySize() == body.size());
                                    REQUIRE(request.pendingBodySize() == 0);
                                    REQUIRE(request.hasBody());
                                    REQUIRE(request.body() == body);
                                    REQUIRE(request.bodyType() == HttpRequest::BodyType::NotChunked);
                                }
                            }
                        }
                    }
                }
                const auto parserStatus = parser.parse();
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData);
            }
        }

        WHEN("parser processes data from all requests byte by byte")
        {
            size_t index = 0;
            IOChannelTest ioChannel(std::string_view(requests.data(), 1));
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));

            THEN("all requests are successfully parsed")
            {
                for (const auto &httpMethod : httpMethods)
                {
                    for (const auto &urlPath : urlPaths)
                    {
                        for (const auto &urlQuery : urlQueries)
                        {
                            for (const auto &headersBlock : headersBlocks)
                            {
                                for (const auto &body : bodies)
                                {
                                    std::string currentRequest;
                                    currentRequest.reserve(2048);
                                    currentRequest.append(httpMethod)
                                        .append(" ")
                                        .append(urlPath).append(urlQuery.empty() ? "" : "?").append(urlQuery)
                                        .append(" HTTP/1.1\r\n");
                                    for (const auto &field : headersBlock)
                                        currentRequest.append(field.first).append(":").append(field.second).append("\r\n");
                                    currentRequest.append("Content-Length: ").append(std::to_string(body.size())).append("\r\n\r\n").append(body);
                                    auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
                                    while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
                                    {
                                        ioChannel.readBuffer().write(&requests[++index], 1);
                                        parserStatus = parser.parse();
                                    }
                                    REQUIRE(HttpRequestParser::ParserStatus::ParsedRequest == parserStatus);
                                    REQUIRE((currentRequest.size() - body.size()) == parser.requestSize());
                                    const auto &httpRequest = parser.request();
                                    switch(httpRequest.method())
                                    {
                                        case HttpRequest::Method::GET:
                                            REQUIRE(httpMethod == "GET");
                                            break;
                                        case HttpRequest::Method::PUT:
                                            REQUIRE(httpMethod == "PUT");
                                            break;
                                        case HttpRequest::Method::POST:
                                            REQUIRE(httpMethod == "POST");
                                            break;
                                        case HttpRequest::Method::PATCH:
                                            REQUIRE(httpMethod == "PATCH");
                                            break;
                                        case HttpRequest::Method::DELETE:
                                            REQUIRE(httpMethod == "DELETE");
                                            break;
                                        case HttpRequest::Method::HEAD:
                                            REQUIRE(httpMethod == "HEAD");
                                            break;
                                        case HttpRequest::Method::OPTIONS:
                                            REQUIRE(httpMethod == "OPTIONS");
                                            break;
                                        default:
                                            FAIL("This code is supposed to be unreachable.");
                                    }
                                    REQUIRE(urlPath == httpRequest.targetPath());
                                    REQUIRE(urlQuery == httpRequest.targetQuery());
                                    REQUIRE(0 == httpRequest.headerCount(""));
                                    REQUIRE(1 == httpRequest.headerCount("Host"));
                                    REQUIRE(0 == httpRequest.headerCount("AValidHeaderName"));
                                    REQUIRE(0 == httpRequest.headerCount("An!nvalid\tHeaderName"));
                                    REQUIRE(httpRequest.headersCount() == (headersBlock.size() + 1));
                                    for (const auto &field : headersBlock)
                                    {
                                        REQUIRE(httpRequest.hasHeader(field.first));
                                        REQUIRE(httpRequest.headerCount(field.first) == 1);
                                        const auto headerValue = httpRequest.header(field.first);
                                        REQUIRE(QByteArray(headerValue.data(), headerValue.size()) == QByteArray(field.second.data(), field.second.size()).trimmed());
                                    }
                                    REQUIRE(httpRequest.hasHeader("Content-Length"));
                                    REQUIRE(httpRequest.headerCount("Content-Length") == 1);
                                    REQUIRE(httpRequest.header("Content-Length") == std::to_string(body.size()));
                                    REQUIRE(!httpRequest.isComplete());
                                    REQUIRE(!httpRequest.chunked());
                                    REQUIRE(httpRequest.requestBodySize() == body.size());
                                    REQUIRE(httpRequest.pendingBodySize() == body.size());
                                    REQUIRE(!httpRequest.hasBody());
                                    REQUIRE(httpRequest.body().empty());
                                    REQUIRE(httpRequest.bodyType() == HttpRequest::BodyType::NotChunked);
                                    for (size_t bodyIdx = 0; bodyIdx < body.size(); ++bodyIdx)
                                    {
                                        REQUIRE(!httpRequest.isComplete());
                                        char ch = requests[index + bodyIdx];
                                        ioChannel.readBuffer().write(&ch, 1);
                                        REQUIRE(HttpRequestParser::ParserStatus::ParsedBody == parser.parse());
                                        REQUIRE(httpRequest.pendingBodySize() == (body.size() - bodyIdx - 1));
                                        REQUIRE(httpRequest.hasBody());
                                        REQUIRE(httpRequest.body().size() == 1);
                                        REQUIRE(httpRequest.body()[0] == ch);
                                    }
                                    REQUIRE(httpRequest.isComplete());
                                    index += body.size();
                                }
                            }
                        }
                    }
                }
                REQUIRE(parser.parse() == HttpRequestParser::ParserStatus::NeedsMoreData);
            }
        }
    }
}


SCENARIO("HttpRequestParser processes all available request body when parsing request metadata (request line + headers)")
{
    GIVEN("a request with body")
    {
        const std::string_view bodyData("This is the body data");
        const std::string_view request("POST / HTTP/1.1\r\nHost: example.com\r\nContent-Length: 21\r\n\r\nThis is the body data");
        const auto bodySizeToKeep = GENERATE_RANGE(AS(size_t), 0, 21);
        std::string_view trimmedRequest(request.data(), request.size() - 21 + bodySizeToKeep);

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(trimmedRequest);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser processes body data available after request metadata is parsed")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(trimmedRequest.size() == parser.requestSize());

                AND_THEN("parser extracts the correct information from the request data")
                {
                    const auto &httpRequest = parser.request();
                    REQUIRE(bodySizeToKeep == 0 || httpRequest.hasBody());
                    REQUIRE(httpRequest.isComplete() == (bodySizeToKeep == 21));
                    REQUIRE(httpRequest.requestBodySize() == 21);
                    REQUIRE(httpRequest.pendingBodySize() == (21 - bodySizeToKeep));
                    REQUIRE(httpRequest.body().size() == bodySizeToKeep);
                    REQUIRE(!httpRequest.hasBody() || bodyData.starts_with(httpRequest.body()));
                }
            }
        }
    }
}


SCENARIO("HttpRequestParser does not allow transfer-encoding trimmed values that do not end with chunked transfer-coding without parameters or weight")
{
    GIVEN("requests with transfer-encoding entries whose trimmed values end with chunked")
    {
        const auto transferEncodingValue = GENERATE(AS(std::string_view),
                                                    "chunked",
                                                    " \t chunked\t  ",
                                                    "gzip, chunked",
                                                    "token1 ; token2 = \"blah\"; q=1.000, chunked");
        std::string request;
        request.reserve(64);
        request.append("POST / HTTP/1.1\r\nHost: example.com\r\nTransfer-Encoding: ").append(transferEncodingValue).append("\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser parses request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());

                AND_THEN("parser extracts the correct information from request")
                {
                    const auto &httpRequest = parser.request();
                    REQUIRE(httpRequest.chunked());
                }
            }
        }

        WHEN("the request is parsed byte by byte")
        {
            IOChannelTest ioChannel(std::string_view(request.data(), 1));
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            for (int i = 1; i < (request.size() - 1); ++i)
            {
                ioChannel.readBuffer().write(&request[i], 1);
                REQUIRE(HttpRequestParser::ParserStatus::NeedsMoreData == parser.parse());
            }
            ioChannel.readBuffer().write(&request[request.size() - 1], 1);
            const auto parserStatus = parser.parse();

            THEN("parser parses request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());

                AND_THEN("parser extracts the correct information from request")
                {
                    const auto &httpRequest = parser.request();
                    REQUIRE(httpRequest.chunked());
                }
            }
        }
    }

    GIVEN("requests with transfer-encoding entries whose trimmed values do not end with chunked")
    {
        const auto transferEncodingValue = GENERATE(AS(std::string_view),
                                                    "gzip",
                                                    " \t blah\t  ",
                                                    "not that word here",
                                                    "chunked ; text=\"chunk must not contain transfer parameters\"",
                                                    "chunked ; text=\"chunk must not contain weight\" ; q=0.5",
                                                    "anything_goes_before_chunked",
                                                    "\t  \tanything_goes_before_chunked\t\t\t  \t ");
        std::string request;
        request.reserve(64);
        request.append("POST / HTTP/1.1\r\nHost: example.com\r\nTransfer-Encoding: ").append(transferEncodingValue).append("\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("the request is parsed byte by byte")
        {
            IOChannelTest ioChannel(std::string_view(request.data(), 1));
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            size_t index = 0;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[++index], 1);
                parserStatus = parser.parse();
            }

            THEN("parser fails to parse request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }
}


SCENARIO("HttpRequestParser does not allow requests containing both transfer-encoding and content-length header fields")
{
    GIVEN("requests containing both transfer-encoding and content-length")
    {
        const auto contentLengthValue = GENERATE(AS(std::string_view), "0", "1234");
        const auto transferEncodingValue = GENERATE(AS(std::string_view),
                                                    "chunked",
                                                    " \t chunked\t  ",
                                                    "gzip, chunked",
                                                    "token1 ; token2 = \"blah\"; q=1.000, chunked");
        const auto contentLengthFirst = GENERATE(AS(bool), true, false);
        std::string request;
        request.reserve(64);
        request.append("POST / HTTP/1.1\r\nHost: example.com\r\n");
        if (contentLengthFirst)
        {
            request.append("content-length: ").append(contentLengthValue).append("\r\n");
            request.append("transfer-encoding: ").append(transferEncodingValue).append("\r\n");
        }
        else
        {
            request.append("transfer-encoding: ").append(transferEncodingValue).append("\r\n");
            request.append("content-length: ").append(contentLengthValue).append("\r\n");
        }
        request.append("\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("the request is parsed byte by byte")
        {
            IOChannelTest ioChannel(std::string_view(request.data(), 1));
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            size_t index = 0;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[++index], 1);
                parserStatus = parser.parse();
            }

            THEN("parser fails to parse request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }
}


SCENARIO("HttpRequestParser does not allow multiple transfer-encoding entries")
{
    GIVEN("requests with transfer-encoding entries whose trimmed values end with chunked")
    {
        const auto firstTransferEncodingValue = GENERATE(AS(std::string_view),
                                                         "chunked",
                                                         " \t chunked\t  ",
                                                         "gzip, chunked",
                                                         "token1 ; token2 = \"blah\"; q=1.000, chunked");
        const auto secondTransferEncodingValue = GENERATE(AS(std::string_view),
                                                          "chunked",
                                                          " \t chunked\t  ",
                                                          "gzip, chunked",
                                                          "token1 ; token2 = \"blah\"; q=1.000, chunked");
        std::string request;
        request.reserve(64);
        request.append("POST / HTTP/1.1\r\nHost: example.com\r\n");
        request.append("Transfer-Encoding: ").append(firstTransferEncodingValue).append("\r\n");
        request.append("Transfer-Encoding: ").append(secondTransferEncodingValue).append("\r\n");
        request.append("\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel(std::string_view(request.data(), 1));
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            size_t index = 0;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[++index], 1);
                parserStatus = parser.parse();
            }

            THEN("parser fails to parse request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }
}


SCENARIO("HttpRequestParser parses requests with chunked bodies")
{
    GIVEN("a single http request with headers and a chunked body")
    {
        const auto httpMethod = GENERATE(AS(std::string_view), "POST", "PUT");
        const auto urlPath = GENERATE(AS(std::string_view), "/", "/an/absolute/path");
        const auto urlQuery = GENERATE(AS(std::string_view), "", "a_query");
        const auto headersBlock = GENERATE(AS(std::vector<std::pair<std::string_view, std::string_view>>),
                                           {{"host", " example.com"}},
                                           {{"name", " value"}, {"host", " example.com"}},
                                           {{"host", " example.com"}, {"name1", " value1"}, {"name2", "  value2 "}, {"name3", " va l \t\t ue\t3"}},
                                           {{"Host", " www.example.com"},
                                            {"Referer", " vulnerable.host.net"},
                                            {"Connection", " keep-alive"},
                                            {"Upgrade-Insecure-Requests", " 1"},
                                            {"User-Agent", " Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/52.0.2743.116 Safari/537.36"},
                                            {"Accept", " text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8"},
                                            {"Accept-Encoding", " gzip, deflate, sdch"},
                                            {"Accept-Language", " en-US,en;q=0.8,ru;q=0.6"},
                                            {"Cookie", " a=sdfasd; sdf=3242u389erfhhs; djcnjhe=sdfsdafsdjfb324te1267dd; sdaf=mo2u8943478t67437461746rfdgfcdc; ityu=9u489573484duifhd; GTYFT=nsdjhcbyq3te76ewgfcZ; uityut=23Y746756247856425784657; GA=URHUFVHHVSDNFDHGYSDGF; a=%45345%dfdfg %4656%4534sdfjhsdb.sdfsg.sdfgsf.; aa=4583478; aaaaa=34435345; rrr=iy7t67t6tsdf; ggg=234i5y24785y78ry534785; sdf=3242u389erfhhs; ityu=9u489573484duifhd; GTYFT=nsdjhcbyq3te76ewgfcZ; uityut=23Y746756247856425784657; GA=URHUFVHHVSDNFDHGYSDGF; a=%45345%dfdfg %4656%4534sdfjhsdb.sdfsg.sdfgsf.; nsdjhfb=4358345y; jkbsdff=aaaa; aa=4583478; ggg=234i5y24785y78ry534785; mmm=23uy47fbhdsfbgh; bsdfhbhfgdqqwew=883476757%345345; jksdfb=2348y; ndfsgsfdg=235trHHVGHFGC; erertrt=3242342343423324234; g=888888888788"}});
        const auto bodies = GENERATE(AS(std::vector<std::pair<std::string_view, std::string_view>>),
                                     {{"0\r\n", ""}},
                                     {{"0 ; name = value;q=0.000\r\n", ""}},
                                     {{"15\r\n", "This is the body data"}, {"0\r\n", ""}},
                                     {{"15;name=\"Some qdtext here\" ; name2 = value2\r\n", "This is the body data"}, {"0\r\n", ""}},
                                     {{"0a\r\n", "First data"}, {"0B\r\n", "Second data"}, {"0A\r\n", "Third data"}, {"0\r\n", ""}});
        const auto trailers = GENERATE(AS(std::vector<std::pair<std::string_view, std::string_view>>),
                                        {},
                                        {{"checksum", " 06432d110e1b28308328da0a93ebafe022ffb95eee963af616eac13e530a66de"}},
                                        {{"name", " value"}, {"md5", " 934246903c3d5be19dd8c3c4769ef5ba"}},
                                        {{"name1", " value1"}, {"sha-1", " a511270c6cb2fc0c49d34554a3ae500f6f42a699"}, {"name2", "  value2 "}, {"name3", " va l \t\t ue\t3"}});
        std::string request;
        request.reserve(1024);
        request.append(httpMethod)
            .append(" ")
            .append(urlPath).append(urlQuery.empty() ? "" : "?").append(urlQuery)
            .append(" HTTP/1.1\r\n");
        for (const auto &field : headersBlock)
            request.append(field.first).append(":").append(field.second).append("\r\n");
        request.append("Transfer-Encoding: chunked\r\n\r\n");
        for (const auto &body : bodies)
            request.append(body.first).append(body.second).append(!body.second.empty() ? "\r\n" : "");
        for (const auto &trailer : trailers)
            request.append(trailer.first).append(":").append(trailer.second).append("\r\n");
        request.append("\r\n");

        WHEN("the request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("the request is successfully parsed")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() > parser.requestSize());

                AND_THEN("the parser extracts the correct metadata from the request data")
                {
                    const auto &httpRequest = parser.request();
                    switch(httpRequest.method())
                    {
                        case HttpRequest::Method::GET:
                            REQUIRE(httpMethod == "GET");
                            break;
                        case HttpRequest::Method::PUT:
                            REQUIRE(httpMethod == "PUT");
                            break;
                        case HttpRequest::Method::POST:
                            REQUIRE(httpMethod == "POST");
                            break;
                        case HttpRequest::Method::PATCH:
                            REQUIRE(httpMethod == "PATCH");
                            break;
                        case HttpRequest::Method::DELETE:
                            REQUIRE(httpMethod == "DELETE");
                            break;
                        case HttpRequest::Method::HEAD:
                            REQUIRE(httpMethod == "HEAD");
                            break;
                        case HttpRequest::Method::OPTIONS:
                            REQUIRE(httpMethod == "OPTIONS");
                            break;
                        default:
                            FAIL("This code is supposed to be unreachable.");
                    }
                    REQUIRE(urlPath == httpRequest.targetPath());
                    REQUIRE(urlQuery == httpRequest.targetQuery());
                    REQUIRE(0 == httpRequest.headerCount(""));
                    REQUIRE(1 == httpRequest.headerCount("Host"));
                    REQUIRE(0 == httpRequest.headerCount("AValidHeaderName"));
                    REQUIRE(0 == httpRequest.headerCount("An!nvalid\tHeaderName"));
                    REQUIRE(httpRequest.headersCount() == (headersBlock.size() + 1));
                    for (const auto &field : headersBlock)
                    {
                        REQUIRE(httpRequest.hasHeader(field.first));
                        REQUIRE(httpRequest.headerCount(field.first) == 1);
                        const auto headerValue = httpRequest.header(field.first);
                        REQUIRE(QByteArray(headerValue.data(), headerValue.size()) == QByteArray(field.second.data(), field.second.size()).trimmed());
                    }
                    REQUIRE(httpRequest.hasHeader("Transfer-Encoding"));
                    REQUIRE(httpRequest.headerCount("Transfer-Encoding") == 1);
                    REQUIRE(httpRequest.header("Transfer-Encoding") == "chunked");
                    REQUIRE(!httpRequest.isComplete())
                    REQUIRE(httpRequest.chunked())
                    REQUIRE(httpRequest.requestBodySize() == 0);
                    REQUIRE(httpRequest.pendingBodySize() == 0);
                    REQUIRE(!httpRequest.hasBody());
                    REQUIRE(httpRequest.bodyType() == HttpRequest::BodyType::Chunked);

                    AND_WHEN("parser parses chunked body")
                    {
                        THEN("parser successfully parses chunked body")
                        {
                            size_t expectedRequestBodySize = 0;
                            for (const auto &body : bodies)
                            {
                                if (!body.second.empty())
                                {
                                    REQUIRE(parser.parse() == HttpRequestParser::ParserStatus::ParsedBody);
                                    expectedRequestBodySize += body.second.size();
                                    REQUIRE(httpRequest.requestBodySize() == expectedRequestBodySize);
                                    REQUIRE(httpRequest.chunked());
                                    REQUIRE(httpRequest.pendingBodySize() == 0);
                                    REQUIRE(httpRequest.hasBody());
                                    REQUIRE(httpRequest.body() == body.second);
                                }
                                else
                                {
                                    REQUIRE(parser.parse() == HttpRequestParser::ParserStatus::ParsedRequest);
                                    REQUIRE(parser.requestSize() == request.size());
                                    REQUIRE(httpRequest.requestBodySize() == expectedRequestBodySize);
                                    REQUIRE(httpRequest.chunked());
                                    REQUIRE(httpRequest.pendingBodySize() == 0);
                                    REQUIRE(!httpRequest.hasBody());
                                    REQUIRE(parser.trailersCount() == trailers.size());
                                    for (const auto &trailer : trailers)
                                    {
                                        REQUIRE(parser.hasTrailer(trailer.first));
                                        REQUIRE(parser.trailerCount(trailer.first) == 1);
                                        const auto trailerValue = parser.trailer(trailer.first);
                                        REQUIRE(QByteArray(trailerValue.data(), trailerValue.size()) == QByteArray(trailer.second.data(), trailer.second.size()).trimmed());
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        WHEN("the request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            size_t index = 0;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[index++], 1);
                parserStatus = parser.parse();
            }

            THEN("request metadata (request line + headers) is successfully parsed")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() > parser.requestSize());

                AND_THEN("parser extracts the correct metadata from request data")
                {
                    const auto &httpRequest = parser.request();
                    switch(httpRequest.method())
                    {
                        case HttpRequest::Method::GET:
                            REQUIRE(httpMethod == "GET");
                            break;
                        case HttpRequest::Method::PUT:
                            REQUIRE(httpMethod == "PUT");
                            break;
                        case HttpRequest::Method::POST:
                            REQUIRE(httpMethod == "POST");
                            break;
                        case HttpRequest::Method::PATCH:
                            REQUIRE(httpMethod == "PATCH");
                            break;
                        case HttpRequest::Method::DELETE:
                            REQUIRE(httpMethod == "DELETE");
                            break;
                        case HttpRequest::Method::HEAD:
                            REQUIRE(httpMethod == "HEAD");
                            break;
                        case HttpRequest::Method::OPTIONS:
                            REQUIRE(httpMethod == "OPTIONS");
                            break;
                        default:
                            FAIL("This code is supposed to be unreachable.");
                    }
                    REQUIRE(urlPath == httpRequest.targetPath());
                    REQUIRE(urlQuery == httpRequest.targetQuery());
                    REQUIRE(0 == httpRequest.headerCount(""));
                    REQUIRE(1 == httpRequest.headerCount("Host"));
                    REQUIRE(0 == httpRequest.headerCount("AValidHeaderName"));
                    REQUIRE(0 == httpRequest.headerCount("An!nvalid\tHeaderName"));
                    REQUIRE(httpRequest.headersCount() == (headersBlock.size() + 1));
                    for (const auto &field : headersBlock)
                    {
                        REQUIRE(httpRequest.hasHeader(field.first));
                        REQUIRE(httpRequest.headerCount(field.first) == 1);
                        const auto headerValue = httpRequest.header(field.first);
                        REQUIRE(QByteArray(headerValue.data(), headerValue.size()) == QByteArray(field.second.data(), field.second.size()).trimmed());
                    }
                    REQUIRE(httpRequest.hasHeader("Transfer-Encoding"));
                    REQUIRE(httpRequest.headerCount("Transfer-Encoding") == 1);
                    REQUIRE(httpRequest.header("Transfer-Encoding") == "chunked");
                    REQUIRE(!httpRequest.isComplete())
                    REQUIRE(httpRequest.chunked())
                    REQUIRE(httpRequest.requestBodySize() == 0);
                    REQUIRE(httpRequest.pendingBodySize() == 0);
                    REQUIRE(!httpRequest.hasBody());
                    REQUIRE(httpRequest.bodyType() == HttpRequest::BodyType::Chunked);

                    AND_WHEN("parser parses chunked body byte by byte")
                    {
                        THEN("parser successfully parses chunked body")
                        {
                            size_t expectedRequestBodySize = 0;
                            std::string parsedBody;
                            parsedBody.reserve(256);
                            for (auto idx = parser.requestSize(); idx < (request.size() - 1); ++idx)
                            {
                                ioChannel.readBuffer().write(&request[index++], 1);
                                switch (parser.parse())
                                {
                                    case Kourier::HttpRequestParser::ParserStatus::ParsedRequest:
                                    case Kourier::HttpRequestParser::ParserStatus::Failed:
                                        FAIL("This code is supposed to be unreachable.");
                                    case Kourier::HttpRequestParser::ParserStatus::ParsedBody:
                                        REQUIRE(httpRequest.chunked());
                                        REQUIRE(httpRequest.hasBody());
                                        REQUIRE(httpRequest.requestBodySize() == ++expectedRequestBodySize);
                                        REQUIRE(httpRequest.body().size() == 1);
                                        parsedBody.push_back(httpRequest.body()[0]);
                                        break;
                                    case Kourier::HttpRequestParser::ParserStatus::NeedsMoreData:
                                        continue;
                                }
                            }
                            ioChannel.readBuffer().write(&request[index++], 1);
                            REQUIRE(parser.parse() == Kourier::HttpRequestParser::ParserStatus::ParsedRequest);
                            REQUIRE(parser.requestSize() == request.size());
                            REQUIRE(httpRequest.requestBodySize() == expectedRequestBodySize);
                            REQUIRE(httpRequest.chunked());
                            REQUIRE(httpRequest.pendingBodySize() == 0);
                            REQUIRE(!httpRequest.hasBody());
                            REQUIRE(parser.trailersCount() == trailers.size());
                            for (const auto &trailer : trailers)
                            {
                                REQUIRE(parser.hasTrailer(trailer.first));
                                REQUIRE(parser.trailerCount(trailer.first) == 1);
                                const auto trailerValue = parser.trailer(trailer.first);
                                REQUIRE(QByteArray(trailerValue.data(), trailerValue.size()) == QByteArray(trailer.second.data(), trailer.second.size()).trimmed());
                            }
                        }
                    }
                }
            }
        }
    }

    GIVEN("multiple http requests with with headers and a chunked body")
    {
        const std::vector<std::string_view> httpMethods({"POST", "PUT"});
        const std::vector<std::string_view> urlPaths({"/", "/an/absolute/path"});
        const std::vector<std::string_view> urlQueries({"", "a_query"});
        const std::vector<std::vector<std::pair<std::string_view, std::string_view>>> headersBlocks({
                                                                                                     {{"host", " example.com"}},
                                                                                                     {{"name", " value"}, {"host", " example.com"}},
                                                                                                     {{"host", " example.com"}, {"name1", " value1"}, {"name2", "  value2 "}, {"name3", " va l \t\t ue\t3"}},
                                                                                                     {{"Host", " www.example.com"},
                                                                                                      {"Referer", " vulnerable.host.net"},
                                                                                                      {"Connection", " keep-alive"},
                                                                                                      {"Upgrade-Insecure-Requests", " 1"},
                                                                                                      {"User-Agent", " Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/52.0.2743.116 Safari/537.36"},
                                                                                                      {"Accept", " text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8"},
                                                                                                      {"Accept-Encoding", " gzip, deflate, sdch"},
                                                                                                      {"Accept-Language", " en-US,en;q=0.8,ru;q=0.6"},
                                                                                                      {"Cookie", " a=sdfasd; sdf=3242u389erfhhs; djcnjhe=sdfsdafsdjfb324te1267dd; sdaf=mo2u8943478t67437461746rfdgfcdc; ityu=9u489573484duifhd; GTYFT=nsdjhcbyq3te76ewgfcZ; uityut=23Y746756247856425784657; GA=URHUFVHHVSDNFDHGYSDGF; a=%45345%dfdfg %4656%4534sdfjhsdb.sdfsg.sdfgsf.; aa=4583478; aaaaa=34435345; rrr=iy7t67t6tsdf; ggg=234i5y24785y78ry534785; sdf=3242u389erfhhs; ityu=9u489573484duifhd; GTYFT=nsdjhcbyq3te76ewgfcZ; uityut=23Y746756247856425784657; GA=URHUFVHHVSDNFDHGYSDGF; a=%45345%dfdfg %4656%4534sdfjhsdb.sdfsg.sdfgsf.; nsdjhfb=4358345y; jkbsdff=aaaa; aa=4583478; ggg=234i5y24785y78ry534785; mmm=23uy47fbhdsfbgh; bsdfhbhfgdqqwew=883476757%345345; jksdfb=2348y; ndfsgsfdg=235trHHVGHFGC; erertrt=3242342343423324234; g=888888888788"}}});
        const std::vector<std::vector<std::pair<std::string_view, std::string_view>>> allBodies({
                                                                                                 {{"0\r\n", ""}},
                                                                                                 {{"0 ; name = value;q=0.000\r\n", ""}},
                                                                                                 {{"15\r\n", "This is the body data"}, {"0\r\n", ""}},
                                                                                                 {{"15;name=\"Some qdtext here\" ; name2 = value2\r\n", "This is the body data"}, {"0\r\n", ""}},
                                                                                                 {{"0a\r\n", "First data"}, {"0B\r\n", "Second data"}, {"0A\r\n", "Third data"}, {"0\r\n", ""}}});
        const std::vector<std::vector<std::pair<std::string_view, std::string_view>>> allTrailers({
                                                                                                   {},
                                                                                                   {{"checksum", " 06432d110e1b28308328da0a93ebafe022ffb95eee963af616eac13e530a66de"}},
                                                                                                   {{"name", " value"}, {"md5", " 934246903c3d5be19dd8c3c4769ef5ba"}},
                                                                                                   {{"name1", " value1"}, {"sha-1", " a511270c6cb2fc0c49d34554a3ae500f6f42a699"}, {"name2", "  value2 "}, {"name3", " va l \t\t ue\t3"}}});
        std::string requests;
        requests.reserve(1024*1024);
        for (const auto &httpMethod : httpMethods)
        {
            for (const auto &urlPath : urlPaths)
            {
                for (const auto &urlQuery : urlQueries)
                {
                    for (const auto &headersBlock : headersBlocks)
                    {
                        for (const auto &bodies : allBodies)
                        {
                            for (const auto &trailers : allTrailers)
                            {
                                requests.append(httpMethod)
                                .append(" ")
                                    .append(urlPath).append(urlQuery.empty() ? "" : "?").append(urlQuery)
                                    .append(" HTTP/1.1\r\n");
                                for (const auto &field : headersBlock)
                                    requests.append(field.first).append(":").append(field.second).append("\r\n");
                                requests.append("Transfer-Encoding: chunked\r\n\r\n");
                                for (const auto &body : bodies)
                                    requests.append(body.first).append(body.second).append(!body.second.empty() ? "\r\n" : "");
                                for (const auto &trailer : trailers)
                                    requests.append(trailer.first).append(":").append(trailer.second).append("\r\n");
                                requests.append("\r\n");
                            }
                        }
                    }
                }
            }
        }

        WHEN("parser processes data from all requests at once")
        {
            IOChannelTest ioChannel(requests);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            std::string currentRequest;
            currentRequest.reserve(1024);

            THEN("all requests are successfully parsed")
            {
                for (const auto &httpMethod : httpMethods)
                {
                    for (const auto &urlPath : urlPaths)
                    {
                        for (const auto &urlQuery : urlQueries)
                        {
                            for (const auto &headersBlock : headersBlocks)
                            {
                                for (const auto &bodies : allBodies)
                                {
                                    for (const auto &trailers : allTrailers)
                                    {
                                        currentRequest.clear();
                                        currentRequest.append(httpMethod)
                                            .append(" ")
                                            .append(urlPath).append(urlQuery.empty() ? "" : "?").append(urlQuery)
                                            .append(" HTTP/1.1\r\n");
                                        for (const auto &field : headersBlock)
                                            currentRequest.append(field.first).append(":").append(field.second).append("\r\n");
                                        currentRequest.append("Transfer-Encoding: chunked\r\n\r\n");
                                        for (const auto &body : bodies)
                                            currentRequest.append(body.first).append(body.second).append(!body.second.empty() ? "\r\n" : "");
                                        for (const auto &trailer : trailers)
                                            currentRequest.append(trailer.first).append(":").append(trailer.second).append("\r\n");
                                        currentRequest.append("\r\n");
                                        const auto parserStatus = parser.parse();
                                        REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                                        REQUIRE(currentRequest.size() > parser.requestSize());
                                        const auto &httpRequest = parser.request();
                                        switch(httpRequest.method())
                                        {
                                            case HttpRequest::Method::GET:
                                                REQUIRE(httpMethod == "GET");
                                                break;
                                            case HttpRequest::Method::PUT:
                                                REQUIRE(httpMethod == "PUT");
                                                break;
                                            case HttpRequest::Method::POST:
                                                REQUIRE(httpMethod == "POST");
                                                break;
                                            case HttpRequest::Method::PATCH:
                                                REQUIRE(httpMethod == "PATCH");
                                                break;
                                            case HttpRequest::Method::DELETE:
                                                REQUIRE(httpMethod == "DELETE");
                                                break;
                                            case HttpRequest::Method::HEAD:
                                                REQUIRE(httpMethod == "HEAD");
                                                break;
                                            case HttpRequest::Method::OPTIONS:
                                                REQUIRE(httpMethod == "OPTIONS");
                                                break;
                                            default:
                                                FAIL("This code is supposed to be unreachable.");
                                        }
                                        REQUIRE(urlPath == httpRequest.targetPath());
                                        REQUIRE(urlQuery == httpRequest.targetQuery());
                                        REQUIRE(0 == httpRequest.headerCount(""));
                                        REQUIRE(1 == httpRequest.headerCount("Host"));
                                        REQUIRE(0 == httpRequest.headerCount("AValidHeaderName"));
                                        REQUIRE(0 == httpRequest.headerCount("An!nvalid\tHeaderName"));
                                        REQUIRE(httpRequest.headersCount() == (headersBlock.size() + 1));
                                        for (const auto &field : headersBlock)
                                        {
                                            REQUIRE(httpRequest.hasHeader(field.first));
                                            REQUIRE(httpRequest.headerCount(field.first) == 1);
                                            const auto headerValue = httpRequest.header(field.first);
                                            REQUIRE(QByteArray(headerValue.data(), headerValue.size()) == QByteArray(field.second.data(), field.second.size()).trimmed());
                                        }
                                        REQUIRE(httpRequest.hasHeader("Transfer-Encoding"));
                                        REQUIRE(httpRequest.headerCount("Transfer-Encoding") == 1);
                                        REQUIRE(httpRequest.header("Transfer-Encoding") == "chunked");
                                        REQUIRE(!httpRequest.isComplete());
                                        REQUIRE(httpRequest.chunked());
                                        REQUIRE(httpRequest.requestBodySize() == 0);
                                        REQUIRE(httpRequest.pendingBodySize() == 0);
                                        REQUIRE(!httpRequest.hasBody());
                                        REQUIRE(httpRequest.bodyType() == HttpRequest::BodyType::Chunked);
                                        size_t expectedRequestBodySize = 0;
                                        for (const auto &body : bodies)
                                        {
                                            if (!body.second.empty())
                                            {
                                                REQUIRE(parser.parse() == HttpRequestParser::ParserStatus::ParsedBody);
                                                expectedRequestBodySize += body.second.size();
                                                REQUIRE(httpRequest.requestBodySize() == expectedRequestBodySize);
                                                REQUIRE(httpRequest.chunked());
                                                REQUIRE(httpRequest.pendingBodySize() == 0);
                                                REQUIRE(httpRequest.hasBody());
                                                REQUIRE(httpRequest.body() == body.second);
                                            }
                                            else
                                            {
                                                REQUIRE(parser.parse() == HttpRequestParser::ParserStatus::ParsedRequest);
                                                REQUIRE(parser.requestSize() == currentRequest.size());
                                                REQUIRE(httpRequest.requestBodySize() == expectedRequestBodySize);
                                                REQUIRE(httpRequest.chunked());
                                                REQUIRE(httpRequest.pendingBodySize() == 0);
                                                REQUIRE(!httpRequest.hasBody());
                                                REQUIRE(parser.trailersCount() == trailers.size());
                                                for (const auto &trailer : trailers)
                                                {
                                                    REQUIRE(parser.hasTrailer(trailer.first));
                                                    REQUIRE(parser.trailerCount(trailer.first) == 1);
                                                    const auto trailerValue = parser.trailer(trailer.first);
                                                    REQUIRE(QByteArray(trailerValue.data(), trailerValue.size()) == QByteArray(trailer.second.data(), trailer.second.size()).trimmed());
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                const auto parserStatus = parser.parse();
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData);
            }
        }

        WHEN("parser processes data from all requests byte by byte")
        {
            size_t index = 0;
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            std::string currentRequest;
            currentRequest.reserve(1024);

            THEN("all requests are successfully parsed")
            {
                for (const auto &httpMethod : httpMethods)
                {
                    for (const auto &urlPath : urlPaths)
                    {
                        for (const auto &urlQuery : urlQueries)
                        {
                            for (const auto &headersBlock : headersBlocks)
                            {
                                for (const auto &bodies : allBodies)
                                {
                                    for (const auto &trailers : allTrailers)
                                    {
                                        currentRequest.clear();
                                        currentRequest.append(httpMethod)
                                            .append(" ")
                                            .append(urlPath).append(urlQuery.empty() ? "" : "?").append(urlQuery)
                                            .append(" HTTP/1.1\r\n");
                                        for (const auto &field : headersBlock)
                                            currentRequest.append(field.first).append(":").append(field.second).append("\r\n");
                                        currentRequest.append("Transfer-Encoding: chunked\r\n\r\n");
                                        for (const auto &body : bodies)
                                            currentRequest.append(body.first).append(body.second).append(!body.second.empty() ? "\r\n" : "");
                                        for (const auto &trailer : trailers)
                                            currentRequest.append(trailer.first).append(":").append(trailer.second).append("\r\n");
                                        currentRequest.append("\r\n");
                                        auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
                                        while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
                                        {
                                            ioChannel.readBuffer().write(&requests[index++], 1);
                                            parserStatus = parser.parse();
                                        }
                                        REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                                        REQUIRE(currentRequest.size() > parser.requestSize());
                                        const auto &httpRequest = parser.request();
                                        switch(httpRequest.method())
                                        {
                                            case HttpRequest::Method::GET:
                                                REQUIRE(httpMethod == "GET");
                                                break;
                                            case HttpRequest::Method::PUT:
                                                REQUIRE(httpMethod == "PUT");
                                                break;
                                            case HttpRequest::Method::POST:
                                                REQUIRE(httpMethod == "POST");
                                                break;
                                            case HttpRequest::Method::PATCH:
                                                REQUIRE(httpMethod == "PATCH");
                                                break;
                                            case HttpRequest::Method::DELETE:
                                                REQUIRE(httpMethod == "DELETE");
                                                break;
                                            case HttpRequest::Method::HEAD:
                                                REQUIRE(httpMethod == "HEAD");
                                                break;
                                            case HttpRequest::Method::OPTIONS:
                                                REQUIRE(httpMethod == "OPTIONS");
                                                break;
                                            default:
                                                FAIL("This code is supposed to be unreachable.");
                                        }
                                        REQUIRE(urlPath == httpRequest.targetPath());
                                        REQUIRE(urlQuery == httpRequest.targetQuery());
                                        REQUIRE(0 == httpRequest.headerCount(""));
                                        REQUIRE(1 == httpRequest.headerCount("Host"));
                                        REQUIRE(0 == httpRequest.headerCount("AValidHeaderName"));
                                        REQUIRE(0 == httpRequest.headerCount("An!nvalid\tHeaderName"));
                                        REQUIRE(httpRequest.headersCount() == (headersBlock.size() + 1));
                                        for (const auto &field : headersBlock)
                                        {
                                            REQUIRE(httpRequest.hasHeader(field.first));
                                            REQUIRE(httpRequest.headerCount(field.first) == 1);
                                            const auto headerValue = httpRequest.header(field.first);
                                            REQUIRE(QByteArray(headerValue.data(), headerValue.size()) == QByteArray(field.second.data(), field.second.size()).trimmed());
                                        }
                                        REQUIRE(httpRequest.hasHeader("Transfer-Encoding"));
                                        REQUIRE(httpRequest.headerCount("Transfer-Encoding") == 1);
                                        REQUIRE(httpRequest.header("Transfer-Encoding") == "chunked");
                                        REQUIRE(!httpRequest.isComplete());
                                        REQUIRE(httpRequest.chunked());
                                        REQUIRE(httpRequest.requestBodySize() == 0);
                                        REQUIRE(httpRequest.pendingBodySize() == 0);
                                        REQUIRE(!httpRequest.hasBody());
                                        REQUIRE(httpRequest.bodyType() == HttpRequest::BodyType::Chunked);
                                        size_t expectedRequestBodySize = 0;
                                        std::string parsedBody;
                                        parsedBody.reserve(256);
                                        for (auto idx = parser.requestSize(); idx < (currentRequest.size() - 1); ++idx)
                                        {
                                            ioChannel.readBuffer().write(&requests[index++], 1);
                                            switch (parser.parse())
                                            {
                                                case Kourier::HttpRequestParser::ParserStatus::ParsedRequest:
                                                case Kourier::HttpRequestParser::ParserStatus::Failed:
                                                    FAIL("This code is supposed to be unreachable.");
                                                case Kourier::HttpRequestParser::ParserStatus::ParsedBody:
                                                    REQUIRE(httpRequest.chunked());
                                                    REQUIRE(httpRequest.hasBody());
                                                    REQUIRE(httpRequest.requestBodySize() == ++expectedRequestBodySize);
                                                    REQUIRE(httpRequest.body().size() == 1);
                                                    parsedBody.push_back(httpRequest.body()[0]);
                                                    break;
                                                case Kourier::HttpRequestParser::ParserStatus::NeedsMoreData:
                                                    continue;
                                            }
                                        }
                                        ioChannel.readBuffer().write(&requests[index++], 1);
                                        REQUIRE(parser.parse() == Kourier::HttpRequestParser::ParserStatus::ParsedRequest);
                                        REQUIRE(parser.requestSize() == currentRequest.size());
                                        REQUIRE(httpRequest.requestBodySize() == expectedRequestBodySize);
                                        REQUIRE(httpRequest.chunked());
                                        REQUIRE(httpRequest.pendingBodySize() == 0);
                                        REQUIRE(!httpRequest.hasBody());
                                        REQUIRE(parser.trailersCount() == trailers.size());
                                        for (const auto &trailer : trailers)
                                        {
                                            REQUIRE(parser.hasTrailer(trailer.first));
                                            REQUIRE(parser.trailerCount(trailer.first) == 1);
                                            const auto trailerValue = parser.trailer(trailer.first);
                                            REQUIRE(QByteArray(trailerValue.data(), trailerValue.size()) == QByteArray(trailer.second.data(), trailer.second.size()).trimmed());
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                const auto parserStatus = parser.parse();
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData);
            }
        }
    }
}


SCENARIO("HttpServer responds with 100-Continue status code when client sends expect header with 100-continue value")
{
    GIVEN("a request containing an Expect: 100-continue header")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "POST / HTTP/1.1\r\nHost: example.com\r\nContent-Length: 10\r\nExpect: 100-Continue\r\n\r\n",
                                      "GET / HTTP/1.1\r\nHost: example.com\r\nContent-Length: 0\r\nExpect: 100-Continue\r\n\r\n",
                                      "PUT / HTTP/1.1\r\nHost: example.com\r\nTransfer-Encoding: chunked\r\nExpect: 100-Continue\r\n\r\n",
                                      "GET / HTTP/1.1\r\nHost: example.com\r\nExpect: 100-Continue\r\n\r\n");

        WHEN("request metadata (request line + headers) is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            REQUIRE(ioChannel.writeBuffer().isEmpty());
            const auto parserStatus = parser.parse();

            THEN("parser parses request metadata and writes 'HTTP/1.1 100 Continue\\r\\n\\r\\n' to io channel")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                const std::string_view expectedResponse("HTTP/1.1 100 Continue\r\n\r\n");
                REQUIRE(!ioChannel.writeBuffer().isEmpty());
                const auto writtenData = ioChannel.writeBuffer().peekAll();
                REQUIRE(writtenData == expectedResponse);
            }
        }

        WHEN("request metadata (request line + headers) is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            REQUIRE(ioChannel.writeBuffer().isEmpty());
            for (auto i = 0; i < (request.size() - 1); ++i)
            {
                ioChannel.readBuffer().write(&request[i], 1);
                REQUIRE(parser.parse() == HttpRequestParser::ParserStatus::NeedsMoreData);
            }
            ioChannel.readBuffer().write(&request[request.size() - 1], 1);
            const auto parserStatus = parser.parse();

            THEN("parser parses request metadata and writes 'HTTP/1.1 100 Continue\\r\\n\\r\\n' to io channel")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                const std::string_view expectedResponse("HTTP/1.1 100 Continue\r\n\r\n");
                REQUIRE(!ioChannel.writeBuffer().isEmpty());
                const auto writtenData = ioChannel.writeBuffer().peekAll();
                REQUIRE(writtenData == expectedResponse);
            }
        }
    }

    GIVEN("a request containing multiple Expect: 100-continue field lines in header")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "POST / HTTP/1.1\r\nExpect: 100-Continue\r\nHost: example.com\r\nContent-Length: 10\r\nExpect: 100-Continue\r\n\r\n",
                                      "GET / HTTP/1.1\r\nHost: example.com\r\nExpect: 100-Continue\r\nContent-Length: 0\r\nExpect: 100-Continue\r\n\r\n",
                                      "PUT / HTTP/1.1\r\nExpect: 100-Continue\r\nHost: example.com\r\nExpect: 100-Continue\r\nTransfer-Encoding: chunked\r\nExpect: 100-Continue\r\n\r\n",
                                      "GET / HTTP/1.1\r\nHost: example.com\r\nExpect: 100-Continue\r\nExpect: 100-Continue\r\nExpect: 100-Continue\r\n\r\n");

        WHEN("request metadata (request line + headers) is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            REQUIRE(ioChannel.writeBuffer().isEmpty());
            const auto parserStatus = parser.parse();

            THEN("parser parses request metadata and writes 'HTTP/1.1 100 Continue\\r\\n\\r\\n' to io channel")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                const std::string_view expectedResponse("HTTP/1.1 100 Continue\r\n\r\n");
                REQUIRE(!ioChannel.writeBuffer().isEmpty());
                const auto writtenData = ioChannel.writeBuffer().peekAll();
                REQUIRE(writtenData == expectedResponse);
            }
        }

        WHEN("request metadata (request line + headers) is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            REQUIRE(ioChannel.writeBuffer().isEmpty());
            for (auto i = 0; i < (request.size() - 1); ++i)
            {
                ioChannel.readBuffer().write(&request[i], 1);
                REQUIRE(parser.parse() == HttpRequestParser::ParserStatus::NeedsMoreData);
            }
            ioChannel.readBuffer().write(&request[request.size() - 1], 1);
            const auto parserStatus = parser.parse();

            THEN("parser parses request metadata and writes 'HTTP/1.1 100 Continue\\r\\n\\r\\n' to io channel")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                const std::string_view expectedResponse("HTTP/1.1 100 Continue\r\n\r\n");
                REQUIRE(!ioChannel.writeBuffer().isEmpty());
                const auto writtenData = ioChannel.writeBuffer().peekAll();
                REQUIRE(writtenData == expectedResponse);
            }
        }
    }
}


SCENARIO("HttpRequestParser allows server-wide options with * instead of absolute path for OPTIONS request")
{
    GIVEN("a server-wide OPTIONS request")
    {
        std::string_view request("OPTIONS * HTTP/1.1\r\nHost: host.com\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("request is successfully parsed")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());

                AND_THEN("parser extracts the correct information from server-wide OPTIONS request")
                {
                    const auto &request = parser.request();
                    REQUIRE(request.method() == HttpRequest::Method::OPTIONS);
                    REQUIRE(1 == request.headersCount());
                    REQUIRE(0 == request.headerCount("Content-Length"));
                    REQUIRE(1 == request.headerCount("Host"));
                    REQUIRE(0 == request.headerCount("Date"));
                    REQUIRE(0 == request.headerCount("Transfer-Encoding"));
                    REQUIRE(0 == request.headerCount("AValidHeaderName"));
                    REQUIRE(0 == request.headerCount("An!nvalid\tHeaderName"));
                    REQUIRE(request.targetPath() == "*");
                    REQUIRE(request.targetQuery().empty());
                }
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            size_t index = 0;
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[index++], 1);
                parserStatus = parser.parse();
            }
            REQUIRE(index == request.size());

            THEN("the request is successfully parsed")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);
                REQUIRE(request.size() == parser.requestSize());

                AND_THEN("the parser extracts the correct information from the request data")
                {
                    const auto &request = parser.request();
                    REQUIRE(request.method() == HttpRequest::Method::OPTIONS);
                    REQUIRE(1 == request.headersCount());
                    REQUIRE(0 == request.headerCount("Content-Length"));
                    REQUIRE(1 == request.headerCount("Host"));
                    REQUIRE(0 == request.headerCount("Date"));
                    REQUIRE(0 == request.headerCount("Transfer-Encoding"));
                    REQUIRE(0 == request.headerCount("AValidHeaderName"));
                    REQUIRE(0 == request.headerCount("An!nvalid\tHeaderName"));
                    REQUIRE(request.targetPath() == "*");
                    REQUIRE(request.targetQuery().empty());
                }
            }
        }
    }

    GIVEN("a non-OPTIONS request targeting *")
    {
        const auto httpMethod = GENERATE(AS(std::string_view), "GET", "PUT", "PATCH", "POST", "DELETE", "HEAD");
        const std::string_view urlPath("*");
        const auto urlQuery = GENERATE(AS(std::string_view),
                                       "",
                                       "a_query",
                                       "key=val",
                                       "date=2015-05-31&locations=Los%20Angeles%7CNew%20York&attendees=10%7C5&services=Housekeeping,Catering%7CHousekeeping&duration=60",
                                       "aid=304142&label=gen173nr-342396dbc1b331fab24&tmpl=searchresults&ac_click_type=b&ac_position=0&checkin_month=3&checkin_monthday=7&checkin_year=2019&checkout_month=3&checkout_monthday=10&checkout_year=2019&class_interval=1&dest_id=20015107&dest_type=city&dtdisc=0&from_sf=1&group_adults=1&group_children=0&inac=0&index_postcard=0&label_click=undef&no_rooms=1&postcard=0&raw_dest_type=city&room1=A&sb_price_type=total&sb_travel_purpose=business&search_selected=1&shw_aparth=1&slp_r_match=0&src=index&srpvid=e0267a2be8ef0020&ss=Pasadena%2C%20California%2C%20USA&ss_all=0&ss_raw=pasadena&ssb=empty&sshis=0&nflt=hotelfacility%3D107%3Bmealplan%3D1%3Bpri%3D4%3Bpri%3D3%3Bclass%3D4%3Bclass%3D5%3Bpopular_activities%3D55%3Bhr_24%3D8%3Btdb%3D3%3Breview_score%3D70%3Broomfacility%3D75%3B&rsf=blah");

        std::string request;
        request.reserve(256);
        request.append(httpMethod)
            .append(" ")
            .append(urlPath).append(urlQuery.empty() ? "" : "?").append(urlQuery)
            .append(" ")
            .append("HTTP/1.1\r\n")
            .append("Host: host.com\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            const auto parserStatus = parser.parse();

            THEN("parser fails to parse the malformed request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
            size_t index = 0;
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[index++], 1);
                parserStatus = parser.parse();
            }

            THEN("parser fails to parse the malformed request")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::MalformedRequest);
            }
        }
    }
}


SCENARIO("HttpRequestParser enforces limit on request size")
{
    constexpr static size_t requestSizeLimit = 64;
    std::shared_ptr<HttpRequestLimits> pHttpRequestLimits(new HttpRequestLimits);
    pHttpRequestLimits->maxRequestSize = requestSizeLimit;

    GIVEN("a request whose request size limit is exceeded while parsing absolute path")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "GET /aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                      "GET /aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa HTTP/1.1\r\nHost: example.com\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            const auto parserStatus = parser.parse();

            THEN("parser fails and reports too big message error")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            REQUIRE(request.size() > requestSizeLimit);
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            size_t processedSize = 0;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[processedSize++], 1);
                parserStatus = parser.parse();
            }

            THEN("parser fails and reports too big message error")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
            }
        }
    }

    GIVEN("a request whose request size limit is exceeded while parsing query")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "GET /aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa?bbbbbbbbbb",
                                      "GET /aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa?bbbbbbbbbb HTTP/1.1\r\nHost: example.com\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            const auto parserStatus = parser.parse();

            THEN("parser fails and reports too big message error")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            REQUIRE(request.size() > requestSizeLimit);
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            size_t processedSize = 0;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[processedSize++], 1);
                parserStatus = parser.parse();
            }

            THEN("parser fails and reports too big message error")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
            }
        }
    }

    GIVEN("a request whose request size limit is exceeded while parsing http version")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "GET /aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa HTTP/1.1",
                                      "GET /aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa HTTP/1.1\r\nHost: example.com\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            const auto parserStatus = parser.parse();

            THEN("parser fails and reports too big message error")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            REQUIRE(request.size() > requestSizeLimit);
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            size_t processedSize = 0;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[processedSize++], 1);
                parserStatus = parser.parse();
            }

            THEN("parser fails and reports too big message error before version gets parsed")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
            }
        }
    }

    GIVEN("a request whose request size limit is exceeded while parsing header name")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "GET /aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa HTTP/1.1\r\nHost",
                                      "GET /aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa HTTP/1.1\r\nHost: example.com\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            const auto parserStatus = parser.parse();

            THEN("parser fails and reports too big message error")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            REQUIRE(request.size() > requestSizeLimit);
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            size_t processedSize = 0;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[processedSize++], 1);
                parserStatus = parser.parse();
            }

            THEN("parser fails and reports too big message error before version gets parsed")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
            }
        }
    }

    GIVEN("a request whose request size limit is exceeded while parsing header value")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "GET /aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa HTTP/1.1\r\nHost: example.com",
                                      "GET /aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa HTTP/1.1\r\nHost: example.com\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            const auto parserStatus = parser.parse();

            THEN("parser fails and reports too big message error")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            REQUIRE(request.size() > requestSizeLimit);
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            size_t processedSize = 0;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[processedSize++], 1);
                parserStatus = parser.parse();
            }

            THEN("parser fails and reports too big message error before version gets parsed")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
            }
        }
    }

    GIVEN("a request whose request size limit is exceeded after parsing the headers and counting request body")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "POST / HTTP/1.1\r\nHost: example.com\r\nContent-Length: 21\r\n\r\n",
                                      "POST / HTTP/1.1\r\nHost: example.com\r\nContent-Length: 21\r\n\r\nThis is the body data");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            const auto parserStatus = parser.parse();

            THEN("parser fails and reports too big message error")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            size_t processedSize = 0;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[processedSize++], 1);
                parserStatus = parser.parse();
            }

            THEN("parser fails and reports too big message error before version gets parsed")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
            }
        }
    }

    GIVEN("a chunked request whose request size limit is exceeded when parsing the chunk size")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "POST / HTTP/1.1\r\nHost: host\r\nTransfer-Encoding: chunked\r\n\r\nFFFFFF",
                                      "POST / HTTP/1.1\r\nHost: host\r\nTransfer-Encoding: chunked\r\n\r\nFFFFFF\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            auto parserStatus = parser.parse();

            THEN("parser parses request metadata successfully")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);

                AND_WHEN("chunk metadata is parsed")
                {
                    parserStatus = parser.parse();

                    THEN("parser fails and reports too big message error")
                    {
                        REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                        REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
                    }
                }
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            size_t processedSize = 0;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[processedSize++], 1);
                parserStatus = parser.parse();
            }

            THEN("parser parses request metadata successfully")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);

                AND_WHEN("chunk metadata is parsed")
                {
                    parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
                    while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
                    {
                        ioChannel.readBuffer().write(&request[processedSize++], 1);
                        parserStatus = parser.parse();
                    }

                    THEN("parser fails and reports too big message error before version gets parsed")
                    {
                        REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                        REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
                    }
                }
            }
        }
    }

    GIVEN("a chunked request whose request size limit is exceeded when parsing the chunk extension")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\nF ; name = value",
                                      "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\nF ; name = value\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            auto parserStatus = parser.parse();

            THEN("parser parses request metadata successfully")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);

                AND_WHEN("chunk metadata is parsed")
                {
                    parserStatus = parser.parse();

                    THEN("parser fails and reports too big message error")
                    {
                        REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                        REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
                    }
                }
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            size_t processedSize = 0;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[processedSize++], 1);
                parserStatus = parser.parse();
            }

            THEN("parser parses request metadata successfully")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);

                AND_WHEN("chunk metadata is parsed")
                {
                    parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
                    while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
                    {
                        ioChannel.readBuffer().write(&request[processedSize++], 1);
                        parserStatus = parser.parse();
                    }

                    THEN("parser fails and reports too big message error before version gets parsed")
                    {
                        REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                        REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
                    }
                }
            }
        }
    }

    GIVEN("a chunked request whose request size limit is exceeded when adding the first chunk size to request size")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "POST / HTTP/1.1\r\nHost: h\r\nTransfer-Encoding: chunked\r\n\r\nFF\r\n",
                                      "POST / HTTP/1.1\r\nHost: h\r\nTransfer-Encoding: chunked\r\n\r\nFF\r\nThis is the data dude");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            auto parserStatus = parser.parse();

            THEN("parser parses request metadata successfully")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);

                AND_WHEN("chunk metadata is parsed")
                {
                    parserStatus = parser.parse();

                    THEN("parser fails and reports too big message error")
                    {
                        REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                        REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
                    }
                }
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            size_t processedSize = 0;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[processedSize++], 1);
                parserStatus = parser.parse();
            }

            THEN("parser parses request metadata successfully")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);

                AND_WHEN("chunk metadata is parsed")
                {
                    parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
                    while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
                    {
                        ioChannel.readBuffer().write(&request[processedSize++], 1);
                        parserStatus = parser.parse();
                    }

                    THEN("parser fails and reports too big message error before version gets parsed")
                    {
                        REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                        REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
                    }
                }
            }
        }
    }

    GIVEN("a chunked request whose request size limit is exceeded when adding the second chunk size to request size")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "POST / HTTP/1.1\r\nHost:h\r\nTransfer-Encoding:chunked\r\n\r\n1\r\nz\r\n2\r\n",
                                      "POST / HTTP/1.1\r\nHost:h\r\nTransfer-Encoding:chunked\r\n\r\n1\r\nz\r\nF\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            auto parserStatus = parser.parse();

            THEN("parser parses request metadata successfully")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);

                AND_WHEN("first chunk is parsed")
                {
                    parserStatus = parser.parse();

                    THEN("parser parses chunk")
                    {
                        REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedBody);
                        REQUIRE(parser.request().body() == "z");

                        AND_WHEN("second chunk metadata is parsed")
                        {
                            parserStatus = parser.parse();

                            THEN("parser fails and reports too big message error")
                            {
                                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                                REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
                            }
                        }
                    }
                }
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            size_t processedSize = 0;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[processedSize++], 1);
                parserStatus = parser.parse();
            }

            THEN("parser parses request metadata successfully")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);

                AND_WHEN("first chunk is parsed")
                {
                    parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
                    while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
                    {
                        ioChannel.readBuffer().write(&request[processedSize++], 1);
                        parserStatus = parser.parse();
                    }

                    THEN("parser parses chunk")
                    {
                        REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedBody);
                        REQUIRE(parser.request().body() == "z");

                        AND_WHEN("second chunk metadata is parsed")
                        {
                            parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
                            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
                            {
                                ioChannel.readBuffer().write(&request[processedSize++], 1);
                                parserStatus = parser.parse();
                            }

                            THEN("parser fails and reports too big message error")
                            {
                                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                                REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
                            }
                        }
                    }
                }
            }
        }
    }

    GIVEN("a request whose request size limit is exceeded while parsing trailer name")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "POST / HTTP/1.1\r\nHost:h\r\nTransfer-Encoding:chunked\r\n\r\n0\r\ntrailer-name",
                                      "POST / HTTP/1.1\r\nHost:h\r\nTransfer-Encoding:chunked\r\n\r\n0\r\ntrailer-name:value\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            auto parserStatus = parser.parse();

            THEN("parser parses request metadata successfully")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);

                AND_WHEN("rest of chunked message is parsed")
                {
                    parserStatus = parser.parse();

                    THEN("parser fails and reports too big message error")
                    {
                        REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                        REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
                    }
                }
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            size_t processedSize = 0;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[processedSize++], 1);
                parserStatus = parser.parse();
            }

            THEN("parser parses request metadata successfully")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);

                AND_WHEN("rest of chunked message is parsed")
                {
                    parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
                    while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
                    {
                        ioChannel.readBuffer().write(&request[processedSize++], 1);
                        parserStatus = parser.parse();
                    }

                    THEN("parser fails and reports too big message error")
                    {
                        REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                        REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
                    }
                }
            }
        }
    }

    GIVEN("a request whose request size limit is exceeded while parsing trailer value")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "POST / HTTP/1.1\r\nHost:h\r\nTransfer-Encoding:chunked\r\n\r\n0\r\nname: trailer-value",
                                      "POST / HTTP/1.1\r\nHost:h\r\nTransfer-Encoding:chunked\r\n\r\n0\r\nname: trailer-value\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            auto parserStatus = parser.parse();

            THEN("parser parses request metadata successfully")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);

                AND_WHEN("rest of chunked message is parsed")
                {
                    parserStatus = parser.parse();

                    THEN("parser fails and reports too big message error")
                    {
                        REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                        REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
                    }
                }
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            size_t processedSize = 0;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[processedSize++], 1);
                parserStatus = parser.parse();
            }

            THEN("parser parses request metadata successfully")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);

                AND_WHEN("rest of chunked message is parsed")
                {
                    parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
                    while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
                    {
                        ioChannel.readBuffer().write(&request[processedSize++], 1);
                        parserStatus = parser.parse();
                    }

                    THEN("parser fails and reports too big message error")
                    {
                        REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                        REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
                    }
                }
            }
        }
    }
}


SCENARIO("HttpRequestParser enforces limit on request body")
{
    constexpr static size_t bodySizeLimit = 5;
    std::shared_ptr<HttpRequestLimits> pHttpRequestLimits(new HttpRequestLimits);
    pHttpRequestLimits->maxBodySize = bodySizeLimit;

    GIVEN("request with body larger than parser is allowed to parse")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "POST / HTTP/1.1\r\nHost: example.com\r\nContent-Length: 12\r\n\r\n",
                                      "POST / HTTP/1.1\r\nHost: example.com\r\nContent-Length: 12\r\n\r\nHello World!");

        WHEN("request metadata is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            const auto parserStatus = parser.parse();

            THEN("parser fails and reports too big message error")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
            }
        }

        WHEN("request metadata is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            size_t processedSize = 0;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[processedSize++], 1);
                parserStatus = parser.parse();
            }

            THEN("parser fails and reports too big message error")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
            }
        }
    }

    GIVEN("chunked request with body larger than parser is allowed to parse")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "POST / HTTP/1.1\r\nHost: example.com\r\nTransfer-Encoding: chunked\r\n\r\nC\r\n",
                                      "POST / HTTP/1.1\r\nHost: example.com\r\nTransfer-Encoding: chunked\r\n\r\nC\r\nHello World!\r\n");

        WHEN("request metadata is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            const auto parserStatus = parser.parse();

            THEN("parser parses request metadata successfully")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);

                AND_WHEN("chunk metadata is parsed")
                {
                    const auto parserStatus = parser.parse();

                    THEN("parser fails and reports too big message error")
                    {
                        REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                        REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
                    }
                }
            }
        }

        WHEN("request metadata is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            size_t processedSize = 0;
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[processedSize++], 1);
                parserStatus = parser.parse();
            }

            THEN("parser parses request metadata successfully")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);

                AND_WHEN("chunk metadata is parsed")
                {
                    auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
                    while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
                    {
                        ioChannel.readBuffer().write(&request[processedSize++], 1);
                        parserStatus = parser.parse();
                    }

                    THEN("parser fails and reports too big message error")
                    {
                        REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                        REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
                    }
                }
            }
        }
    }

    GIVEN("chunked request with bodies whose sizes summed are larger than parser is allowed to parse")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "POST / HTTP/1.1\r\nHost: example.com\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n7\r\n World!\r\n");

        WHEN("request metadata is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            const auto parserStatus = parser.parse();

            THEN("parser parses request metadata successfully")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);

                AND_WHEN("chunk is parsed")
                {
                    const auto parserStatus = parser.parse();

                    THEN("parser parses first chunk data")
                    {
                        REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedBody);
                        REQUIRE(parser.request().body() == "Hello");

                        AND_WHEN("next chunk is parsed")
                        {
                            const auto parserStatus = parser.parse();

                            THEN("parser fails and reports too big message error")
                            {
                                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                                REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
                            }
                        }
                    }
                }
            }
        }

        WHEN("request metadata is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            size_t processedSize = 0;
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[processedSize++], 1);
                parserStatus = parser.parse();
            }

            THEN("parser parses request metadata successfully")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);

                AND_WHEN("chunk is parsed")
                {
                    auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
                    while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
                    {
                        ioChannel.readBuffer().write(&request[processedSize++], 1);
                        parserStatus = parser.parse();
                    }

                    THEN("parser parses first chunk data successfully")
                    {
                        std::string chunkData;
                        auto parserStatus = HttpRequestParser::ParserStatus::ParsedBody;
                        while (parserStatus == HttpRequestParser::ParserStatus::ParsedBody)
                        {
                            chunkData.append(parser.request().body());
                            ioChannel.readBuffer().write(&request[processedSize++], 1);
                            parserStatus = parser.parse();
                        }
                        REQUIRE(chunkData == "Hello");

                        AND_WHEN("next chunk is parsed")
                        {
                            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
                            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
                            {
                                ioChannel.readBuffer().write(&request[processedSize++], 1);
                                parserStatus = parser.parse();
                            }

                            THEN("parser fails and reports too big message error")
                            {
                                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                                REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
                            }
                        }
                    }
                }
            }
        }
    }
}


SCENARIO("HttpRequestParser enforces limit on target uri")
{
    constexpr static size_t targetUriLimit = 4;
    std::shared_ptr<HttpRequestLimits> pHttpRequestLimits(new HttpRequestLimits);
    pHttpRequestLimits->maxUrlSize = targetUriLimit;

    GIVEN("request with larger target uri is parsed")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "GET /aaaa",
                                      "GET /aaaa HTTP/1.1\r\nHost: example.com\r\n\r\n",
                                      "GET /a?aa",
                                      "GET /a?aa HTTP/1.1\r\nHost: example.com\r\n\r\n",
                                      "GET /?aaa",
                                      "GET /?aaa HTTP/1.1\r\nHost: example.com\r\n\r\n",
                                      "GET /aaa?",
                                      "GET /aaa? HTTP/1.1\r\nHost: example.com\r\n\r\n",
                                      "GET /%AF?",
                                      "GET /%AF? HTTP/1.1\r\nHost: example.com\r\n\r\n",
                                      "GET /?%AF",
                                      "GET /?%AF HTTP/1.1\r\nHost: example.com\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            const auto parserStatus = parser.parse();

            THEN("parser fails and reports too big message error")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
            }
        }

        WHEN("request metadata is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            size_t processedSize = 0;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[processedSize++], 1);
                parserStatus = parser.parse();
            }

            THEN("parser fails and reports too big message error")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
            }
        }
    }
}


SCENARIO("HttpRequestParser enforces limit on headers")
{
    constexpr static size_t maxHeaderNameSize = 5;
    constexpr static size_t maxHeaderValueSize = 5;
    constexpr static size_t maxHeaderLineCount = 3;
    std::shared_ptr<HttpRequestLimits> pHttpRequestLimits(new HttpRequestLimits);
    pHttpRequestLimits->maxHeaderNameSize = maxHeaderNameSize;
    pHttpRequestLimits->maxHeaderValueSize = maxHeaderValueSize;
    pHttpRequestLimits->maxHeaderLineCount = maxHeaderLineCount;

    GIVEN("request with header name/value/line count larger than parser is allowed to accept is parsed")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "GET / HTTP/1.1\r\nHost: a\r\ntoo-large-name: value\r\n\r\n",
                                      "GET / HTTP/1.1\r\nHost: a\r\nname: too-large-value\r\n\r\n",
                                      "GET / HTTP/1.1\r\nHost: a\r\ntoo:\r\nmany:\r\nlines:\r\n\r\n");

        WHEN("request is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            const auto parserStatus = parser.parse();

            THEN("parser fails and reports too big message error")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            size_t processedSize = 0;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[processedSize++], 1);
                parserStatus = parser.parse();
            }

            THEN("parser fails and reports too big message error")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
            }
        }
    }
}


SCENARIO("HttpRequestParser enforces limit on trailers")
{
    constexpr static size_t maxTrailerNameSize = 5;
    constexpr static size_t maxTrailerValueSize = 5;
    constexpr static size_t maxTrailerLineCount = 3;
    std::shared_ptr<HttpRequestLimits> pHttpRequestLimits(new HttpRequestLimits);
    pHttpRequestLimits->maxTrailerNameSize = maxTrailerNameSize;
    pHttpRequestLimits->maxTrailerValueSize = maxTrailerValueSize;
    pHttpRequestLimits->maxTrailerLineCount = maxTrailerLineCount;

    GIVEN("request with trailer name/value/line count larger than parser is allowed to accept is parsed")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "GET / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n0\r\ntoo-large-name: value\r\n\r\n",
                                      "GET / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n0\r\nname: too-large-value\r\n\r\n",
                                      "GET / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n0\r\ntoo:\r\nmany:\r\nlines:\r\nhere:\r\n\r\n");

        WHEN("request metadata is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            const auto parserStatus = parser.parse();

            THEN("parser successfully parses request metadata")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);

                AND_WHEN("last chunk is parsed with trailers")
                {
                    const auto parserStatus = parser.parse();

                    THEN("parser fails and reports too big message error")
                    {
                        REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                        REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
                    }
                }
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            size_t processedSize = 0;
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[processedSize++], 1);
                parserStatus = parser.parse();
            }

            THEN("parser successfully parses request metadata")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);

                AND_WHEN("last chunk is parsed with trailers")
                {
                    auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
                    while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
                    {
                        ioChannel.readBuffer().write(&request[processedSize++], 1);
                        parserStatus = parser.parse();
                    }

                    THEN("parser fails and reports too big message error")
                    {
                        REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                        REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
                    }
                }
            }
        }
    }
}


SCENARIO("HttpRequestParser enforces limit on chunk metadata")
{
    constexpr static size_t maxChunkMetadataSize = 5;
    std::shared_ptr<HttpRequestLimits> pHttpRequestLimits(new HttpRequestLimits);
    pHttpRequestLimits->maxChunkMetadataSize = maxChunkMetadataSize;

    GIVEN("request with chunk metadata larger than parser is allowed to accept is parsed")
    {
        const auto request = GENERATE(AS(std::string_view),
                                      "GET / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\nFFFFFF\r\n\r\n",
                                      "GET / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n0 ; name = value\r\n\r\n",
                                      "GET / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n1; name = \"I love quoted strings.\"\r\n\r\n");

        WHEN("request metadata is parsed at once")
        {
            IOChannelTest ioChannel(request);
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            const auto parserStatus = parser.parse();

            THEN("parser successfully parses request metadata")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);

                AND_WHEN("last chunk is parsed with trailers")
                {
                    const auto parserStatus = parser.parse();

                    THEN("parser fails and reports too big message error")
                    {
                        REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                        REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
                    }
                }
            }
        }

        WHEN("request is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            HttpRequestParser parser(ioChannel, pHttpRequestLimits);
            size_t processedSize = 0;
            auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
            while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
            {
                ioChannel.readBuffer().write(&request[processedSize++], 1);
                parserStatus = parser.parse();
            }

            THEN("parser successfully parses request metadata")
            {
                REQUIRE(parserStatus == HttpRequestParser::ParserStatus::ParsedRequest);

                AND_WHEN("last chunk is parsed with trailers")
                {
                    auto parserStatus = HttpRequestParser::ParserStatus::NeedsMoreData;
                    while (parserStatus == HttpRequestParser::ParserStatus::NeedsMoreData)
                    {
                        ioChannel.readBuffer().write(&request[processedSize++], 1);
                        parserStatus = parser.parse();
                    }

                    THEN("parser fails and reports too big message error")
                    {
                        REQUIRE(parserStatus == HttpRequestParser::ParserStatus::Failed);
                        REQUIRE(parser.error() == HttpServer::ServerError::TooBigRequest);
                    }
                }
            }
        }
    }
}


// SCENARIO("HttpRequestParser parses the same http request without headers and body sequentially")
// {
//    GIVEN("a get request")
//    {
//        std::string_view request("GET /plaintext HTTP/1.1\r\nHost: host.com\r\n\r\n");
//        IOChannelTest ioChannel(request);
//        HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));

//        WHEN("the request is parsed sequentially")
//        {
//            int64_t iterations = 5000000;
//            int64_t counter = 0;
//            QElapsedTimer timer;
//            timer.start();
//            do
//            {
//                if (HttpRequestParser::ParserStatus::ParsedRequest == parser.parse())
//                    ioChannel.readBuffer().write(request);
//                else
//                    FAIL("Failed to parse request");

//            }
//            while(++counter < iterations);
//            WARN(QByteArray("Parser processed ").append(QByteArray::number((1000000000.0*iterations)/timer.nsecsElapsed())).append(" req/s.").constData());
//        }
//    }
// }
