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

#include "AsyncServerWorker.h"
#include <QCoreApplication>
#include <QSemaphore>
#include <Spectator.h>

using Kourier::ExecutionState;
using Kourier::ServerWorker;
using Kourier::AsyncServerWorker;
using Spectator::SemaphoreAwaiter;


namespace Tests::AsyncServerWorker::Spec
{
class TestAsyncServerWorker : public ServerWorker
{
Q_OBJECT
public:
    TestAsyncServerWorker()
    {
        REQUIRE(m_pCreatedInstance == nullptr);
        m_pCreatedInstance = this;
        m_createdSemaphore.release(1);
    }
    TestAsyncServerWorker(std::string arg)
    {
        REQUIRE(m_pCreatedInstance == nullptr);
        m_pCreatedInstance = this;
        m_arg = arg;
        m_createdSemaphore.release(1);
    }
    ~TestAsyncServerWorker() override
    {
        REQUIRE(m_pCreatedInstance == this);
        m_pCreatedInstance = nullptr;
        m_destroyedSemaphore.release(1);
    }
    ExecutionState state() const override {return m_state;}

public slots:
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

public:
    static TestAsyncServerWorker *instance() {return m_pCreatedInstance;}
    static std::string arg() {return m_arg;}
    static QVariant &startData() {return m_startData;}
    static int startCallCount() {return m_startCallCount;}
    static int stopCallCount() {return m_stopCallCount;}
    static void clear()
    {
        m_startData = {};
        m_arg.clear();
        m_startCallCount = 0;
        m_stopCallCount = 0;
        while (m_createdSemaphore.tryAcquire(1));
        while (m_destroyedSemaphore.tryAcquire(1));
        while (m_startCalledSemaphore.tryAcquire(1));
        while (m_stopCalledSemaphore.tryAcquire(1));
    }
    static bool waitForCreation() {return SemaphoreAwaiter::signalSlotAwareWait(m_createdSemaphore, 1);}

private:
    void doStart(QVariant data) override
    {
        REQUIRE(m_state == ExecutionState::Stopped);
        m_state = ExecutionState::Starting;
        m_startData = data;
        ++m_startCallCount;
    }
    void doStop() override
    {
        REQUIRE(m_state == ExecutionState::Started || m_state == ExecutionState::Starting);
        m_state = ExecutionState::Stopping;
        ++m_stopCallCount;
    }

private:
    ExecutionState m_state = ExecutionState::Stopped;
    static TestAsyncServerWorker *m_pCreatedInstance;
    static QVariant m_startData;
    static std::string m_arg;
    static int m_startCallCount;
    static int m_stopCallCount;
    static QSemaphore m_createdSemaphore;
    static QSemaphore m_destroyedSemaphore;
    static QSemaphore m_startCalledSemaphore;
    static QSemaphore m_stopCalledSemaphore;
};

TestAsyncServerWorker *TestAsyncServerWorker::m_pCreatedInstance = nullptr;
QVariant TestAsyncServerWorker::m_startData;
std::string TestAsyncServerWorker::m_arg;
int TestAsyncServerWorker::m_startCallCount = 0;
int TestAsyncServerWorker::m_stopCallCount = 0;
QSemaphore TestAsyncServerWorker::m_createdSemaphore;
QSemaphore TestAsyncServerWorker::m_destroyedSemaphore;
QSemaphore TestAsyncServerWorker::m_startCalledSemaphore;
QSemaphore TestAsyncServerWorker::m_stopCalledSemaphore;

}

using namespace Tests::AsyncServerWorker::Spec;


SCENARIO("AsyncServerWorker is created in Stopped state")
{
    GIVEN("a default-constructed async server worker")
    {
        REQUIRE(TestAsyncServerWorker::instance() == nullptr);
        TestAsyncServerWorker::clear();
        AsyncServerWorker<TestAsyncServerWorker> worker;

        WHEN("worker state is fetched")
        {
            const auto workerState = worker.state();

            THEN("worker is in Stopped state")
            {
                REQUIRE(workerState == ExecutionState::Stopped);
                REQUIRE(TestAsyncServerWorker::arg().empty());
            }
        }
    }
}


SCENARIO("AsyncServerWorker can be created for non-default constructible workers")
{
    GIVEN("an async server worker constructed with init data")
    {
        REQUIRE(TestAsyncServerWorker::instance() == nullptr);
        TestAsyncServerWorker::clear();
        const std::string initData("This is the init data");
        AsyncServerWorker<TestAsyncServerWorker, std::string> worker(initData);

        WHEN("worker state is fetched")
        {
            const auto workerState = worker.state();

            THEN("worker is in Stopped state")
            {
                REQUIRE(workerState == ExecutionState::Stopped);
                REQUIRE(TestAsyncServerWorker::arg() == initData);
            }
        }
    }
}


SCENARIO("AsyncServerWorker creates async worker on another thread")
{
    GIVEN("a started async server worker")
    {
        REQUIRE(TestAsyncServerWorker::instance() == nullptr);
        TestAsyncServerWorker::clear();
        AsyncServerWorker<TestAsyncServerWorker> worker;
        REQUIRE(worker.state() == ExecutionState::Stopped);
        worker.start(QVariant{});
        REQUIRE(worker.state() == ExecutionState::Starting);
        REQUIRE(TestAsyncServerWorker::waitForCreation());

        WHEN("thread of the async server worker instance is fetched")
        {
            const auto *pThread = TestAsyncServerWorker::instance()->thread();
            REQUIRE(pThread != nullptr);

            THEN("fetched thread is different from current thread")
            {
                REQUIRE(QThread::currentThread() != pThread);
            }
        }
    }
}


SCENARIO("AsyncServerWorker emits started after async worker starts")
{
    GIVEN("an async server worker")
    {
        REQUIRE(TestAsyncServerWorker::instance() == nullptr);
        TestAsyncServerWorker::clear();
        AsyncServerWorker<TestAsyncServerWorker> worker;
        bool emittedStarted = false;
        QObject::connect(&worker, &ServerWorker::started, [&emittedStarted](){emittedStarted = true;});
        QObject::connect(&worker, &ServerWorker::stopped, [](){FAIL("This code is supposed to be unreachable");});
        QObject::connect(&worker, &ServerWorker::failed, [](){FAIL("This code is supposed to be unreachable");});

        WHEN("worker is started")
        {
            REQUIRE(worker.state() == ExecutionState::Stopped);
            const auto data = GENERATE(AS(QVariant), QVariant(int(145)), QVariant(QByteArray("Some data to use while starting...")));
            worker.start(data);
            REQUIRE(worker.state() == ExecutionState::Starting);

            THEN("async server worker emits started after async worker starts")
            {
                REQUIRE(TestAsyncServerWorker::waitForCreation());
                REQUIRE(!emittedStarted);
                REQUIRE(QMetaObject::invokeMethod(TestAsyncServerWorker::instance(), "setStarted", Qt::QueuedConnection));
                QCoreApplication::sendPostedEvents();
                do
                {
                    QCoreApplication::processEvents();
                }
                while (!emittedStarted);
                REQUIRE(worker.state() == ExecutionState::Started);
            }
        }
    }
}


SCENARIO("AsyncServerWorker emits stopped after async worker stops")
{
    GIVEN("a started async server worker")
    {
        REQUIRE(TestAsyncServerWorker::instance() == nullptr);
        TestAsyncServerWorker::clear();
        AsyncServerWorker<TestAsyncServerWorker> worker;
        bool emittedStarted = false;
        QObject::connect(&worker, &ServerWorker::started, [&emittedStarted](){emittedStarted = true;});
        bool emittedStopped = false;
        QObject::connect(&worker, &ServerWorker::stopped, [&emittedStopped](){emittedStopped = true;});
        QObject::connect(&worker, &ServerWorker::failed, [](){FAIL("This code is supposed to be unreachable");});
        REQUIRE(worker.state() == ExecutionState::Stopped);
        const auto data = GENERATE(AS(QVariant), QVariant(int(145)), QVariant(QByteArray("Some data to use while starting...")));
        worker.start(data);
        REQUIRE(worker.state() == ExecutionState::Starting);
        REQUIRE(TestAsyncServerWorker::waitForCreation());
        REQUIRE(!emittedStarted);
        REQUIRE(QMetaObject::invokeMethod(TestAsyncServerWorker::instance(), "setStarted", Qt::QueuedConnection));
        QCoreApplication::sendPostedEvents();
        do
        {
            QCoreApplication::processEvents();
        }
        while (!emittedStarted);
        REQUIRE(worker.state() == ExecutionState::Started);

        WHEN("worker is stopped")
        {
            worker.stop();
            REQUIRE(worker.state() == ExecutionState::Stopping);

            THEN("async server worker emits stopped after async worker stops")
            {
                REQUIRE(!emittedStopped);
                REQUIRE(QMetaObject::invokeMethod(TestAsyncServerWorker::instance(), "setStopped", Qt::QueuedConnection));
                QCoreApplication::sendPostedEvents();
                do
                {
                    QCoreApplication::processEvents();
                }
                while (!emittedStopped);
                REQUIRE(worker.state() == ExecutionState::Stopped);
            }
        }
    }
}


SCENARIO("AsyncServerWorker emits failed if async server worker fails")
{
    GIVEN("an async server worker")
    {
        REQUIRE(TestAsyncServerWorker::instance() == nullptr);
        TestAsyncServerWorker::clear();
        AsyncServerWorker<TestAsyncServerWorker> worker;
        QObject::connect(&worker, &ServerWorker::started, [](){FAIL("This code is supposed to be unreachable");});
        QObject::connect(&worker, &ServerWorker::stopped, [](){FAIL("This code is supposed to be unreachable");});
        bool emittedFailed = false;
        std::string emittedErrorMessage;
        QObject::connect(&worker, &ServerWorker::failed, [&emittedFailed, &emittedErrorMessage](std::string_view errorMessage)
        {
            emittedFailed = true;
            emittedErrorMessage  = errorMessage;
        });

        WHEN("worker is started")
        {
            REQUIRE(worker.state() == ExecutionState::Stopped);
            const auto data = GENERATE(AS(QVariant), QVariant(int(145)), QVariant(QByteArray("Some data to use while starting...")));
            worker.start(data);
            REQUIRE(worker.state() == ExecutionState::Starting);

            THEN("async server worker emits failed if async worker fails")
            {
                REQUIRE(TestAsyncServerWorker::waitForCreation());
                REQUIRE(!emittedFailed);
                std::string_view errorMessage("This is the error message that will be emitted.");
                REQUIRE(QMetaObject::invokeMethod(TestAsyncServerWorker::instance(), "setFailed", Qt::QueuedConnection, errorMessage));
                QCoreApplication::sendPostedEvents();
                do
                {
                    QCoreApplication::processEvents();
                }
                while (!emittedFailed);
                REQUIRE(errorMessage == emittedErrorMessage);
                REQUIRE(worker.state() == ExecutionState::Stopped);
            }
        }
    }
}


SCENARIO("Starting AsyncServerWorker can be stopped")
{
    GIVEN("an async server worker")
    {
        REQUIRE(TestAsyncServerWorker::instance() == nullptr);
        TestAsyncServerWorker::clear();
        AsyncServerWorker<TestAsyncServerWorker> worker;
        bool emittedStarted = false;
        QObject::connect(&worker, &ServerWorker::started, [&emittedStarted](){emittedStarted = true;});
        bool emittedStopped = false;
        QObject::connect(&worker, &ServerWorker::stopped, [&emittedStopped](){emittedStopped = true;});
        bool emittedFailed = false;
        std::string emittedErrorMessage;
        QObject::connect(&worker, &ServerWorker::failed, [&emittedFailed, &emittedErrorMessage](std::string_view errorMessage)
        {
            emittedFailed = true;
            emittedErrorMessage  = errorMessage;
        });

        WHEN("worker is started")
        {
            REQUIRE(worker.state() == ExecutionState::Stopped);
            const auto data = GENERATE(AS(QVariant), QVariant(int(145)), QVariant(QByteArray("Some data to use while starting...")));
            worker.start(data);
            REQUIRE(worker.state() == ExecutionState::Starting);

            THEN("worker starts its async workers which enter Starting state")
            {
                REQUIRE(TestAsyncServerWorker::waitForCreation());
                do
                {
                    QCoreApplication::sendPostedEvents();
                }
                while (TestAsyncServerWorker::instance()->state() != ExecutionState::Starting);

                AND_WHEN("worker is requested to stop while it is starting")
                {
                    worker.stop();

                    THEN("worker awaits its worker to start before stopping it")
                    {

                        REQUIRE(worker.state() == ExecutionState::Starting);
                        REQUIRE(!emittedStarted);
                        REQUIRE(QMetaObject::invokeMethod(TestAsyncServerWorker::instance(), "setStarted", Qt::QueuedConnection));
                        do
                        {
                            QCoreApplication::sendPostedEvents();
                        }
                        while (worker.state() != ExecutionState::Stopping);
                        REQUIRE(!emittedStarted);

                        AND_THEN("async server worker emits stopped after its internal async worker stops")
                        {
                            REQUIRE(!emittedStopped);
                            REQUIRE(QMetaObject::invokeMethod(TestAsyncServerWorker::instance(), "setStopped", Qt::QueuedConnection));
                            do
                            {
                                QCoreApplication::sendPostedEvents();
                            }
                            while (worker.state() != ExecutionState::Stopped);
                            REQUIRE(emittedStopped);
                            REQUIRE(!emittedFailed);
                        }
                    }

                    AND_WHEN("internal worker fails to start")
                    {
                        std::string_view errorMessage("This is the error message that will be sent by the worker.");
                        REQUIRE(worker.state() == ExecutionState::Starting);
                        REQUIRE(!emittedStarted);
                        REQUIRE(QMetaObject::invokeMethod(TestAsyncServerWorker::instance(), "setFailed", Qt::QueuedConnection, errorMessage));

                        THEN("server emits failed")
                        {
                            do
                            {
                                QCoreApplication::sendPostedEvents();
                            }
                            while (worker.state() != ExecutionState::Stopped);
                            REQUIRE(!emittedStarted);
                            REQUIRE(!emittedStopped);
                            REQUIRE(emittedFailed);
                            REQUIRE(emittedErrorMessage == errorMessage);
                        }
                    }
                }
            }
        }
    }
}

#include "AsyncServerWorker.spec.moc"
