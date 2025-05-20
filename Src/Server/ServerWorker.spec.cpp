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

#include "ServerWorker.h"
#include "ConnectionListener.h"
#include "ConnectionHandler.h"
#include "ConnectionHandlerFactory.h"
#include "ConnectionHandlerRepository.h"
#include <Spectator.h>
#include <set>
#include <sys/socket.h>

using Kourier::ServerWorker;
using Kourier::ConnectionListener;
using Kourier::ConnectionHandler;
using Kourier::ConnectionHandlerFactory;
using Kourier::ConnectionHandlerRepository;

namespace Test::ServerWorker::Spec
{

class TestConnectionListener : public ConnectionListener
{
KOURIER_OBJECT(Test::ServerWorker::Spec::TestConnectionListener)
public:
    TestConnectionListener() = default;
    ~TestConnectionListener() override = default;
    bool start(QVariant data) override {m_data = data; return m_errorMessage.empty();}
    std::string_view errorMessage() const override {return m_errorMessage;}
    int backlogSize() const override {FAIL("This method is supposed to be unreachable.");}
    qintptr socketDescriptor() const override {FAIL("This method is supposed to be unreachable.");}
    QVariant data() const {return m_data;}
    void setToFail(std::string_view errorMessage) {REQUIRE(!errorMessage.empty()); m_errorMessage = errorMessage;}

private:
    QVariant m_data;
    std::string m_errorMessage;
};

class TestConnectionHandler : public ConnectionHandler
{
KOURIER_OBJECT(Test::ServerWorker::Spec::TestConnectionHandler)
public:
    TestConnectionHandler(qintptr socketDescriptor) : m_socketDescriptor(socketDescriptor) {}
    ~TestConnectionHandler() override = default;
    void finish() override {finished(this);}
    qintptr socketDescriptor() const {return m_socketDescriptor;}

private:
    const qintptr m_socketDescriptor;
};

class TestConnectionHandlerFactory : public ConnectionHandlerFactory
{
public:
    TestConnectionHandlerFactory() = default;
    ~TestConnectionHandlerFactory() override = default;
    ConnectionHandler *create(qintptr socketDescriptor) override
    {
        if (m_setToFail)
            return nullptr;
        else
        {
            auto *pHandler = new TestConnectionHandler(socketDescriptor);
            m_createdHandlers.insert(pHandler);
            return pHandler;
        }
    }
    void setToFail() {m_setToFail = true;}
    const std::set<TestConnectionHandler*> &createdHandlers() const {return m_createdHandlers;}

private:
    bool m_setToFail = false;
    std::set<TestConnectionHandler*> m_createdHandlers;
};

}

using namespace Test::ServerWorker::Spec;


SCENARIO("ServerWorker makes ConnectionListener listen for incoming connections during start")
{
    GIVEN("a ServerWorker")
    {
        auto pListener = std::make_shared<TestConnectionListener>();
        ServerWorker serverWorker(pListener,
                                  std::make_shared<TestConnectionHandlerFactory>(),
                                  std::make_shared<ConnectionHandlerRepository>());
        bool emittedStarted = false;
        QObject::connect(&serverWorker, &ServerWorker::started, [&emittedStarted](){emittedStarted = true;});
        QObject::connect(&serverWorker, &ServerWorker::stopped, [](){FAIL("This code is supposed to be unreachable.");});
        QObject::connect(&serverWorker, &ServerWorker::failed, [](){FAIL("This code is supposed to be unreachable.");});

        WHEN("ServerWorker is started with some data")
        {
            QVariantMap variantMap;
            auto pCounter = std::make_shared<std::atomic_size_t>();
            variantMap["connectionCount"].setValue(pCounter);
            QVariant data(variantMap);
            REQUIRE(!emittedStarted);
            serverWorker.start(data);

            THEN("ServerWorker emits started if connection listener successfully starts with given data")
            {
                REQUIRE(emittedStarted);
                REQUIRE(pListener->data() == data);
            }
        }
    }
}


SCENARIO("ServerWorker fails to start if ConnectionListener fails to listen for incoming connections")
{
    GIVEN("a ServerWorker")
    {
        auto pListener = std::make_shared<TestConnectionListener>();
        std::string_view errorMessage("This is the error message that connection listener will emit in failed");
        pListener->setToFail(errorMessage);
        ServerWorker serverWorker(pListener,
                                  std::make_shared<TestConnectionHandlerFactory>(),
                                  std::make_shared<ConnectionHandlerRepository>());
        QObject::connect(&serverWorker, &ServerWorker::started, [](){FAIL("This code is supposed to be unreachable.");});
        QObject::connect(&serverWorker, &ServerWorker::stopped, [](){FAIL("This code is supposed to be unreachable.");});
        bool emittedFailed = false;
        std::string emittedErrorMessage;
        QObject::connect(&serverWorker, &ServerWorker::failed, [&emittedFailed, &emittedErrorMessage](std::string_view errorMessage){emittedFailed = true; emittedErrorMessage = errorMessage;});

        WHEN("ServerWorker is started with some data")
        {
            QVariantMap variantMap;
            auto pCounter = std::make_shared<std::atomic_size_t>();
            variantMap["connectionCount"].setValue(pCounter);
            QVariant data(variantMap);
            REQUIRE(!emittedFailed);
            serverWorker.start(data);

            THEN("ServerWorker emits failed if connection listener fails to start with given data")
            {
                REQUIRE(emittedFailed);
                REQUIRE(emittedErrorMessage == errorMessage);
                REQUIRE(pListener->data() == data);
            }
        }
    }
}


SCENARIO("ServerWorker creates handler and adds it to repository whenever a new connection gets established")
{
    GIVEN("a started ServerWorker")
    {
        auto pListener = std::make_shared<TestConnectionListener>();
        auto pHandlerFactory = std::make_shared<TestConnectionHandlerFactory>();
        auto pHandlerRepository = std::make_shared<ConnectionHandlerRepository>();
        ServerWorker serverWorker(pListener,
                                  pHandlerFactory,
                                  pHandlerRepository);
        bool emittedStarted = false;
        QObject::connect(&serverWorker, &ServerWorker::started, [&emittedStarted](){emittedStarted = true;});
        QObject::connect(&serverWorker, &ServerWorker::stopped, [](){FAIL("This code is supposed to be unreachable.");});
        QObject::connect(&serverWorker, &ServerWorker::failed, [](){FAIL("This code is supposed to be unreachable.");});
        QVariantMap variantMap;
        auto pCounter = std::make_shared<std::atomic_size_t>();
        variantMap["connectionCount"].setValue(pCounter);
        QVariant data(variantMap);
        REQUIRE(!emittedStarted);
        serverWorker.start(data);
        REQUIRE(emittedStarted);
        REQUIRE(pListener->data() == data);

        WHEN("connection listener reports a new connection")
        {
            REQUIRE(pHandlerFactory->createdHandlers().empty());
            REQUIRE(pHandlerRepository->handlerCount() == 0);
            const auto socketDescriptorsToEmit = GENERATE(AS(std::set<qintptr>), {}, {3008}, {3, 17, 48});
            for (auto socketDescriptor : socketDescriptorsToEmit)
                pListener->newConnection(socketDescriptor);

            THEN("ServerWorker creates a new handle with given socket descriptor and adds it to repository")
            {
                REQUIRE(pHandlerFactory->createdHandlers().size() == socketDescriptorsToEmit.size());
                std::set<qintptr> createdHandlersDescriptors;
                for (auto *pCreatedHandler : pHandlerFactory->createdHandlers())
                    createdHandlersDescriptors.insert(pCreatedHandler->socketDescriptor());
                REQUIRE(createdHandlersDescriptors == socketDescriptorsToEmit);
                REQUIRE(pHandlerRepository->handlerCount() == socketDescriptorsToEmit.size());
            }
        }
    }
}


SCENARIO("ServerWorker stops by deleting connection listener and stopping repository")
{
    GIVEN("a started ServerWorker")
    {
        auto pListener = std::make_shared<TestConnectionListener>();
        auto pHandlerFactory = std::make_shared<TestConnectionHandlerFactory>();
        auto pHandlerRepository = std::make_shared<ConnectionHandlerRepository>();
        ServerWorker serverWorker(pListener,
                                  pHandlerFactory,
                                  pHandlerRepository);
        bool emittedStarted = false;
        QObject::connect(&serverWorker, &ServerWorker::started, [&emittedStarted](){emittedStarted = true;});
        bool emittedStopped = false;
        QObject::connect(&serverWorker, &ServerWorker::stopped, [&emittedStopped](){emittedStopped = true;});
        QObject::connect(&serverWorker, &ServerWorker::failed, [](){FAIL("This code is supposed to be unreachable.");});
        QVariantMap variantMap;
        auto pCounter = std::make_shared<std::atomic_size_t>();
        variantMap["connectionCount"].setValue(pCounter);
        QVariant data(variantMap);
        REQUIRE(!emittedStarted);
        REQUIRE(!emittedStopped);
        serverWorker.start(data);
        REQUIRE(emittedStarted);
        REQUIRE(!emittedStopped);
        REQUIRE(pListener->data() == data);

        WHEN("ServerWorker is stopped")
        {
            const auto pListenerUseCount = pListener.use_count();
            serverWorker.stop();

            THEN("ServerWorker deletes connection listener and stops repository")
            {
                REQUIRE((pListenerUseCount - 1) == pListener.use_count());

                AND_THEN("repository emits stopped and ServerWorker emits stopped")
                {
                    REQUIRE(emittedStopped);
                }
            }
        }
    }
}


SCENARIO("ServerWorker supports limiting max connections")
{
    GIVEN("a ServerWorker with limit on maximum connections")
    {
        auto pListener = std::make_shared<TestConnectionListener>();
        auto pHandlerFactory = std::make_shared<TestConnectionHandlerFactory>();
        auto pHandlerRepository = std::make_shared<ConnectionHandlerRepository>();
        ServerWorker serverWorker(pListener,
                                  pHandlerFactory,
                                  pHandlerRepository);
        bool emittedStarted = false;
        QObject::connect(&serverWorker, &ServerWorker::started, [&emittedStarted](){emittedStarted = true;});
        QObject::connect(&serverWorker, &ServerWorker::stopped, [](){FAIL("This code is supposed to be unreachable.");});
        QObject::connect(&serverWorker, &ServerWorker::failed, [](){FAIL("This code is supposed to be unreachable.");});
        QVariantMap variantMap;
        auto pCounter = std::make_shared<std::atomic_size_t>();
        variantMap["connectionCount"].setValue(pCounter);
        const auto maxConnectionLimit = GENERATE(AS(size_t), 1, 3, 5);
        variantMap["maxConnectionCount"].setValue(maxConnectionLimit);
        QVariant data(variantMap);
        REQUIRE(!emittedStarted);
        serverWorker.start(data);
        REQUIRE(emittedStarted);
        REQUIRE(pListener->data() == data);

        WHEN("connections up to the limit are established")
        {
            for (auto i = 0; i < maxConnectionLimit; ++i)
                pListener->newConnection(i);

            THEN("ServerWorker creates handlers for established connections")
            {
                REQUIRE(pHandlerRepository->handlerCount() == maxConnectionLimit);

                AND_WHEN("one more connection is established")
                {
                    const auto socketDescriptor = ::socket(AF_INET, SOCK_STREAM, 0);
                    REQUIRE(socketDescriptor >= 0);
                    pListener->newConnection(socketDescriptor);

                    THEN("worker closes the file descriptor and does not create a handler for it")
                    {
                        REQUIRE(::close(socketDescriptor) == -1 && errno == EBADF);
                        REQUIRE(pHandlerRepository->handlerCount() == maxConnectionLimit);
                    }
                }

                AND_WHEN("handlers finish")
                {
                    for (auto *pHandler : pHandlerFactory->createdHandlers())
                        pHandler->finished(pHandler);

                    THEN("repository becomes empty")
                    {
                        REQUIRE(pHandlerRepository->handlerCount() == 0);

                        AND_THEN("new connections up to maxConnectionLimit can be established again")
                        {
                            for (auto i = 0; i < maxConnectionLimit; ++i)
                                pListener->newConnection(i + maxConnectionLimit);
                            REQUIRE(pHandlerRepository->handlerCount() == maxConnectionLimit);

                            AND_WHEN("one more connection is established")
                            {
                                const auto socketDescriptor = ::socket(AF_INET, SOCK_STREAM, 0);
                                REQUIRE(socketDescriptor >= 0);
                                pListener->newConnection(socketDescriptor);

                                THEN("worker closes the file descriptor and does not create a handler for it")
                                {
                                    REQUIRE(::close(socketDescriptor) == -1 && errno == EBADF);
                                    REQUIRE(pHandlerRepository->handlerCount() == maxConnectionLimit);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}


SCENARIO("ServerWorker fails as expected")
{
    GIVEN("a ServerWorker with a null listener")
    {
        auto pListener = std::make_shared<TestConnectionListener>();
        auto pHandlerFactory = std::make_shared<TestConnectionHandlerFactory>();
        auto pHandlerRepository = std::make_shared<ConnectionHandlerRepository>();
        ServerWorker serverWorker(std::shared_ptr<TestConnectionListener>(),
                                  pHandlerFactory,
                                  pHandlerRepository);
        QObject::connect(&serverWorker, &ServerWorker::started, [](){FAIL("This code is supposed to be unreachable.");});
        QObject::connect(&serverWorker, &ServerWorker::stopped, [](){FAIL("This code is supposed to be unreachable.");});
        bool emittedFailed = false;
        std::string emittedErrorMessage;
        QObject::connect(&serverWorker, &ServerWorker::failed, [&emittedFailed, &emittedErrorMessage](std::string_view errorMessage){emittedFailed = true; emittedErrorMessage = errorMessage;});

        WHEN("ServerWorker is started")
        {
            QVariantMap variantMap;
            auto pCounter = std::make_shared<std::atomic_size_t>();
            variantMap["connectionCount"].setValue(pCounter);
            QVariant data(variantMap);
            REQUIRE(!emittedFailed);
            serverWorker.start(data);

            THEN("Server worker fails to start")
            {
                REQUIRE(emittedFailed);
                REQUIRE(emittedErrorMessage == "Failed to start server worker. Given connection listener is null.");
            }
        }
    }

    GIVEN("a ServerWorker with a null handler factory")
    {
        auto pListener = std::make_shared<TestConnectionListener>();
        auto pHandlerFactory = std::make_shared<TestConnectionHandlerFactory>();
        auto pHandlerRepository = std::make_shared<ConnectionHandlerRepository>();
        ServerWorker serverWorker(pListener,
                                  std::shared_ptr<TestConnectionHandlerFactory>(),
                                  pHandlerRepository);
        QObject::connect(&serverWorker, &ServerWorker::started, [](){FAIL("This code is supposed to be unreachable.");});
        QObject::connect(&serverWorker, &ServerWorker::stopped, [](){FAIL("This code is supposed to be unreachable.");});
        bool emittedFailed = false;
        std::string emittedErrorMessage;
        QObject::connect(&serverWorker, &ServerWorker::failed, [&emittedFailed, &emittedErrorMessage](std::string_view errorMessage){emittedFailed = true; emittedErrorMessage = errorMessage;});

        WHEN("ServerWorker is started")
        {
            QVariantMap variantMap;
            auto pCounter = std::make_shared<std::atomic_size_t>();
            variantMap["connectionCount"].setValue(pCounter);
            QVariant data(variantMap);
            REQUIRE(!emittedFailed);
            serverWorker.start(data);

            THEN("Server worker fails to start")
            {
                REQUIRE(emittedFailed);
                REQUIRE(emittedErrorMessage == "Failed to start server worker. Given connection handler factory is null.");
            }
        }
    }

    GIVEN("a ServerWorker with a null handler repository")
    {
        auto pListener = std::make_shared<TestConnectionListener>();
        auto pHandlerFactory = std::make_shared<TestConnectionHandlerFactory>();
        auto pHandlerRepository = std::make_shared<ConnectionHandlerRepository>();
        ServerWorker serverWorker(pListener,
                                  pHandlerFactory,
                                  std::shared_ptr<ConnectionHandlerRepository>());
        QObject::connect(&serverWorker, &ServerWorker::started, [](){FAIL("This code is supposed to be unreachable.");});
        QObject::connect(&serverWorker, &ServerWorker::stopped, [](){FAIL("This code is supposed to be unreachable.");});
        bool emittedFailed = false;
        std::string emittedErrorMessage;
        QObject::connect(&serverWorker, &ServerWorker::failed, [&emittedFailed, &emittedErrorMessage](std::string_view errorMessage){emittedFailed = true; emittedErrorMessage = errorMessage;});

        WHEN("ServerWorker is started")
        {
            QVariantMap variantMap;
            auto pCounter = std::make_shared<std::atomic_size_t>();
            variantMap["connectionCount"].setValue(pCounter);
            QVariant data(variantMap);
            REQUIRE(!emittedFailed);
            serverWorker.start(data);

            THEN("Server worker fails to start")
            {
                REQUIRE(emittedFailed);
                REQUIRE(emittedErrorMessage == "Failed to start server worker. Given connection handler repository is null.");
            }
        }
    }

    GIVEN("a ServerWorker")
    {
        auto pListener = std::make_shared<TestConnectionListener>();
        auto pHandlerFactory = std::make_shared<TestConnectionHandlerFactory>();
        auto pHandlerRepository = std::make_shared<ConnectionHandlerRepository>();
        ServerWorker serverWorker(pListener,
                                  pHandlerFactory,
                                  pHandlerRepository);
        QObject::connect(&serverWorker, &ServerWorker::started, [](){FAIL("This code is supposed to be unreachable.");});
        QObject::connect(&serverWorker, &ServerWorker::stopped, [](){FAIL("This code is supposed to be unreachable.");});
        bool emittedFailed = false;
        std::string emittedErrorMessage;
        QObject::connect(&serverWorker, &ServerWorker::failed, [&emittedFailed, &emittedErrorMessage](std::string_view errorMessage){emittedFailed = true; emittedErrorMessage = errorMessage;});

        WHEN("ServerWorker is started with data that is not a QVariantMap")
        {
            QVariant data(QByteArray("This is not a QVariantMap for sure."));
            REQUIRE(!emittedFailed);
            serverWorker.start(data);

            THEN("Server worker fails to start")
            {
                REQUIRE(emittedFailed);
                REQUIRE(emittedErrorMessage == "Failed to start connection listener. Given data is not a QVariantMap.");
            }
        }
    }

    GIVEN("a ServerWorker")
    {
        auto pListener = std::make_shared<TestConnectionListener>();
        auto pHandlerFactory = std::make_shared<TestConnectionHandlerFactory>();
        auto pHandlerRepository = std::make_shared<ConnectionHandlerRepository>();
        ServerWorker serverWorker(pListener,
                                  pHandlerFactory,
                                  pHandlerRepository);
        QObject::connect(&serverWorker, &ServerWorker::started, [](){FAIL("This code is supposed to be unreachable.");});
        QObject::connect(&serverWorker, &ServerWorker::stopped, [](){FAIL("This code is supposed to be unreachable.");});
        bool emittedFailed = false;
        std::string emittedErrorMessage;
        QObject::connect(&serverWorker, &ServerWorker::failed, [&emittedFailed, &emittedErrorMessage](std::string_view errorMessage){emittedFailed = true; emittedErrorMessage = errorMessage;});

        WHEN("ServerWorker is started with QVariantMap that does not contain connectionCount")
        {
            QVariantMap variantMap;
            QVariant data(variantMap);
            REQUIRE(!emittedFailed);
            serverWorker.start(data);

            THEN("Server worker fails to start")
            {
                REQUIRE(emittedFailed);
                REQUIRE(emittedErrorMessage == "Failed to start connection listener. Variable connectionCount has not been given. This is an internal error, please report a bug.");
            }
        }
    }

    GIVEN("a ServerWorker")
    {
        auto pListener = std::make_shared<TestConnectionListener>();
        auto pHandlerFactory = std::make_shared<TestConnectionHandlerFactory>();
        auto pHandlerRepository = std::make_shared<ConnectionHandlerRepository>();
        ServerWorker serverWorker(pListener,
                                  pHandlerFactory,
                                  pHandlerRepository);
        QObject::connect(&serverWorker, &ServerWorker::started, [](){FAIL("This code is supposed to be unreachable.");});
        QObject::connect(&serverWorker, &ServerWorker::stopped, [](){FAIL("This code is supposed to be unreachable.");});
        bool emittedFailed = false;
        std::string emittedErrorMessage;
        QObject::connect(&serverWorker, &ServerWorker::failed, [&emittedFailed, &emittedErrorMessage](std::string_view errorMessage){emittedFailed = true; emittedErrorMessage = errorMessage;});

        WHEN("ServerWorker is started with QVariantMap containing a connectionCount that is not of type std::shared_ptr<std::atomic_size_t>")
        {
            QVariantMap variantMap;
            variantMap["connectionCount"].setValue(QByteArray("This is not a pointer to void for sure."));
            QVariant data(variantMap);
            REQUIRE(!emittedFailed);
            serverWorker.start(data);

            THEN("Server worker fails to start")
            {
                REQUIRE(emittedFailed);
                REQUIRE(emittedErrorMessage == "Failed to start connection listener. Given connectionCount variable is not of type std::shared_ptr<std::atomic_size_t>. This is an internal error, please report a bug.");
            }
        }
    }

    GIVEN("a ServerWorker")
    {
        auto pListener = std::make_shared<TestConnectionListener>();
        auto pHandlerFactory = std::make_shared<TestConnectionHandlerFactory>();
        auto pHandlerRepository = std::make_shared<ConnectionHandlerRepository>();
        ServerWorker serverWorker(pListener,
                                  pHandlerFactory,
                                  pHandlerRepository);
        QObject::connect(&serverWorker, &ServerWorker::started, [](){FAIL("This code is supposed to be unreachable.");});
        QObject::connect(&serverWorker, &ServerWorker::stopped, [](){FAIL("This code is supposed to be unreachable.");});
        bool emittedFailed = false;
        std::string emittedErrorMessage;
        QObject::connect(&serverWorker, &ServerWorker::failed, [&emittedFailed, &emittedErrorMessage](std::string_view errorMessage){emittedFailed = true; emittedErrorMessage = errorMessage;});

        WHEN("ServerWorker is started with QVariantMap containing a maxConnectionCount that is not of type size_t")
        {
            QVariantMap variantMap;
            auto pCounter = std::make_shared<std::atomic_size_t>();
            variantMap["connectionCount"].setValue(pCounter);
            variantMap["maxConnectionCount"].setValue(QByteArray("This is not of type int for sure."));
            QVariant data(variantMap);
            REQUIRE(!emittedFailed);
            serverWorker.start(data);

            THEN("Server worker fails to start")
            {
                REQUIRE(emittedFailed);
                REQUIRE(emittedErrorMessage == "Failed to start connection listener. Given maxConnectionCount must be of type size_t. This is an internal error, please report a bug.");
            }
        }
    }

    GIVEN("a ServerWorker")
    {
        auto pListener = std::make_shared<TestConnectionListener>();
        auto pHandlerFactory = std::make_shared<TestConnectionHandlerFactory>();
        auto pHandlerRepository = std::make_shared<ConnectionHandlerRepository>();
        ServerWorker serverWorker(pListener,
                                  pHandlerFactory,
                                  pHandlerRepository);
        QObject::connect(&serverWorker, &ServerWorker::started, [](){FAIL("This code is supposed to be unreachable.");});
        QObject::connect(&serverWorker, &ServerWorker::stopped, [](){FAIL("This code is supposed to be unreachable.");});
        bool emittedFailed = false;
        std::string emittedErrorMessage;
        QObject::connect(&serverWorker, &ServerWorker::failed, [&emittedFailed, &emittedErrorMessage](std::string_view errorMessage){emittedFailed = true; emittedErrorMessage = errorMessage;});

        WHEN("ServerWorker is started with QVariantMap containing a maxConnectionCount that is a size_t larger than connectionCountMaxLimit")
        {
            QVariantMap variantMap;
            auto pCounter = std::make_shared<std::atomic_size_t>();
            variantMap["connectionCount"].setValue(pCounter);
            variantMap["maxConnectionCount"].setValue<size_t>(ServerWorker::connectionCountMaxLimit() + 1);
            QVariant data(variantMap);
            REQUIRE(!emittedFailed);
            serverWorker.start(data);

            THEN("Server worker fails to start")
            {
                REQUIRE(emittedFailed);
                REQUIRE(emittedErrorMessage == std::string("Failed to start connection listener. Given maxConnectionCount is larger than ").append(std::to_string(ServerWorker::connectionCountMaxLimit())).append("."));
            }
        }
    }
}
