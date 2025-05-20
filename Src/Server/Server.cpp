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
#include <QThread>


namespace Kourier
{

Server::Server(std::shared_ptr<ServerWorkerFactory> pServerWorkerFactory) :
    m_pServerWorkerFactory(pServerWorkerFactory),
    m_workerCount(QThread::idealThreadCount())
{
    if (pServerWorkerFactory.get() == nullptr)
        qFatal("ServerWorkerFactory is null.");
}

bool Server::start(QVariant data)
{
    if (m_state != ExecutionState::Stopped)
        return false;
    else
    {
        assert(m_workers.empty());
        m_state = ExecutionState::Starting;
        m_pendingStop = false;
        m_hasFailed = false;
        for (auto i = 0; i < m_workerCount; ++i)
        {
            auto pWorker = m_pServerWorkerFactory->create();
            QObject::connect(pWorker.get(), &ServerWorker::started, this, &Server::onWorkerStarted);
            QObject::connect(pWorker.get(), &ServerWorker::stopped, this, &Server::onWorkerStopped);
            QObject::connect(pWorker.get(), &ServerWorker::failed, this, &Server::onWorkerFailed);
            m_workers.push_back(pWorker);
        }
        for (auto &worker : m_workers)
            worker->start(data);
        return true;
    }
}

void Server::stop()
{
    switch (m_state)
    {
        case ExecutionState::Starting:
            m_pendingStop = true;
            break;
        case ExecutionState::Started:
            for (auto &worker : m_workers)
                worker->stop();
            m_state = ExecutionState::Stopping;
            break;
        case ExecutionState::Stopping:
        case ExecutionState::Stopped:
            break;
    }
}

void Server::setWorkerCount(int workerCount)
{
    m_workerCount = (workerCount > 0 && workerCount <= QThread::idealThreadCount()) ? workerCount : m_workerCount;
}

void Server::onWorkerStarted()
{
    processStartingServerWorkers();
}

void Server::onWorkerStopped()
{
    processStoppingServerWorkers();
}

void Server::onWorkerFailed(std::string_view errorMessage)
{
    m_errorMessage = errorMessage;
    m_hasFailed = true;
    processStartingServerWorkers();
}

void Server::processStartingServerWorkers()
{
    for (auto &worker : m_workers)
    {
        switch (worker->state())
        {
            case ExecutionState::Started:
            case ExecutionState::Stopped:
                break;
            case ExecutionState::Starting:
            case ExecutionState::Stopping:
                return;
        }
    }
    if (!m_hasFailed && !m_pendingStop)
    {
        m_state = ExecutionState::Started;
        emit started();
    }
    else
    {
        m_state = ExecutionState::Stopping;
        for (auto &worker : m_workers)
        {
            if (worker->state() == ExecutionState::Started)
                worker->stop();
        }
        processStoppingServerWorkers();
    }
}

void Server::processStoppingServerWorkers()
{
    for (auto &worker : m_workers)
    {
        if (worker->state() != ExecutionState::Stopped)
            return;
    }
    m_state = ExecutionState::Stopped;
    m_workers.clear();
    if (m_hasFailed)
        emit failed(m_errorMessage);
    else
        emit stopped();
}

}
