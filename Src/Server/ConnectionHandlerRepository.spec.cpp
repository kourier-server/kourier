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

#include "ConnectionHandlerRepository.h"
#include "ConnectionHandler.h"
#include <Spectator.h>
#include <set>
#include <memory>


using Kourier::ConnectionHandlerRepository;
using Kourier::ConnectionHandler;
using Kourier::Object;

namespace Tests::ConnectionHandlerRepository::Spec
{

class TestTcpConnectionHandler : public ConnectionHandler
{
KOURIER_OBJECT(Tests::ConnectionHandlerRepository::Spec::TestTcpConnectionHandler)
public:
    TestTcpConnectionHandler() {m_createdHandlers.insert(this);}
    TestTcpConnectionHandler(const TestTcpConnectionHandler&) = delete;
    TestTcpConnectionHandler &operator=(const TestTcpConnectionHandler&) = delete;
    ~TestTcpConnectionHandler() override {m_createdHandlers.erase(this);}
    void finish() override {m_isFinishing = true;}
    void emitFinished() {finished(this);}
    bool isFinishing() const {return m_isFinishing;}
    static const std::set<TestTcpConnectionHandler*> &createdHandlers() {return m_createdHandlers;}

private:
    bool m_isFinishing = false;
    static std::set<TestTcpConnectionHandler*> m_createdHandlers;
};

std::set<TestTcpConnectionHandler*> TestTcpConnectionHandler::m_createdHandlers;

}

using namespace Tests::ConnectionHandlerRepository::Spec;


SCENARIO("ConnectionHandlerRepository emits stopped after all handlers stop")
{
    GIVEN("a repository without any handlers")
    {
        ConnectionHandlerRepository repository;
        bool emittedStopped = false;
        Object::connect(&repository, &ConnectionHandlerRepository::stopped, [&emittedStopped](){emittedStopped = true;});

        WHEN("repository is stopped")
        {
            REQUIRE(!emittedStopped);
            repository.stop();

            THEN("repostiory emits stopped imediatelly")
            {
                REQUIRE(emittedStopped);

                AND_WHEN("repository is stopped again")
                {
                    emittedStopped = false;
                    repository.stop();

                    THEN("stopped is not emitted again")
                    {
                        REQUIRE(!emittedStopped);
                    }
                }

                AND_WHEN("a handler is added to the stopped repository")
                {
                    REQUIRE(TestTcpConnectionHandler::createdHandlers().empty());
                    auto *pHandler = new TestTcpConnectionHandler;
                    REQUIRE(TestTcpConnectionHandler::createdHandlers().size() == 1);
                    repository.add(pHandler);

                    THEN("repository deletes handler imediatelly")
                    {
                        REQUIRE(TestTcpConnectionHandler::createdHandlers().empty());
                    }
                }
            }
        }
    }

    GIVEN("a repository with handlers")
    {
        REQUIRE(TestTcpConnectionHandler::createdHandlers().empty());
        ConnectionHandlerRepository repository;
        bool emittedStopped = false;
        Object::connect(&repository, &ConnectionHandlerRepository::stopped, [&emittedStopped](){emittedStopped = true;});
        const auto handlerCount = GENERATE(AS(int), 1, 3, 5);
        for (auto i = 0; i < handlerCount; ++i)
        {
            repository.add(new TestTcpConnectionHandler);
            REQUIRE(TestTcpConnectionHandler::createdHandlers().size() == (i + 1));
        }
        for (auto *pHandler : TestTcpConnectionHandler::createdHandlers())
        {
            REQUIRE(!pHandler->isFinishing());
        }

        WHEN("repository is stopped")
        {
            repository.stop();

            THEN("repostiory stops all added handlers")
            {
                REQUIRE(!emittedStopped);
                for (auto *pHandler : TestTcpConnectionHandler::createdHandlers())
                {
                    REQUIRE(pHandler->isFinishing());
                }

                AND_WHEN("last handler emits finished")
                {
                    REQUIRE(!TestTcpConnectionHandler::createdHandlers().empty())
                    auto createdHandlers = TestTcpConnectionHandler::createdHandlers();
                    auto it = createdHandlers.cbegin();
                    auto *pLastHandlerToEmitFinished = *it;
                    ++it;
                    while (it != createdHandlers.cend())
                    {
                        auto *pHandler = *it;
                        REQUIRE(TestTcpConnectionHandler::createdHandlers().contains(pHandler));
                        pHandler->emitFinished();
                        QCoreApplication::processEvents();
                        REQUIRE(!TestTcpConnectionHandler::createdHandlers().contains(pHandler));
                        ++it;
                    }
                    REQUIRE(!emittedStopped);
                    REQUIRE(TestTcpConnectionHandler::createdHandlers().contains(pLastHandlerToEmitFinished));
                    pLastHandlerToEmitFinished->emitFinished();
                    QCoreApplication::processEvents();
                    REQUIRE(!TestTcpConnectionHandler::createdHandlers().contains(pLastHandlerToEmitFinished));
                    REQUIRE(TestTcpConnectionHandler::createdHandlers().empty());

                    THEN("repository emits stopped and deletes handlers")
                    {
                        REQUIRE(emittedStopped);

                        AND_WHEN("a handler is added to the stopped repository")
                        {
                            auto *pHandler = new TestTcpConnectionHandler;
                            REQUIRE(TestTcpConnectionHandler::createdHandlers().size() == 1);
                            repository.add(pHandler);

                            THEN("repository deletes handler imediatelly")
                            {
                                REQUIRE(TestTcpConnectionHandler::createdHandlers().empty());
                            }
                        }
                    }
                }

                AND_WHEN("a handler is added to the stopping repository")
                {
                    auto *pHandler = new TestTcpConnectionHandler;
                    REQUIRE(TestTcpConnectionHandler::createdHandlers().size() == (handlerCount + 1));
                    repository.add(pHandler);

                    THEN("repository deletes handler imediatelly")
                    {
                        REQUIRE(TestTcpConnectionHandler::createdHandlers().size() == handlerCount);
                    }
                }
            }
        }
    }

    GIVEN("a repository with handlers")
    {
        const auto handlerCount = GENERATE(AS(int), 0, 1, 3, 5);
        REQUIRE(TestTcpConnectionHandler::createdHandlers().empty());
        std::unique_ptr<ConnectionHandlerRepository> repository(new ConnectionHandlerRepository);

        for (auto i = 0; i < handlerCount; ++i)
        {
            repository->add(new TestTcpConnectionHandler);
            REQUIRE(TestTcpConnectionHandler::createdHandlers().size() == (i + 1));
        }

        WHEN("repository is deleted")
        {
            repository.reset();

            THEN("repository deletes all added handlers")
            {
                REQUIRE(TestTcpConnectionHandler::createdHandlers().empty());
            }
        }

        WHEN("repository is deleted after being stopped")
        {
            for (auto *pHandler : TestTcpConnectionHandler::createdHandlers())
            {
                REQUIRE(!pHandler->isFinishing());
            }
            repository->stop();
            for (auto *pHandler : TestTcpConnectionHandler::createdHandlers())
            {
                REQUIRE(pHandler->isFinishing());
            }
            repository.reset();

            THEN("repository deletes all added handlers")
            {
                REQUIRE(TestTcpConnectionHandler::createdHandlers().empty());
            }
        }
    }
}
