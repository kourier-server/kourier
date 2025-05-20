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

#ifndef KOURIER_ASYNC_SERVER_WORKER_H
#define KOURIER_ASYNC_SERVER_WORKER_H

#include "ServerWorker.h"
#include "../Core/AsyncQObject.h"
#include <type_traits>


namespace Kourier
{

class BaseAsyncServerWorker : public ServerWorker
{
Q_OBJECT
public:
    BaseAsyncServerWorker() = default;
    ~BaseAsyncServerWorker() override = default;

protected slots:
    void onAsyncServerWorkerStarted() {onAsyncServerWorkerStartedImpl();}
    void onAsyncServerWorkerStopped() {onAsyncServerWorkerStoppedImpl();}
    void onAsyncServerWorkerFailed(std::string_view errorMessage) {onAsyncServerWorkerFailedImpl(errorMessage);}

protected:
    virtual void onAsyncServerWorkerStartedImpl() = 0;
    virtual void onAsyncServerWorkerStoppedImpl() = 0;
    virtual void onAsyncServerWorkerFailedImpl(std::string_view errorMessage) = 0;
};

template <class T_ServerWorker, class... Args>
class AsyncServerWorker : public BaseAsyncServerWorker
{
public:
    AsyncServerWorker(Args... args) :
        m_worker(args...)
    {
        if (m_worker.get())
        {
            QObject::connect(m_worker.get(), &ServerWorker::started, this, &AsyncServerWorker::onAsyncServerWorkerStarted, Qt::QueuedConnection);
            QObject::connect(m_worker.get(), &ServerWorker::stopped, this, &AsyncServerWorker::onAsyncServerWorkerStopped, Qt::QueuedConnection);
            QObject::connect(m_worker.get(), &ServerWorker::failed, this, &AsyncServerWorker::onAsyncServerWorkerFailed, Qt::QueuedConnection);
        }
    }
    ~AsyncServerWorker() override = default;
    ExecutionState state() const override {return m_state;}

private:
    void doStart(QVariant data) override
    {
        if (m_state != ExecutionState::Stopped)
            return;
        m_pendingStop = false;
        if (m_worker.get() == nullptr)
            emit failed("Failed to create async server worker.");
        else if (!QMetaObject::invokeMethod(m_worker.get(), "start", Qt::QueuedConnection, data))
            emit failed("Failed to start async server worker.");
        else
            m_state = ExecutionState::Starting;
    }

    void doStop() override
    {
        switch (m_state)
        {
            case ExecutionState::Starting:
                m_pendingStop = true;
                break;
            case ExecutionState::Started:
                if (m_worker.get() == nullptr)
                    qFatal("Failed to create async server worker.");
                else if (!QMetaObject::invokeMethod(m_worker.get(), "stop", Qt::QueuedConnection))
                    qFatal("Failed to stop async server worker.");
                else
                    m_state = ExecutionState::Stopping;
                break;
            case ExecutionState::Stopping:
            case ExecutionState::Stopped:
                break;
        }
    }

private:
    void onAsyncServerWorkerStartedImpl() override
    {
        if (!m_pendingStop)
        {
            m_state = ExecutionState::Started;
            emit started();
        }
        else if (m_worker.get() == nullptr)
            qFatal("Failed to create async server worker.");
        else if (!QMetaObject::invokeMethod(m_worker.get(), "stop", Qt::QueuedConnection))
            qFatal("Failed to stop async server worker.");
        else
            m_state = ExecutionState::Stopping;
    }

    void onAsyncServerWorkerStoppedImpl() override
    {
        m_state = ExecutionState::Stopped;
        emit stopped();
    }

    void onAsyncServerWorkerFailedImpl(std::string_view errorMessage) override
    {
        m_state = ExecutionState::Stopped;
        emit failed(errorMessage);
    }

private:
    static_assert(std::is_base_of_v<ServerWorker, T_ServerWorker>);
    ExecutionState m_state = ExecutionState::Stopped;
    AsyncQObject<T_ServerWorker, Args...> m_worker;
    bool m_pendingStop = false;
};

}

#endif // KOURIER_ASYNC_SERVER_WORKER_H
