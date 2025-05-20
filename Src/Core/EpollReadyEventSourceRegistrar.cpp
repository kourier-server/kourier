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

#include "EpollReadyEventSourceRegistrar.h"
#include "UnixUtils.h"
#include <sys/eventfd.h>


namespace Kourier
{

EpollReadyEventSourceRegistrar::EpollReadyEventSourceRegistrar(EpollEventNotifier *pEventNotifier) :
    EpollEventSource(EPOLLET | EPOLLIN, pEventNotifier),
    m_eventFd(eventfd(0, EFD_NONBLOCK))
{
    if (-1 == m_eventFd)
        qFatal("Failed to create event for epoll-based event dispatcher. Exiting.");
}

EpollReadyEventSourceRegistrar::EpollReadyEventSourceRegistrar() :
    EpollEventSource(EPOLLET | EPOLLIN, EpollEventNotifier::current()),
    m_eventFd(eventfd(0, EFD_NONBLOCK))
{
    if (-1 == m_eventFd)
        qFatal("Failed to create event for epoll-based event dispatcher. Exiting.");
}

EpollReadyEventSourceRegistrar::~EpollReadyEventSourceRegistrar()
{
    setEnabled(false);
    UnixUtils::safeClose(m_eventFd);
}

void EpollReadyEventSourceRegistrar::addReadyEvent(EpollEventSource *pEpollEventSource, uint32_t events)
{
    if (!pEpollEventSource->m_isInReadyList)
    {
        pEpollEventSource->m_isInReadyList = true;
        pEpollEventSource->m_postedEventTypes = events;
        set();
        pEpollEventSource->m_pNext = m_pReadyEvents;
        pEpollEventSource->m_pPrevious = nullptr;
        if (m_pReadyEvents)
            m_pReadyEvents->m_pPrevious = pEpollEventSource;
        m_pReadyEvents = pEpollEventSource;
    }
    else
        pEpollEventSource->m_postedEventTypes = (pEpollEventSource->m_postedEventTypes | events);
}

void EpollReadyEventSourceRegistrar::removeReadyEvent(EpollEventSource *pEpollEventSource)
{
    if (!pEpollEventSource->m_isInReadyList)
        return;
    pEpollEventSource->m_isInReadyList = false;
    pEpollEventSource->m_postedEventTypes = 0;
    if (pEpollEventSource->m_pPrevious)
        pEpollEventSource->m_pPrevious->m_pNext = pEpollEventSource->m_pNext;
    if (pEpollEventSource->m_pNext)
        pEpollEventSource->m_pNext->m_pPrevious = pEpollEventSource->m_pPrevious;
    if (m_pReadyEvents == pEpollEventSource)
        m_pReadyEvents = m_pReadyEvents->m_pNext;
    else if (m_pEventsBeingTriggered == pEpollEventSource)
        m_pEventsBeingTriggered = m_pEventsBeingTriggered->m_pNext;
}

void EpollReadyEventSourceRegistrar::set()
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

void EpollReadyEventSourceRegistrar::reset()
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

void EpollReadyEventSourceRegistrar::onEvent(uint32_t epollEvents)
{
    if (EPOLLIN == (epollEvents & EPOLLIN))
    {
        reset();
        m_pEventsBeingTriggered = m_pReadyEvents;
        m_pReadyEvents = nullptr;
        while (m_pEventsBeingTriggered != nullptr)
        {
            auto *pEvent = m_pEventsBeingTriggered;
            m_pEventsBeingTriggered = m_pEventsBeingTriggered->m_pNext;
            if (m_pEventsBeingTriggered)
                m_pEventsBeingTriggered->m_pPrevious = nullptr;
            assert(pEvent);
            pEvent->m_isInReadyList = false;
            auto events = pEvent->m_postedEventTypes;
            pEvent->m_postedEventTypes = 0;
            pEvent->onEvent(events);
        }
    }
}

}
