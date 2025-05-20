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

#include "HttpServerOptions.h"
#include "HttpFieldBlock.h"
#include <Spectator.h>
#include <utility>


using Kourier::HttpServerOptions;
using Kourier::HttpServer;
using Kourier::HttpFieldBlock;


SCENARIO("HttpServerOptions returns default option values")
{
    GIVEN("an HttpServerOptions instance")
    {
        const HttpServerOptions serverOptions;

        WHEN("option value is fetched")
        {
            const auto option = GENERATE(AS(HttpServer::ServerOption),
                                         HttpServer::ServerOption::WorkerCount,
                                         HttpServer::ServerOption::TcpServerBacklogSize,
                                         HttpServer::ServerOption::IdleTimeoutInSecs,
                                         HttpServer::ServerOption::RequestTimeoutInSecs,
                                         HttpServer::ServerOption::MaxUrlSize,
                                         HttpServer::ServerOption::MaxHeaderNameSize,
                                         HttpServer::ServerOption::MaxHeaderValueSize,
                                         HttpServer::ServerOption::MaxHeaderLineCount,
                                         HttpServer::ServerOption::MaxTrailerNameSize,
                                         HttpServer::ServerOption::MaxTrailerValueSize,
                                         HttpServer::ServerOption::MaxTrailerLineCount,
                                         HttpServer::ServerOption::MaxChunkMetadataSize,
                                         HttpServer::ServerOption::MaxRequestSize,
                                         HttpServer::ServerOption::MaxBodySize,
                                         HttpServer::ServerOption::MaxConnectionCount);
            const auto optionValue = serverOptions.getOption(option);

            THEN("instance returns expected default value")
            {
                REQUIRE(optionValue == HttpServerOptions::defaultOptionValue(option));
            }
        }
    }
}


SCENARIO("HttpServerOptions does not accept negative values")
{
    GIVEN("an HttpServerOptions instance")
    {
        HttpServerOptions serverOptions;

        WHEN("negative option values are set for option")
        {
            const auto option = GENERATE(AS(HttpServer::ServerOption),
                                         HttpServer::ServerOption::WorkerCount,
                                         HttpServer::ServerOption::TcpServerBacklogSize,
                                         HttpServer::ServerOption::IdleTimeoutInSecs,
                                         HttpServer::ServerOption::RequestTimeoutInSecs,
                                         HttpServer::ServerOption::MaxUrlSize,
                                         HttpServer::ServerOption::MaxHeaderNameSize,
                                         HttpServer::ServerOption::MaxHeaderValueSize,
                                         HttpServer::ServerOption::MaxHeaderLineCount,
                                         HttpServer::ServerOption::MaxTrailerNameSize,
                                         HttpServer::ServerOption::MaxTrailerValueSize,
                                         HttpServer::ServerOption::MaxTrailerLineCount,
                                         HttpServer::ServerOption::MaxChunkMetadataSize,
                                         HttpServer::ServerOption::MaxRequestSize,
                                         HttpServer::ServerOption::MaxBodySize,
                                         HttpServer::ServerOption::MaxConnectionCount);
            const auto value = GENERATE(AS(int64_t), -1, -52, std::numeric_limits<int64_t>::min() + 1, std::numeric_limits<int64_t>::min());
            const auto succeededToSetNegativeValue = serverOptions.setOption(option, value);

            THEN("option fails to be set to given negative value")
            {
                REQUIRE(!succeededToSetNegativeValue);
                REQUIRE(serverOptions.errorMessage() == "Failed to set option. Option values must be non-negative.");
            }
        }
    }
}


SCENARIO("HttpServerOptions sets maximum option values for request limits and max connection count when given value is zero")
{
    GIVEN("an HttpServerOptions instance")
    {
        HttpServerOptions serverOptions;

        WHEN("zero is set for option")
        {
            const auto option = GENERATE(AS(std::pair<HttpServer::ServerOption, bool>),
                                         {HttpServer::ServerOption::WorkerCount, false},
                                         {HttpServer::ServerOption::IdleTimeoutInSecs, false},
                                         {HttpServer::ServerOption::RequestTimeoutInSecs, false},
                                         {HttpServer::ServerOption::MaxUrlSize, true},
                                         {HttpServer::ServerOption::MaxHeaderNameSize, true},
                                         {HttpServer::ServerOption::MaxHeaderValueSize, true},
                                         {HttpServer::ServerOption::MaxHeaderLineCount, true},
                                         {HttpServer::ServerOption::MaxTrailerNameSize, true},
                                         {HttpServer::ServerOption::MaxTrailerValueSize, true},
                                         {HttpServer::ServerOption::MaxTrailerLineCount, true},
                                         {HttpServer::ServerOption::MaxChunkMetadataSize, true},
                                         {HttpServer::ServerOption::MaxRequestSize, true},
                                         {HttpServer::ServerOption::MaxBodySize, true},
                                         {HttpServer::ServerOption::MaxConnectionCount, true});
            const auto succeeded = serverOptions.setOption(option.first, 0);
            const auto optionValue = serverOptions.getOption(option.first);

            THEN("instance sets maximum value for options regarding request limits and max connection count")
            {
                REQUIRE(succeeded);
                REQUIRE(optionValue == (option.second ? HttpServerOptions::maxOptionValue(option.first) : 0));
            }
        }
    }
}


SCENARIO("HttpServerOptions does not allow workerCount values greater than QThread::idealThreadCount()")
{
    GIVEN("an HttpServerOptions instance")
    {
        HttpServerOptions serverOptions;

        WHEN("a value up to QThread::idealThreadCount() is set for workerCount")
        {
            const auto workerCount = GENERATE_RANGE(AS(int), 0, QThread::idealThreadCount());
            REQUIRE(serverOptions.errorMessage().empty());
            const auto succeeded = serverOptions.setOption(HttpServer::ServerOption::WorkerCount, workerCount);

            THEN("HttpServerOptions succeeds to set worker count")
            {
                REQUIRE(succeeded);
                REQUIRE(serverOptions.errorMessage().empty());
                REQUIRE(workerCount == serverOptions.getOption(HttpServer::ServerOption::WorkerCount));
            }
        }

        WHEN("a value greater than QThread::idealThreadCount() is set for workerCount")
        {
            const auto largerDelta = GENERATE(AS(int), 1, 5, 1024);
            const auto largerWorkerCount = QThread::idealThreadCount() + largerDelta;
            REQUIRE(serverOptions.errorMessage().empty());
            const auto succeeded = serverOptions.setOption(HttpServer::ServerOption::WorkerCount, largerWorkerCount);

            THEN("HttpServerOptions fails to set worker count")
            {
                REQUIRE(!succeeded);
                REQUIRE(!serverOptions.errorMessage().empty());
                REQUIRE(HttpServerOptions::defaultOptionValue(HttpServer::ServerOption::WorkerCount) == serverOptions.getOption(HttpServer::ServerOption::WorkerCount));
            }
        }
    }
}


SCENARIO("HttpServerOptions does not allow tcp server backlog size value equal to zero")
{
    GIVEN("an HttpServerOptions instance")
    {
        HttpServerOptions serverOptions;

        WHEN("a positive integer is set for tcpServerBacklogSize")
        {
            const auto backlogSize = GENERATE(AS(int64_t), 1, 8, 4096, 1 << 20);
            REQUIRE(serverOptions.errorMessage().empty());
            const auto succeeded = serverOptions.setOption(HttpServer::ServerOption::TcpServerBacklogSize, backlogSize);

            THEN("HttpServerOptions succeeds to set backlog size")
            {
                REQUIRE(succeeded);
                REQUIRE(serverOptions.errorMessage().empty());
                REQUIRE(backlogSize == serverOptions.getOption(HttpServer::ServerOption::TcpServerBacklogSize));
            }
        }

        WHEN("a value equal to zero is set for backlog size")
        {
            REQUIRE(serverOptions.errorMessage().empty());
            const auto succeeded = serverOptions.setOption(HttpServer::ServerOption::TcpServerBacklogSize, 0);

            THEN("HttpServerOptions fails to set worker count")
            {
                REQUIRE(!succeeded);
                REQUIRE(!serverOptions.errorMessage().empty());
                REQUIRE(HttpServerOptions::defaultOptionValue(HttpServer::ServerOption::TcpServerBacklogSize) == serverOptions.getOption(HttpServer::ServerOption::TcpServerBacklogSize));
                REQUIRE(serverOptions.getOption(HttpServer::ServerOption::TcpServerBacklogSize) > 0);
            }
        }
    }
}


SCENARIO("HttpServerOptions does not allow tcp server backlog sizes greater than 1 << 31")
{
    GIVEN("an HttpServerOptions instance")
    {
        HttpServerOptions serverOptions;

        WHEN("a value grater than 1 << 31 is set for server backlog size")
        {
            const auto serverBacklogSize = GENERATE(AS(int64_t), (size_t(1) << 31) + 1, (size_t(1) << 31) + 1024, size_t(1) << 58);
            REQUIRE(serverOptions.errorMessage().empty());
            const auto succeeded = serverOptions.setOption(HttpServer::ServerOption::TcpServerBacklogSize, serverBacklogSize);

            THEN("HttpServerOptions fails to set backlog size")
            {
                REQUIRE(!succeeded);
                REQUIRE(!serverOptions.errorMessage().empty());
                REQUIRE(HttpServerOptions::defaultOptionValue(HttpServer::ServerOption::TcpServerBacklogSize) == serverOptions.getOption(HttpServer::ServerOption::TcpServerBacklogSize));
                REQUIRE(serverOptions.getOption(HttpServer::ServerOption::TcpServerBacklogSize) == 4096);
            }
        }
    }
}


SCENARIO("HttpServerOptions does not allow timeout values greater than 1 << 31")
{
    GIVEN("an HttpServerOptions instance")
    {
        HttpServerOptions serverOptions;

        WHEN("a value grater than 1 << 31 is set for request timeout")
        {
            const auto requestTimeoutInSecs = GENERATE(AS(int64_t), (size_t(1) << 31) + 1, (size_t(1) << 31) + 1024, size_t(1) << 58);
            REQUIRE(serverOptions.errorMessage().empty());
            const auto succeeded = serverOptions.setOption(HttpServer::ServerOption::RequestTimeoutInSecs, requestTimeoutInSecs);

            THEN("HttpServerOptions fails to set request timeout")
            {
                REQUIRE(!succeeded);
                REQUIRE(!serverOptions.errorMessage().empty());
                REQUIRE(HttpServerOptions::defaultOptionValue(HttpServer::ServerOption::RequestTimeoutInSecs) == serverOptions.getOption(HttpServer::ServerOption::RequestTimeoutInSecs));
                REQUIRE(serverOptions.getOption(HttpServer::ServerOption::RequestTimeoutInSecs) == 0);
            }
        }

        WHEN("a value grater than 1 << 31 is set for idle timeout")
        {
            const auto idleTimeoutInSecs = GENERATE(AS(int64_t), (size_t(1) << 31) + 1, (size_t(1) << 31) + 1024, size_t(1) << 58);
            REQUIRE(serverOptions.errorMessage().empty());
            const auto succeeded = serverOptions.setOption(HttpServer::ServerOption::IdleTimeoutInSecs, idleTimeoutInSecs);

            THEN("HttpServerOptions fails to set idle timeout")
            {
                REQUIRE(!succeeded);
                REQUIRE(!serverOptions.errorMessage().empty());
                REQUIRE(HttpServerOptions::defaultOptionValue(HttpServer::ServerOption::IdleTimeoutInSecs) == serverOptions.getOption(HttpServer::ServerOption::IdleTimeoutInSecs));
                REQUIRE(serverOptions.getOption(HttpServer::ServerOption::IdleTimeoutInSecs) == 0);
            }
        }
    }
}


SCENARIO("HttpServerOptions does not allow header/trailer field names sizes greater than HttpFieldBlock::maxFieldNameSize")
{
    GIVEN("an HttpServerOptions instance")
    {
        HttpServerOptions serverOptions;

        WHEN("a value grater than HttpFieldBlock::maxFieldNameSize is set for header/trailer name size")
        {
            const auto fieldNameSize = GENERATE(AS(int64_t),
                                                    HttpFieldBlock::maxFieldNameSize() + 1,
                                                    HttpFieldBlock::maxFieldNameSize() + 1024,
                                                    std::numeric_limits<int64_t>::max(),
                                                    std::numeric_limits<int64_t>::max() - 1);
            const auto optionToSet = GENERATE(AS(HttpServer::ServerOption),
                                              HttpServer::ServerOption::MaxHeaderNameSize,
                                              HttpServer::ServerOption::MaxTrailerNameSize);
            REQUIRE(serverOptions.errorMessage().empty());
            const auto succeeded = serverOptions.setOption(optionToSet, fieldNameSize);

            THEN("HttpServerOptions fails to set field name size")
            {
                REQUIRE(!succeeded);
                REQUIRE(!serverOptions.errorMessage().empty());
                REQUIRE(HttpServerOptions::defaultOptionValue(optionToSet) == serverOptions.getOption(optionToSet));
            }
        }
    }
}


SCENARIO("HttpServerOptions does not allow header/trailer field value sizes greater than HttpFieldBlock::maxFieldValueSize")
{
    GIVEN("an HttpServerOptions instance")
    {
        HttpServerOptions serverOptions;

        WHEN("a value grater than HttpFieldBlock::maxFieldValueSize is set for header/trailer value size")
        {
            const auto fieldValueSize = GENERATE(AS(int64_t),
                                                HttpFieldBlock::maxFieldValueSize() + 1,
                                                HttpFieldBlock::maxFieldValueSize() + 1024,
                                                std::numeric_limits<int64_t>::max(),
                                                std::numeric_limits<int64_t>::max() - 1);
            const auto optionToSet = GENERATE(AS(HttpServer::ServerOption),
                                              HttpServer::ServerOption::MaxHeaderValueSize,
                                              HttpServer::ServerOption::MaxTrailerValueSize);
            REQUIRE(serverOptions.errorMessage().empty());
            const auto succeeded = serverOptions.setOption(optionToSet, fieldValueSize);

            THEN("HttpServerOptions fails to set field value size")
            {
                REQUIRE(!succeeded);
                REQUIRE(!serverOptions.errorMessage().empty());
                REQUIRE(HttpServerOptions::defaultOptionValue(optionToSet) == serverOptions.getOption(optionToSet));
            }
        }
    }
}


SCENARIO("HttpServerOptions does not allow header/trailer line counts greater than HttpFieldBlock::maxFieldLines")
{
    GIVEN("an HttpServerOptions instance")
    {
        HttpServerOptions serverOptions;

        WHEN("a value grater than HttpFieldBlock::maxFieldLines is set for header/trailer line count")
        {
            const auto fieldLineCount = GENERATE(AS(int64_t),
                                                 HttpFieldBlock::maxFieldLines() + 1,
                                                 HttpFieldBlock::maxFieldLines() + 1024,
                                                 std::numeric_limits<int64_t>::max(),
                                                 std::numeric_limits<int64_t>::max() - 1);
            const auto optionToSet = GENERATE(AS(HttpServer::ServerOption),
                                              HttpServer::ServerOption::MaxHeaderLineCount,
                                              HttpServer::ServerOption::MaxTrailerLineCount);
            REQUIRE(serverOptions.errorMessage().empty());
            const auto succeeded = serverOptions.setOption(optionToSet, fieldLineCount);

            THEN("HttpServerOptions fails to set field line count")
            {
                REQUIRE(!succeeded);
                REQUIRE(!serverOptions.errorMessage().empty());
                REQUIRE(HttpServerOptions::defaultOptionValue(optionToSet) == serverOptions.getOption(optionToSet));
            }
        }
    }
}
