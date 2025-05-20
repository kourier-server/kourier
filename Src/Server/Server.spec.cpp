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

#include "Server.h"
#include "ServerWorker.h"
#include "ServerWorkerFactory.h"
#include <QCoreApplication>
#include <QThread>
#include <set>
#include <Spectator.h>

using Kourier::ExecutionState;
using Kourier::Server;
using Kourier::ServerWorker;
using Kourier::ServerWorkerFactory;


namespace Tests::Server::Spec
{
class TestServerWorker : public ServerWorker
{
Q_OBJECT
public:
    TestServerWorker() {m_createdWorkers.insert(this);}
    ~TestServerWorker() override  {m_createdWorkers.erase(this);}
    static size_t createdWorkersCount() {return m_createdWorkers.size();}
    static const std::set<TestServerWorker*> &createdWorkers() {return m_createdWorkers;}
    ExecutionState state() const override {return m_state;}
    void setStarted()
    {
        REQUIRE(m_state == ExecutionState::Starting);
        m_state = ExecutionState::Started;
        emit started();
    }
    void setStopped()
    {
        REQUIRE(m_state == ExecutionState::Stopping);
        m_state = ExecutionState::Stopped;
        emit stopped();
    }
    void setFailed(std::string_view errorMessage)
    {
        REQUIRE(m_state == ExecutionState::Starting);
        m_state = ExecutionState::Stopped;
        emit failed(errorMessage);
    }
    static QVariantList &startData() {return m_startData;}
    static int startCallCount() {return m_startCallCount;}
    static int stopCallCount() {return m_stopCallCount;}
    static void clear()
    {
        m_createdWorkers.clear();
        m_startData.clear();
        m_startCallCount = 0;
        m_stopCallCount = 0;
    }

private:
    void doStart(QVariant data) override
    {
        REQUIRE(m_state == ExecutionState::Stopped);
        m_state = ExecutionState::Starting;
        m_startData.append(data);
        ++m_startCallCount;
    }
    void doStop() override
    {
        REQUIRE(m_state == ExecutionState::Started);
        m_state = ExecutionState::Stopping;
        ++m_stopCallCount;
    }

private:
    ExecutionState m_state = ExecutionState::Stopped;
    static std::set<TestServerWorker*> m_createdWorkers;
    static QVariantList m_startData;
    static int m_startCallCount;
    static int m_stopCallCount;
};

std::set<TestServerWorker*> TestServerWorker::m_createdWorkers;
QVariantList TestServerWorker::m_startData;
int TestServerWorker::m_startCallCount = 0;
int TestServerWorker::m_stopCallCount = 0;


class TestServerWorkerFactory : public ServerWorkerFactory
{
public:
    TestServerWorkerFactory() = default;
    ~TestServerWorkerFactory() override = default;
    std::shared_ptr<ServerWorker> create() override {return std::shared_ptr<ServerWorker>(new TestServerWorker);}
};
}

using namespace Tests::Server::Spec;


SCENARIO("Server is created in Stopped state")
{
    GIVEN("a server")
    {
        Server server(std::shared_ptr<ServerWorkerFactory>(new TestServerWorkerFactory));

        WHEN("server state is fetched")
        {
            const auto serverState = server.state();

            THEN("server is in Stopped state")
            {
                REQUIRE(serverState == ExecutionState::Stopped);
            }
        }
    }
}


SCENARIO("Server uses all available cores by default")
{
    GIVEN("a server")
    {
        Server server(std::shared_ptr<ServerWorkerFactory>(new TestServerWorkerFactory));

        WHEN("worker count is fetched")
        {
            const auto workerCount = server.workerCount();

            THEN("server uses as many workers as cores by default")
            {
                REQUIRE(workerCount == QThread::idealThreadCount());

                AND_WHEN("a positive value smaller or equal to core count is set")
                {
                    const auto workerCountToSet = GENERATE_RANGE(AS(int), 1, QThread::idealThreadCount());
                    server.setWorkerCount(workerCountToSet);

                    THEN("server uses given value as worker count")
                    {
                        REQUIRE(server.workerCount() == workerCountToSet);

                        AND_WHEN("a non positive value is used")
                        {
                            const auto nonPositiveValue = GENERATE(AS(int), 0, -1, -2, -121);
                            server.setWorkerCount(nonPositiveValue);

                            THEN("server does not change worker count")
                            {
                                REQUIRE(server.workerCount() == workerCountToSet);
                            }
                        }

                        AND_WHEN("a value greater than the core count is used")
                        {
                            const auto positiveValue = GENERATE(AS(int), 1, 2, 121);
                            const auto tooLargeValue = QThread::idealThreadCount() + positiveValue;
                            server.setWorkerCount(tooLargeValue);

                            THEN("server does not change worker count")
                            {
                                REQUIRE(server.workerCount() == workerCountToSet);
                            }
                        }
                    }
                }
            }
        }
    }
}


SCENARIO("Server creates as many workers as given worker count")
{
    GIVEN("a server")
    {
        REQUIRE(TestServerWorker::createdWorkersCount() == 0);
        TestServerWorker::clear();
        Server server(std::shared_ptr<ServerWorkerFactory>(new TestServerWorkerFactory));
        const auto workerCountToTry = GENERATE(AS(int), 0, 1, 3, 5);
        server.setWorkerCount(workerCountToTry);

        WHEN("server is started")
        {
            const auto data = GENERATE(AS(QVariant), QVariant(int(3)), QVariant(QByteArray("some data")));
            REQUIRE(server.state() == ExecutionState::Stopped);
            server.start(data);
            REQUIRE(server.state() == ExecutionState::Starting);

            THEN("server creates as many workers as given worker count")
            {
                const auto expectedWorkerCount = (workerCountToTry > 0 && workerCountToTry <= QThread::idealThreadCount()) ? workerCountToTry : QThread::idealThreadCount();
                REQUIRE(TestServerWorker::createdWorkersCount() == expectedWorkerCount);
                REQUIRE(TestServerWorker::startCallCount() == expectedWorkerCount);
                REQUIRE(TestServerWorker::startData().size() == expectedWorkerCount);

                AND_THEN("server passes given data when starting workers")
                {
                    for (const auto &givenStartData : TestServerWorker::startData())
                    {
                        REQUIRE(givenStartData.metaType() == data.metaType());
                        REQUIRE(givenStartData == data);
                    }
                }
            }
        }
    }
}


SCENARIO("Server emits started after all workers start")
{
    GIVEN("a server")
    {
        REQUIRE(TestServerWorker::createdWorkersCount() == 0);
        TestServerWorker::clear();
        Server server(std::shared_ptr<ServerWorkerFactory>(new TestServerWorkerFactory));
        bool emittedStarted = false;
        QObject::connect(&server, &Server::started, [&emittedStarted](){emittedStarted = true;});
        QObject::connect(&server, &Server::stopped, [](){FAIL("This code is supposed to be unreachable");});
        QObject::connect(&server, &Server::failed, [](){FAIL("This code is supposed to be unreachable");});
        const auto workerCountToTry = GENERATE(AS(int), 0, 1, 3, 5);
        server.setWorkerCount(workerCountToTry);

        WHEN("server is started")
        {
            REQUIRE(server.state() == ExecutionState::Stopped);
            server.start(QVariant{});
            REQUIRE(server.state() == ExecutionState::Starting);

            THEN("server emits started after all workers start")
            {
                const auto expectedWorkerCount = (workerCountToTry > 0 && workerCountToTry <= QThread::idealThreadCount()) ? workerCountToTry : QThread::idealThreadCount();
                REQUIRE(TestServerWorker::createdWorkers().size() == expectedWorkerCount);
                REQUIRE(TestServerWorker::startCallCount() == expectedWorkerCount);
                REQUIRE(TestServerWorker::startData().size() == expectedWorkerCount);
                size_t counter = 0;
                bool isLastWorker = false;
                for (auto *pWorker : TestServerWorker::createdWorkers())
                {
                    REQUIRE(!isLastWorker);
                    REQUIRE(server.state() == ExecutionState::Starting);
                    isLastWorker = (++counter == TestServerWorker::createdWorkers().size());
                    REQUIRE(!emittedStarted);
                    pWorker->setStarted();
                    QCoreApplication::sendPostedEvents();
                    REQUIRE(isLastWorker == emittedStarted);
                }
                REQUIRE(isLastWorker);
                REQUIRE(server.state() == ExecutionState::Started);
            }
        }
    }
}


SCENARIO("Server emits stopped after all workers stop")
{
    GIVEN("a started server")
    {
        REQUIRE(TestServerWorker::createdWorkersCount() == 0);
        TestServerWorker::clear();
        Server server(std::shared_ptr<ServerWorkerFactory>(new TestServerWorkerFactory));
        bool emittedStarted = false;
        QObject::connect(&server, &Server::started, [&emittedStarted](){emittedStarted = true;});
        bool emittedStopped = false;
        QObject::connect(&server, &Server::stopped, [&emittedStopped](){emittedStopped = true;});
        QObject::connect(&server, &Server::failed, [](){FAIL("This code is supposed to be unreachable");});
        const auto workerCountToTry = GENERATE(AS(int), 0, 1, 3, 5);
        server.setWorkerCount(workerCountToTry);
        REQUIRE(server.state() == ExecutionState::Stopped);
        server.start(QVariant{});
        REQUIRE(server.state() == ExecutionState::Starting);
        const auto expectedWorkerCount = (workerCountToTry > 0 && workerCountToTry <= QThread::idealThreadCount()) ? workerCountToTry : QThread::idealThreadCount();
        REQUIRE(TestServerWorker::createdWorkers().size() == expectedWorkerCount);
        REQUIRE(TestServerWorker::startCallCount() == expectedWorkerCount);
        REQUIRE(TestServerWorker::startData().size() == expectedWorkerCount);
        size_t counter = 0;
        bool isLastWorker = false;
        for (auto *pWorker : TestServerWorker::createdWorkers())
        {
            REQUIRE(!isLastWorker);
            REQUIRE(server.state() == ExecutionState::Starting);
            isLastWorker = (++counter == TestServerWorker::createdWorkers().size());
            REQUIRE(!emittedStarted);
            pWorker->setStarted();
            QCoreApplication::sendPostedEvents();
            REQUIRE(isLastWorker == emittedStarted);
        }
        REQUIRE(isLastWorker);

        WHEN("server is stopped")
        {
            REQUIRE(server.state() == ExecutionState::Started);
            REQUIRE(!emittedStopped);
            REQUIRE(TestServerWorker::stopCallCount() == 0);
            server.stop();
            REQUIRE(server.state() == ExecutionState::Stopping);
            REQUIRE(TestServerWorker::stopCallCount() == expectedWorkerCount);
            REQUIRE(!emittedStopped);

            THEN("server emits stopped after all workers stop")
            {
                size_t counter = 0;
                bool isLastWorker = false;
                auto createdWorkers = TestServerWorker::createdWorkers();
                for (auto *pWorker : createdWorkers)
                {
                    REQUIRE(!isLastWorker);
                    REQUIRE(server.state() == ExecutionState::Stopping);
                    isLastWorker = (++counter == TestServerWorker::createdWorkers().size());
                    REQUIRE(!emittedStopped);
                    pWorker->setStopped();
                    QCoreApplication::sendPostedEvents();
                    REQUIRE(isLastWorker == emittedStopped);
                }
                REQUIRE(isLastWorker);
                REQUIRE(server.state() == ExecutionState::Stopped);
            }
        }
    }
}


SCENARIO("Starting server can be stopped")
{
    GIVEN("a server")
    {
        REQUIRE(TestServerWorker::createdWorkersCount() == 0);
        TestServerWorker::clear();
        Server server(std::shared_ptr<ServerWorkerFactory>(new TestServerWorkerFactory));
        bool emittedStarted = false;
        QObject::connect(&server, &Server::started, [&emittedStarted](){emittedStarted = true;});
        bool emittedStopped = false;
        QObject::connect(&server, &Server::stopped, [&emittedStopped](){emittedStopped = true;});
        bool emittedFailed = false;
        std::string emittedErrorMessage;
        QObject::connect(&server, &Server::failed, [&emittedFailed, &emittedErrorMessage](std::string_view errorMessage)
        {
            emittedFailed = true;
            emittedErrorMessage = errorMessage;
        });
        const auto workerCountToTry = GENERATE(AS(int), 0, 1, 3, 5);
        server.setWorkerCount(workerCountToTry);

        WHEN("server is started")
        {
            REQUIRE(server.state() == ExecutionState::Stopped);
            server.start(QVariant{});
            REQUIRE(server.state() == ExecutionState::Starting);

            THEN("server starts all of its workers which enter Starting state")
            {
                const auto expectedWorkerCount = (workerCountToTry > 0 && workerCountToTry <= QThread::idealThreadCount()) ? workerCountToTry : QThread::idealThreadCount();
                REQUIRE(TestServerWorker::createdWorkers().size() == expectedWorkerCount);
                REQUIRE(TestServerWorker::startCallCount() == expectedWorkerCount);
                REQUIRE(TestServerWorker::startData().size() == expectedWorkerCount);
                for (auto *pWorker : TestServerWorker::createdWorkers())
                {
                    REQUIRE(pWorker->state() == ExecutionState::Starting);
                }

                AND_WHEN("server is requested to stop while it is starting")
                {
                    server.stop();

                    THEN("server awaits its workers to start before stopping them")
                    {
                        size_t counter = 0;
                        bool isLastWorker = false;
                        for (auto *pWorker : TestServerWorker::createdWorkers())
                        {
                            REQUIRE(!isLastWorker);
                            REQUIRE(server.state() == ExecutionState::Starting);
                            isLastWorker = (++counter == TestServerWorker::createdWorkers().size());
                            REQUIRE(!emittedStarted);
                            pWorker->setStarted();
                            QCoreApplication::sendPostedEvents();
                            REQUIRE(!emittedStarted);
                        }
                        REQUIRE(isLastWorker);
                        REQUIRE(server.state() == ExecutionState::Stopping);

                        AND_THEN("server emits stopped after all workers stop")
                        {
                            size_t counter = 0;
                            bool isLastWorker = false;
                            auto createdWorkers = TestServerWorker::createdWorkers();
                            for (auto *pWorker : createdWorkers)
                            {
                                REQUIRE(!isLastWorker);
                                REQUIRE(server.state() == ExecutionState::Stopping);
                                isLastWorker = (++counter == TestServerWorker::createdWorkers().size());
                                REQUIRE(!emittedStopped);
                                pWorker->setStopped();
                                QCoreApplication::sendPostedEvents();
                                REQUIRE(isLastWorker == emittedStopped);
                            }
                            REQUIRE(isLastWorker);
                            REQUIRE(server.state() == ExecutionState::Stopped);
                        }
                    }

                    AND_WHEN("one of the workers fail to start")
                    {
                        size_t counter = 0;
                        bool isLastWorker = false;
                        bool hasFailed = false;
                        std::string_view errorMessage("This is the error message that will be sent by the worker.");
                        for (auto *pWorker : TestServerWorker::createdWorkers())
                        {
                            REQUIRE(!isLastWorker);
                            REQUIRE(server.state() == ExecutionState::Starting);
                            isLastWorker = (++counter == TestServerWorker::createdWorkers().size());
                            REQUIRE(!emittedStarted);
                            if (!hasFailed)
                            {
                                hasFailed = true;
                                pWorker->setFailed(errorMessage);
                            }
                            else
                                pWorker->setStarted();
                            QCoreApplication::sendPostedEvents();
                            REQUIRE(!emittedStarted);
                        }
                        REQUIRE(isLastWorker);
                        REQUIRE(server.state() == ((expectedWorkerCount > 1) ? ExecutionState::Stopping : ExecutionState::Stopped));

                        THEN("server emits failed after all workers stop")
                        {
                            size_t counter = 0;
                            bool isLastWorker = TestServerWorker::createdWorkers().empty();
                            auto createdWorkers = TestServerWorker::createdWorkers();
                            for (auto *pWorker : createdWorkers)
                            {
                                REQUIRE(!isLastWorker);
                                isLastWorker = (++counter == TestServerWorker::createdWorkers().size());
                                if (pWorker->state() != ExecutionState::Stopping)
                                    continue;
                                REQUIRE(!emittedStopped);
                                REQUIRE(!emittedFailed);
                                pWorker->setStopped();
                                QCoreApplication::sendPostedEvents();
                                REQUIRE(!emittedStopped);
                                REQUIRE(isLastWorker == emittedFailed);
                            }
                            REQUIRE(isLastWorker);
                            REQUIRE(!emittedStopped);
                            REQUIRE(emittedFailed);
                            REQUIRE(emittedErrorMessage == errorMessage);
                            REQUIRE(server.state() == ExecutionState::Stopped);
                        }
                    }
                }
            }
        }
    }
}


SCENARIO("Starting server stops all workers if any one of them fails to start")
{
    GIVEN("a server")
    {
        REQUIRE(TestServerWorker::createdWorkersCount() == 0);
        TestServerWorker::clear();
        Server server(std::shared_ptr<ServerWorkerFactory>(new TestServerWorkerFactory));
        bool emittedStarted = false;
        QObject::connect(&server, &Server::started, [&emittedStarted](){emittedStarted = true;});
        bool emittedStopped = false;
        QObject::connect(&server, &Server::stopped, [&emittedStopped](){emittedStopped = true;});
        bool emittedFailed = false;
        std::string emittedErrorMessage;
        QObject::connect(&server, &Server::failed, [&emittedFailed, &emittedErrorMessage](std::string_view errorMessage)
        {
            emittedFailed = true;
            emittedErrorMessage = errorMessage;
        });
        const auto workerCountToTry = GENERATE(AS(int), 0, 1, 3, 5);
        server.setWorkerCount(workerCountToTry);

        WHEN("server is started")
        {
            REQUIRE(server.state() == ExecutionState::Stopped);
            server.start(QVariant{});
            REQUIRE(server.state() == ExecutionState::Starting);

            AND_WHEN("at least one worker fails to start")
            {
                const auto expectedWorkerCount = (workerCountToTry > 0 && workerCountToTry <= QThread::idealThreadCount()) ? workerCountToTry : QThread::idealThreadCount();
                REQUIRE(TestServerWorker::createdWorkers().size() == expectedWorkerCount);
                REQUIRE(TestServerWorker::startCallCount() == expectedWorkerCount);
                REQUIRE(TestServerWorker::startData().size() == expectedWorkerCount);
                size_t counter = 0;
                bool isLastWorker = false;
                const auto failAllWorkers = GENERATE(AS(bool), true, false);
                bool hasFailed = false;
                std::string_view errorMessage("Error message");
                const auto createdWorkers = TestServerWorker::createdWorkers();
                for (auto *pWorker : createdWorkers)
                {
                    REQUIRE(!isLastWorker);
                    isLastWorker = (++counter == TestServerWorker::createdWorkers().size());
                    REQUIRE(server.state() == ExecutionState::Starting);
                    REQUIRE(!emittedStarted);
                    if (failAllWorkers)
                        pWorker->setFailed(errorMessage);
                    else
                    {
                        if (!hasFailed)
                        {
                            hasFailed = true;
                            pWorker->setFailed(errorMessage);
                        }
                        else
                            pWorker->setStarted();
                    }
                    QCoreApplication::sendPostedEvents();
                    REQUIRE(!emittedStarted);
                }
                REQUIRE(isLastWorker);
                REQUIRE(server.state() == ((expectedWorkerCount > 1 && !failAllWorkers) ? ExecutionState::Stopping : ExecutionState::Stopped));

                THEN("server requests all workers to stop and emit stopped after they stop")
                {
                    size_t counter = 0;
                    bool isLastWorker = TestServerWorker::createdWorkers().empty();
                    auto createdWorkers = TestServerWorker::createdWorkers();
                    for (auto *pWorker : createdWorkers)
                    {
                        REQUIRE(!isLastWorker);
                        isLastWorker = (++counter == TestServerWorker::createdWorkers().size());
                        if (pWorker->state() != ExecutionState::Stopping)
                            continue;
                        REQUIRE(!emittedStopped);
                        REQUIRE(!emittedFailed);
                        pWorker->setStopped();
                        QCoreApplication::sendPostedEvents();
                        REQUIRE(!emittedStopped);
                        REQUIRE(isLastWorker == emittedFailed);
                    }
                    REQUIRE(isLastWorker);
                    REQUIRE(!emittedStopped);
                    REQUIRE(emittedFailed);
                    REQUIRE(emittedErrorMessage == errorMessage);
                    REQUIRE(server.state() == ExecutionState::Stopped);
                }
            }
        }
    }
}

#include "Server.spec.moc"
