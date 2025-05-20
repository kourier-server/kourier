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

#include "EpollObjectDeleter.h"
#include "UnixUtils.h"
#include "Object.h"
#include <sys/eventfd.h>


namespace Kourier
{

EpollObjectDeleter::EpollObjectDeleter(EpollEventNotifier *pEventNotifier) :
    EpollEventSource(EPOLLET | EPOLLIN, pEventNotifier),
    m_eventFd(eventfd(0, EFD_NONBLOCK))
{
    if (-1 == m_eventFd)
        qFatal("Failed to create event for epoll-based event dispatcher. Exiting.");
}

EpollObjectDeleter::EpollObjectDeleter() :
    EpollEventSource(EPOLLET | EPOLLIN, EpollEventNotifier::current()),
    m_eventFd(eventfd(0, EFD_NONBLOCK))
{
    if (-1 == m_eventFd)
        qFatal("Failed to create event for epoll-based event dispatcher. Exiting.");
}

EpollObjectDeleter::~EpollObjectDeleter()
{
    setEnabled(false);
    UnixUtils::safeClose(m_eventFd);
    deleteScheduledObjects();
}

void EpollObjectDeleter::scheduleForDeletion(Object *pObject)
{
    for (auto pObj : m_objectsToDelete)
    {
        if (pObj == pObject)
            return;
    }
    set();
    m_objectsToDelete.push_back(pObject);
}

void EpollObjectDeleter::set()
{
    if (m_eventIsSet)
        return;
    else
    {
        setEnabled(true);
        m_eventIsSet = true;
        const uint64_t value = 1;
        UnixUtils::safeWrite(m_eventFd, (const char*)&value, sizeof(value));
    }
}

void EpollObjectDeleter::reset()
{
    if (!m_eventIsSet)
        return;
    else
    {
        m_eventIsSet = false;
        uint64_t value = 0;
        UnixUtils::safeRead(m_eventFd, (char*)&value, sizeof(value));
    }
}

void EpollObjectDeleter::onEvent(uint32_t epollEvents)
{
    if (EPOLLIN == (epollEvents & EPOLLIN))
    {
        reset();
        deleteScheduledObjects();
        m_objectsToDelete.clear();
    }
}

void EpollObjectDeleter::deleteScheduledObjects()
{
    for (std::vector<Object*>::size_type i = 0; i < m_objectsToDelete.size(); ++i)
    {
        auto *pObject = m_objectsToDelete[i];
        if (pObject != nullptr)
            delete pObject;
    }
}

}
