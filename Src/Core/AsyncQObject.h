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

#ifndef KOURIER_ASYNC_Q_OBJECT_H
#define KOURIER_ASYNC_Q_OBJECT_H

#include <QThread>
#include <QSemaphore>
#include <QMutexLocker>
#include <memory>
#include <tuple>
#include <utility>


namespace Kourier
{

static void deleteQObjectLater(QObject *pObject)
{
    if (pObject)
        pObject->deleteLater();
}

template <class T_Object, class... Args>
class AsyncQObject
{
static_assert(sizeof...(Args) <= 10);
public:
    explicit AsyncQObject(Args... args) :
        m_thread(new QThread),
        m_args(std::forward<Args>(args)...),
        m_object(nullptr, &deleteQObjectLater)
    {
        QObject::connect(m_thread.get(), &QThread::started, [this]() {this->on_thread_started();});
        QObject::connect(m_thread.get(), &QThread::finished, [this]() {this->on_thread_finished();});
        run();
    }

    ~AsyncQObject()
    {
        stop();
    }

    T_Object *get() {return m_object.get();}

private:
    void run()
    {
        if (m_thread->isRunning())
            return;
        m_thread->start();
        m_semaphore.acquire(1);
    }

    void stop()
    {
        if (!m_thread->isRunning())
            return;
        m_finishedMutex.lock();
        QSemaphore eventDispatcherDestroyedSemaphore;
        auto *pEventDispatcher = m_thread->eventDispatcher();
        QMetaObject::Connection connection;
        if (!m_hasFinished && pEventDispatcher)
            connection = QObject::connect(pEventDispatcher, &QObject::destroyed, [&eventDispatcherDestroyedSemaphore]{eventDispatcherDestroyedSemaphore.release();});
        else
            eventDispatcherDestroyedSemaphore.release();
        m_finishedMutex.unlock();
        m_thread->quit();
        m_thread->wait();
        eventDispatcherDestroyedSemaphore.acquire();
        if (connection)
            QObject::disconnect(connection);
    }

    void on_thread_started()
    {
        try
        {
            if constexpr (sizeof...(Args) == 0)
                m_object.reset(new T_Object);
            else if constexpr (sizeof...(Args) == 1)
                m_object.reset(new T_Object(std::get<0>(m_args)));
            else if constexpr (sizeof...(Args) == 2)
                m_object.reset(new T_Object(std::get<0>(m_args), std::get<1>(m_args)));
            else if constexpr (sizeof...(Args) == 3)
                m_object.reset(new T_Object(std::get<0>(m_args), std::get<1>(m_args), std::get<2>(m_args)));
            else if constexpr (sizeof...(Args) == 4)
                m_object.reset(new T_Object(std::get<0>(m_args), std::get<1>(m_args), std::get<2>(m_args), std::get<3>(m_args)));
            else if constexpr (sizeof...(Args) == 5)
                m_object.reset(new T_Object(std::get<0>(m_args), std::get<1>(m_args), std::get<2>(m_args), std::get<3>(m_args), std::get<4>(m_args)));
            else if constexpr (sizeof...(Args) == 6)
                m_object.reset(new T_Object(std::get<0>(m_args), std::get<1>(m_args), std::get<2>(m_args), std::get<3>(m_args), std::get<4>(m_args), std::get<5>(m_args)));
            else if constexpr (sizeof...(Args) == 7)
                m_object.reset(new T_Object(std::get<0>(m_args), std::get<1>(m_args), std::get<2>(m_args), std::get<3>(m_args), std::get<4>(m_args), std::get<5>(m_args), std::get<6>(m_args)));
            else if constexpr (sizeof...(Args) == 8)
                m_object.reset(new T_Object(std::get<0>(m_args), std::get<1>(m_args), std::get<2>(m_args), std::get<3>(m_args), std::get<4>(m_args), std::get<5>(m_args), std::get<6>(m_args), std::get<7>(m_args)));
            else if constexpr (sizeof...(Args) == 9)
                m_object.reset(new T_Object(std::get<0>(m_args), std::get<1>(m_args), std::get<2>(m_args), std::get<3>(m_args), std::get<4>(m_args), std::get<5>(m_args), std::get<6>(m_args), std::get<7>(m_args), std::get<8>(m_args)));
            else if constexpr (sizeof...(Args) == 10)
                m_object.reset(new T_Object(std::get<0>(m_args), std::get<1>(m_args), std::get<2>(m_args), std::get<3>(m_args), std::get<4>(m_args), std::get<5>(m_args), std::get<6>(m_args), std::get<7>(m_args), std::get<8>(m_args), std::get<9>(m_args)));
            m_semaphore.release(1);
        }
        catch (...)
        {
            m_object.reset(nullptr);
            m_semaphore.release(1);
        }
    }

    void on_thread_finished()
    {
        QMutexLocker locker(&m_finishedMutex);
        m_hasFinished = true;
        if (m_object)
            m_object.release()->deleteLater();
    }

private:
    Q_DISABLE_COPY_MOVE(AsyncQObject)
    std::unique_ptr<T_Object, void(*)(QObject*)> m_object;
    std::unique_ptr<QThread> m_thread;
    QSemaphore m_semaphore;
    QMutex m_finishedMutex;
    bool m_hasFinished = false;
    std::tuple<Args...> m_args;
};

}

#endif // KOURIER_ASYNC_Q_OBJECT_H
