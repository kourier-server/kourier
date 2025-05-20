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
#include "../Core/Object.h"
#include "ConnectionListener.h"
#include "ConnectionHandler.h"
#include "ConnectionHandlerFactory.h"
#include "ConnectionHandlerRepository.h"
#include "../Core/UnixUtils.h"
#include <atomic>
#include <memory>


namespace Kourier
{

class ServerWorkerImpl : public Object
{
KOURIER_OBJECT(Kourier::ServerWorkerImpl);
public:
    ServerWorkerImpl(ServerWorker *pServerWorker,
                     std::shared_ptr<ConnectionListener> pListener,
                     std::shared_ptr<ConnectionHandlerFactory> pHandlerFactory,
                     std::shared_ptr<ConnectionHandlerRepository> pHandlerRepository);
    ServerWorkerImpl(const ServerWorkerImpl &) = delete;
    ServerWorkerImpl &operator=(const ServerWorkerImpl&) = delete;
    ~ServerWorkerImpl() override;

public:
    void start(QVariant data);
    void stop();
    virtual ExecutionState state() const {return m_state;}

private:
    void onNewConnection(qintptr socketDescriptor);
    void onHandlerFinished() {--(*m_pConnectionCount);}
    void onHandlerRepositoryStopped();

private:
    ServerWorker *m_pServerWorker = nullptr;
    std::shared_ptr<ConnectionListener> m_pListener;
    std::shared_ptr<ConnectionHandlerFactory> m_pHandlerFactory;
    std::shared_ptr<ConnectionHandlerRepository> m_pHandlerRepository;
    size_t m_maxConnectionCount = ServerWorker::connectionCountMaxLimit();
    std::shared_ptr<std::atomic_size_t> m_pConnectionCount;
    ExecutionState m_state = ExecutionState::Stopped;
    bool m_hasAlreadyStarted = false;
};

ServerWorkerImpl::ServerWorkerImpl(ServerWorker *pServerWorker,
    std::shared_ptr<ConnectionListener> pListener,
    std::shared_ptr<ConnectionHandlerFactory> pHandlerFactory,
    std::shared_ptr<ConnectionHandlerRepository> pHandlerRepository) :
    m_pServerWorker(pServerWorker),
    m_pListener(pListener),
    m_pHandlerFactory(pHandlerFactory),
    m_pHandlerRepository(pHandlerRepository)
{
    if (m_pServerWorker && m_pListener && m_pHandlerFactory && m_pHandlerRepository)
    {
        Object::connect(m_pListener.get(), &ConnectionListener::newConnection, this, &ServerWorkerImpl::onNewConnection);
        Object::connect(m_pHandlerRepository.get(), &ConnectionHandlerRepository::stopped, this, &ServerWorkerImpl::onHandlerRepositoryStopped);
    }
}

ServerWorkerImpl::~ServerWorkerImpl()
{
    if (m_pListener)
        Object::disconnect(m_pListener.get(), nullptr, this, nullptr);
    if (m_pHandlerRepository)
        Object::disconnect(m_pHandlerRepository.get(), nullptr, this, nullptr);
}

void ServerWorkerImpl::start(QVariant data)
{
    if (m_hasAlreadyStarted)
        return;
    m_hasAlreadyStarted = true;
    if (!m_pListener)
    {
        if (m_pServerWorker)
            emit m_pServerWorker->failed("Failed to start server worker. Given connection listener is null.");
        return;
    }
    else if (!m_pHandlerFactory)
    {
        if (m_pServerWorker)
            emit m_pServerWorker->failed("Failed to start server worker. Given connection handler factory is null.");
        return;
    }
    else if (!m_pHandlerRepository)
    {
        if (m_pServerWorker)
            emit m_pServerWorker->failed("Failed to start server worker. Given connection handler repository is null.");
        return;
    }
    if (data.typeId() != QMetaType::QVariantMap)
    {
        if (m_pServerWorker)
            emit m_pServerWorker->failed("Failed to start connection listener. Given data is not a QVariantMap.");
        return;
    }
    const auto variantMap = data.toMap();
    if (!variantMap.contains("connectionCount"))
    {
        if (m_pServerWorker)
            emit m_pServerWorker->failed("Failed to start connection listener. Variable connectionCount has not been given. This is an internal error, please report a bug.");
        return;
    }
    else if (variantMap["connectionCount"].typeId() != qMetaTypeId<std::shared_ptr<std::atomic_size_t>>())
    {
        if (m_pServerWorker)
            emit m_pServerWorker->failed("Failed to start connection listener. Given connectionCount variable is not of type std::shared_ptr<std::atomic_size_t>. This is an internal error, please report a bug.");
        return;
    }
    else
        m_pConnectionCount = variantMap["connectionCount"].value<std::shared_ptr<std::atomic_size_t>>();
    if (variantMap.contains("maxConnectionCount"))
    {
        if (variantMap["maxConnectionCount"].typeId() != qMetaTypeId<size_t>())
        {
            if (m_pServerWorker)
                emit m_pServerWorker->failed("Failed to start connection listener. Given maxConnectionCount must be of type size_t. This is an internal error, please report a bug.");
            return;
        }
        const auto maxConnectionCount = variantMap["maxConnectionCount"].value<size_t>();
        if (maxConnectionCount > ServerWorker::connectionCountMaxLimit())
        {
            if (m_pServerWorker)
                emit m_pServerWorker->failed(std::string("Failed to start connection listener. Given maxConnectionCount is larger than ").append(std::to_string(ServerWorker::connectionCountMaxLimit())).append("."));
            return;
        }
        else
            m_maxConnectionCount = maxConnectionCount;
    }
    if (!m_pListener->start(data))
    {
        if (m_pServerWorker)
            emit m_pServerWorker->failed(m_pListener->errorMessage());
        return;
    }
    else
    {
        m_state = ExecutionState::Started;
        if (m_pServerWorker)
            emit m_pServerWorker->started();
    }
}

void ServerWorkerImpl::stop()
{
    if (m_state != ExecutionState::Started)
        return;
    m_state = ExecutionState::Stopping;
    Object::disconnect(m_pListener.get(), &ConnectionListener::newConnection, this, &ServerWorkerImpl::onNewConnection);
    m_pListener = {};
    m_pHandlerRepository->stop();
}

void ServerWorkerImpl::onNewConnection(qintptr socketDescriptor)
{
    if ((++(*m_pConnectionCount) <= m_maxConnectionCount) || (m_maxConnectionCount == 0))
    {
        auto *pHandler = m_pHandlerFactory->create(socketDescriptor);
        if (pHandler)
        {
            Object::connect(pHandler, &ConnectionHandler::finished, this, &ServerWorkerImpl::onHandlerFinished);
            m_pHandlerRepository->add(pHandler);
            return;
        }
        else
            --(*m_pConnectionCount);
    }
    else
    {
        UnixUtils::safeClose(socketDescriptor);
        --(*m_pConnectionCount);
    }
}

void ServerWorkerImpl::onHandlerRepositoryStopped()
{
    m_state = ExecutionState::Stopped;
    if (m_pServerWorker)
        emit m_pServerWorker->stopped();
}

ServerWorker::ServerWorker()
{
}

ServerWorker::ServerWorker(
    std::shared_ptr<ConnectionListener> pListener,
    std::shared_ptr<ConnectionHandlerFactory> pHandlerFactory,
    std::shared_ptr<ConnectionHandlerRepository> pHandlerRepository) :
    m_pServerWorkerImpl(new ServerWorkerImpl(this, pListener, pHandlerFactory, pHandlerRepository))
{
}

ServerWorker::~ServerWorker()
{
}

ExecutionState ServerWorker::state() const
{
    return m_pServerWorkerImpl->state();
}

void ServerWorker::doStart(QVariant data)
{
    m_pServerWorkerImpl->start(data);
}

void ServerWorker::doStop()
{
    m_pServerWorkerImpl->stop();
}

}
