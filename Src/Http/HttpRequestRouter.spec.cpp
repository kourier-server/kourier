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

#include "HttpRequestRouter.h"
#include "HttpRequestParser.h"
#include "HttpBroker.h"
#include "HttpBrokerPrivate.h"
#include <Spectator.h>
#include <vector>
#include <utility>

using Kourier::HttpRequestRouter;
using RequestHandler = Kourier::HttpRequestRouter::RequestHandler;
using Kourier::HttpRequest;
using Kourier::HttpBroker;
using Kourier::HttpBrokerPrivate;
using Kourier::IOChannel;
using Kourier::RingBuffer;
using Kourier::DataSource;
using Kourier::DataSink;
using Kourier::HttpRequestParser;
using Kourier::HttpRequestLimits;


namespace Test::HttpRequestRouter
{

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
    DataSink &dataSink() override {std::abort();}
    void onReadNotificationChanged() override {}
    void onWriteNotificationChanged() override {}
};


class TestHttpRequestRouter
{
public:
    TestHttpRequestRouter() :
        m_ioChannel({}),
        m_parser(m_ioChannel, std::make_shared<HttpRequestLimits>()),
        m_brokerPrivate(&m_ioChannel, &m_parser),
        m_broker(&m_brokerPrivate) {}
    inline HttpBroker &broker() {return m_broker;}

private:
    IOChannelTest m_ioChannel;
    HttpRequestParser m_parser;
    Kourier::HttpBrokerPrivate m_brokerPrivate;
    HttpBroker m_broker;
};

}

using namespace Test::HttpRequestRouter;


SCENARIO("HttpRequestRouter validates paths and pointer to request handler function")
{
    GIVEN("a non-absolute path")
    {
        const auto method = GENERATE(AS(HttpRequest::Method),
                                     HttpRequest::Method::POST,
                                     HttpRequest::Method::PUT,
                                     HttpRequest::Method::PATCH,
                                     HttpRequest::Method::DELETE,
                                     HttpRequest::Method::HEAD,
                                     HttpRequest::Method::GET,
                                     HttpRequest::Method::OPTIONS);
        const auto nonAbsolutePath = GENERATE(AS(std::string_view),
                                              "a/non/abosolute/path",
                                              "",
                                              "http://host.com:1234/scheme/and/authority/are/not/allowed",
                                              "1234",
                                              "//absolute/paths/cannot/start/with/double/slashes",
                                              "/an/absolute/path/with/query?a=3",
                                              "/an/absolute/path/with/fragment?#frag",
                                              "/an/absolute/path/with/query/and/fragment?a=3#frag");

        WHEN("a route is added with given path")
        {
            HttpRequestRouter router;
            const auto succeeded = router.addRoute(method, nonAbsolutePath, (RequestHandler)0x1);

            THEN("HttpRequestRouter fails to add the route")
            {
                REQUIRE(!succeeded);
            }
        }
    }

    GIVEN("an absolute path")
    {
        const auto method = GENERATE(AS(HttpRequest::Method),
                                     HttpRequest::Method::POST,
                                     HttpRequest::Method::PUT,
                                     HttpRequest::Method::PATCH,
                                     HttpRequest::Method::DELETE,
                                     HttpRequest::Method::HEAD,
                                     HttpRequest::Method::GET,
                                     HttpRequest::Method::OPTIONS);
        const auto absolutePath = GENERATE(AS(std::string_view),
                                              "/",
                                              "/an/absolute/path",
                                              "/an////absolute/////path/////");

        WHEN("route is added with given path")
        {
            HttpRequestRouter router;
            const auto succeeded = router.addRoute(method, absolutePath, (RequestHandler)0x1);

            THEN("HttpRequestRouter successfully adds the route")
            {
                REQUIRE(succeeded);
            }
        }

        WHEN("route is added with given path and a null pointer to function")
        {
            HttpRequestRouter router;
            const auto succeeded = router.addRoute(method, absolutePath, nullptr);

            THEN("HttpRequestRouter fails to add the route")
            {
                REQUIRE(!succeeded);
            }
        }
    }

    GIVEN("a server-wide options route")
    {
        const auto method = HttpRequest::Method::OPTIONS;
        const auto targetURI = "*";

        WHEN("route is added")
        {
            HttpRequestRouter router;
            const auto succeeded = router.addRoute(method, targetURI, (RequestHandler)0x1);

            THEN("HttpRequestRouter successfully adds the route")
            {
                REQUIRE(succeeded);
            }
        }

        WHEN("route is added with given path and a null pointer to function")
        {
            HttpRequestRouter router;
            const auto succeeded = router.addRoute(method, targetURI, nullptr);

            THEN("HttpRequestRouter fails to add the route")
            {
                REQUIRE(!succeeded);
            }
        }

        WHEN("route is added with non-OPTIONS method")
        {
            const auto nonOptionsMethod = GENERATE(AS(HttpRequest::Method),
                                         HttpRequest::Method::POST,
                                         HttpRequest::Method::PUT,
                                         HttpRequest::Method::PATCH,
                                         HttpRequest::Method::DELETE,
                                         HttpRequest::Method::HEAD,
                                         HttpRequest::Method::GET);
            HttpRequestRouter router;
            const auto succeeded = router.addRoute(nonOptionsMethod, targetURI, (RequestHandler)0x1);

            THEN("HttpRequestRouter fails to add the route")
            {
                REQUIRE(!succeeded);
            }
        }
    }
}


SCENARIO("HttpRequestRouter gets handler using the most specific route for given method")
{
    GIVEN("a set of routes for given method")
    {
        const auto method = GENERATE(AS(HttpRequest::Method),
                                     HttpRequest::Method::POST,
                                     HttpRequest::Method::PUT,
                                     HttpRequest::Method::PATCH,
                                     HttpRequest::Method::DELETE,
                                     HttpRequest::Method::HEAD,
                                     HttpRequest::Method::GET,
                                     HttpRequest::Method::OPTIONS);
        static std::string route;
        HttpRequestRouter router;
        REQUIRE(router.addRoute(method, "/a", [](const HttpRequest &, HttpBroker&){route = "/a";}));
        REQUIRE(router.addRoute(method, "/a/more", [](const HttpRequest &, HttpBroker&){route = "/a/more";}));
        REQUIRE(router.addRoute(method, "/a/more/specific", [](const HttpRequest &, HttpBroker&){route = "/a/more/specific";}));
        REQUIRE(router.addRoute(method, "/a/more/specific/route", [](const HttpRequest &, HttpBroker&){route = "/a/more/specific/route";}));
        REQUIRE(router.addRoute(method, "/amore", [](const HttpRequest &, HttpBroker&){route = "/amore";}));
        REQUIRE(router.addRoute(method, "/amorespecific", [](const HttpRequest &, HttpBroker&){route = "/amorespecific";}));
        REQUIRE(router.addRoute(method, "/amorespecificroute", [](const HttpRequest &, HttpBroker&){route = "/amorespecificroute";}));

        WHEN("handler is fetched for given method/path")
        {
            std::vector<std::pair<std::string_view, RequestHandler>> pathHandlerVector({
                {"/a", router.getHandler(method, "/a")},
                {"/a", router.getHandler(method, "/abcd")},
                {"/a", router.getHandler(method, "/a/")},
                {"/a", router.getHandler(method, "/a/m")},
                {"/a", router.getHandler(method, "/a/mo")},
                {"/a", router.getHandler(method, "/a/mor")},
                {"/a/more", router.getHandler(method, "/a/more")},
                {"/a/more", router.getHandler(method, "/a/more/")},
                {"/a/more", router.getHandler(method, "/a/more/s")},
                {"/a/more", router.getHandler(method, "/a/more/sp")},
                {"/a/more", router.getHandler(method, "/a/more/spe")},
                {"/a/more", router.getHandler(method, "/a/more/spec")},
                {"/a/more", router.getHandler(method, "/a/more/speci")},
                {"/a/more", router.getHandler(method, "/a/more/specif")},
                {"/a/more", router.getHandler(method, "/a/more/specifi")},
                {"/a/more/specific", router.getHandler(method, "/a/more/specific")},
                {"/a/more/specific", router.getHandler(method, "/a/more/specific/")},
                {"/a/more/specific", router.getHandler(method, "/a/more/specific/r")},
                {"/a/more/specific", router.getHandler(method, "/a/more/specific/ro")},
                {"/a/more/specific", router.getHandler(method, "/a/more/specific/rou")},
                {"/a/more/specific", router.getHandler(method, "/a/more/specific/rout")},
                {"/a/more/specific/route", router.getHandler(method, "/a/more/specific/route")},
                {"/a", router.getHandler(method, "/am")},
                {"/a", router.getHandler(method, "/amo")},
                {"/a", router.getHandler(method, "/amor")},
                {"/amore", router.getHandler(method, "/amore")},
                {"/amore", router.getHandler(method, "/amores")},
                {"/amore", router.getHandler(method, "/amoresp")},
                {"/amore", router.getHandler(method, "/amorespe")},
                {"/amore", router.getHandler(method, "/amorespec")},
                {"/amore", router.getHandler(method, "/amorespeci")},
                {"/amore", router.getHandler(method, "/amorespecif")},
                {"/amore", router.getHandler(method, "/amorespecifi")},
                {"/amorespecific", router.getHandler(method, "/amorespecific")},
                {"/amorespecific", router.getHandler(method, "/amorespecificr")},
                {"/amorespecific", router.getHandler(method, "/amorespecificro")},
                {"/amorespecific", router.getHandler(method, "/amorespecificrou")},
                {"/amorespecific", router.getHandler(method, "/amorespecificrout")},
                {"/amorespecificroute", router.getHandler(method, "/amorespecificroute")},
                {"/amorespecificroute", router.getHandler(method, "/amorespecificrouteshereplease")}
            });

            THEN("handler associated with most specific path is given")
            {
                IOChannelTest ioChannel({});
                HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
                TestHttpRequestRouter broker;
                for (auto &[expectedPath, pHandler] : pathHandlerVector)
                {
                    REQUIRE(pHandler != nullptr);
                    route.clear();
                    pHandler(parser.request(), broker.broker());
                    REQUIRE(route == expectedPath);
                }
            }
        }
    }
}


SCENARIO("HttpRequestRouter replaces handler when adding route with existing path/method")
{
    GIVEN("a set of routes for given method")
    {
        const auto method = GENERATE(AS(HttpRequest::Method),
                                     HttpRequest::Method::POST,
                                     HttpRequest::Method::PUT,
                                     HttpRequest::Method::PATCH,
                                     HttpRequest::Method::DELETE,
                                     HttpRequest::Method::HEAD,
                                     HttpRequest::Method::GET,
                                     HttpRequest::Method::OPTIONS);
        static std::string route;
        HttpRequestRouter router;
        REQUIRE(router.addRoute(method, "/a", [](const HttpRequest &, HttpBroker&){route = "/a";}));
        REQUIRE(router.addRoute(method, "/a/more", [](const HttpRequest &, HttpBroker&){route = "/a/more";}));
        REQUIRE(router.addRoute(method, "/a/more/specific", [](const HttpRequest &, HttpBroker&){route = "/a/more/specific";}));
        REQUIRE(router.addRoute(method, "/a/more/specific/route", [](const HttpRequest &, HttpBroker&){route = "/a/more/specific/route";}));
        REQUIRE(router.addRoute(method, "/amore", [](const HttpRequest &, HttpBroker&){route = "/amore";}));
        REQUIRE(router.addRoute(method, "/amorespecific", [](const HttpRequest &, HttpBroker&){route = "/amorespecific";}));
        REQUIRE(router.addRoute(method, "/amorespecificroute", [](const HttpRequest &, HttpBroker&){route = "/amorespecificroute";}));

        WHEN("handler is fetched for given method/path after added routes are replaced")
        {
            REQUIRE(router.addRoute(method, "/a", [](const HttpRequest &, HttpBroker&){route = "replaced: /a";}));
            REQUIRE(router.addRoute(method, "/a/more", [](const HttpRequest &, HttpBroker&){route = "replaced: /a/more";}));
            REQUIRE(router.addRoute(method, "/a/more/specific", [](const HttpRequest &, HttpBroker&){route = "replaced: /a/more/specific";}));
            REQUIRE(router.addRoute(method, "/a/more/specific/route", [](const HttpRequest &, HttpBroker&){route = "replaced: /a/more/specific/route";}));
            REQUIRE(router.addRoute(method, "/amore", [](const HttpRequest &, HttpBroker&){route = "replaced: /amore";}));
            REQUIRE(router.addRoute(method, "/amorespecific", [](const HttpRequest &, HttpBroker&){route = "replaced: /amorespecific";}));
            REQUIRE(router.addRoute(method, "/amorespecificroute", [](const HttpRequest &, HttpBroker&){route = "replaced: /amorespecificroute";}));

            std::vector<std::pair<std::string_view, RequestHandler>> pathHandlerVector({
                {"replaced: /a", router.getHandler(method, "/a")},
                {"replaced: /a", router.getHandler(method, "/abcd")},
                {"replaced: /a", router.getHandler(method, "/a/")},
                {"replaced: /a", router.getHandler(method, "/a/m")},
                {"replaced: /a", router.getHandler(method, "/a/mo")},
                {"replaced: /a", router.getHandler(method, "/a/mor")},
                {"replaced: /a/more", router.getHandler(method, "/a/more")},
                {"replaced: /a/more", router.getHandler(method, "/a/more/")},
                {"replaced: /a/more", router.getHandler(method, "/a/more/s")},
                {"replaced: /a/more", router.getHandler(method, "/a/more/sp")},
                {"replaced: /a/more", router.getHandler(method, "/a/more/spe")},
                {"replaced: /a/more", router.getHandler(method, "/a/more/spec")},
                {"replaced: /a/more", router.getHandler(method, "/a/more/speci")},
                {"replaced: /a/more", router.getHandler(method, "/a/more/specif")},
                {"replaced: /a/more", router.getHandler(method, "/a/more/specifi")},
                {"replaced: /a/more/specific", router.getHandler(method, "/a/more/specific")},
                {"replaced: /a/more/specific", router.getHandler(method, "/a/more/specific/")},
                {"replaced: /a/more/specific", router.getHandler(method, "/a/more/specific/r")},
                {"replaced: /a/more/specific", router.getHandler(method, "/a/more/specific/ro")},
                {"replaced: /a/more/specific", router.getHandler(method, "/a/more/specific/rou")},
                {"replaced: /a/more/specific", router.getHandler(method, "/a/more/specific/rout")},
                {"replaced: /a/more/specific/route", router.getHandler(method, "/a/more/specific/route")},
                {"replaced: /a", router.getHandler(method, "/am")},
                {"replaced: /a", router.getHandler(method, "/amo")},
                {"replaced: /a", router.getHandler(method, "/amor")},
                {"replaced: /amore", router.getHandler(method, "/amore")},
                {"replaced: /amore", router.getHandler(method, "/amores")},
                {"replaced: /amore", router.getHandler(method, "/amoresp")},
                {"replaced: /amore", router.getHandler(method, "/amorespe")},
                {"replaced: /amore", router.getHandler(method, "/amorespec")},
                {"replaced: /amore", router.getHandler(method, "/amorespeci")},
                {"replaced: /amore", router.getHandler(method, "/amorespecif")},
                {"replaced: /amore", router.getHandler(method, "/amorespecifi")},
                {"replaced: /amorespecific", router.getHandler(method, "/amorespecific")},
                {"replaced: /amorespecific", router.getHandler(method, "/amorespecificr")},
                {"replaced: /amorespecific", router.getHandler(method, "/amorespecificro")},
                {"replaced: /amorespecific", router.getHandler(method, "/amorespecificrou")},
                {"replaced: /amorespecific", router.getHandler(method, "/amorespecificrout")},
                {"replaced: /amorespecificroute", router.getHandler(method, "/amorespecificroute")},
                {"replaced: /amorespecificroute", router.getHandler(method, "/amorespecificrouteshereplease")}
            });

            THEN("replaced handler associated with most specific path is given")
            {
                IOChannelTest ioChannel({});
                HttpRequestParser parser(ioChannel, std::shared_ptr<HttpRequestLimits>(new HttpRequestLimits));
                TestHttpRequestRouter broker;
                for (auto &[expectedPath, pHandler] : pathHandlerVector)
                {
                    REQUIRE(pHandler != nullptr);
                    route.clear();
                    pHandler(parser.request(), broker.broker());
                    REQUIRE(route == expectedPath);
                }
            }
        }
    }
}
