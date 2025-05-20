//
// Copyright (C) 2023 Glauco Pacheco <glauco@kourier.io>
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

#include "EpollEventNotifier.h"
#include "EpollEventSource.h"
#include "EpollTimerRegistrar.h"
#include "EpollObjectDeleter.h"
#include "EpollReadyEventSourceRegistrar.h"
#include "UnixUtils.h"
#include "NoDestroy.h"


namespace Kourier
{

EpollEventNotifier::EpollEventNotifier()
    : m_epollInstanceFd(epoll_create1(0)),
    m_pEpollSocketNotifier(new QSocketNotifier(m_epollInstanceFd, QSocketNotifier::Read))
{
    if (m_epollInstanceFd < 0)
        qFatal("Failed to create epoll instance. Exiting.");
    m_epollEventsCache.resize(m_maxNumberOfTriggeredEvents);
    QObject::connect(m_pEpollSocketNotifier, &QSocketNotifier::activated, m_pEpollSocketNotifier, [this]{this->processEvents();});
    m_pTimerRegistrar = new EpollTimerRegistrar(this);
    m_pTimerRegistrar->m_enabled = true;
    add(m_pTimerRegistrar);
    m_pObjectDeleter = new EpollObjectDeleter(this);
    m_pObjectDeleter->m_enabled = true;
    add(m_pObjectDeleter);
    m_pReadyEventRegistrar = new EpollReadyEventSourceRegistrar(this);
    m_pReadyEventRegistrar->m_enabled = true;
    add(m_pReadyEventRegistrar);
}

EpollEventNotifier::~EpollEventNotifier()
{
    if (m_isActive)
        clear();
}

void EpollEventNotifier::registerTimer(TimerPrivate *pTimer)
{
    if (m_isActive)
        m_pTimerRegistrar->add(pTimer);
}

void EpollEventNotifier::unregisterTimer(TimerPrivate *pTimer)
{
    if (m_isActive)
        m_pTimerRegistrar->remove(pTimer);
}

void EpollEventNotifier::scheduleForDeletion(Object *pObject)
{
    if (m_isActive)
        m_pObjectDeleter->scheduleForDeletion(pObject);
    else
        delete pObject;
}

void EpollEventNotifier::postEvent(EpollEventSource *pEpollEventSource, uint32_t events)
{
    if (m_isActive)
        m_pReadyEventRegistrar->addReadyEvent(pEpollEventSource, events);
}

void EpollEventNotifier::removePostedEvents(EpollEventSource *pEpollEventSource)
{
    if (m_isActive)
        m_pReadyEventRegistrar->removeReadyEvent(pEpollEventSource);
}

EpollEventNotifier *EpollEventNotifier::current()
{
    static thread_local NoDestroy<EpollEventNotifier> epollEventNotifier;
    static thread_local NoDestroyCleaner<EpollEventNotifier> epollEventNotifierCleaner(epollEventNotifier, &EpollEventNotifier::clear);
    return &(epollEventNotifier());
}

void EpollEventNotifier::add(EpollEventSource *pEpollEventSource) const
{
    if (m_isActive)
    {
        epoll_event epollEvent{0, nullptr};
        epollEvent.events = pEpollEventSource->eventTypes();
        epollEvent.data.ptr = pEpollEventSource;
        if (0 == epoll_ctl(m_epollInstanceFd, EPOLL_CTL_ADD, pEpollEventSource->fileDescriptor(), &epollEvent))
            return;
        else
            qFatal("Failed to add event source to epoll instance. Exiting.");
    }
}

void EpollEventNotifier::modify(EpollEventSource *pEpollEventSource)
{
    if (m_isActive)
    {
        epoll_event epollEvent{0, nullptr};
        epollEvent.events = pEpollEventSource->eventTypes();
        epollEvent.data.ptr = pEpollEventSource;
        if (0 == epoll_ctl(m_epollInstanceFd, EPOLL_CTL_MOD, pEpollEventSource->fileDescriptor(), &epollEvent))
            removeEventSourceFromPendingEvents(pEpollEventSource);
        else
            qFatal("Failed to modify event source of epoll instance. Exiting.");
    }
}

void EpollEventNotifier::remove(EpollEventSource *pEpollEventSource)
{
    if (m_isActive)
    {
        if (0 == epoll_ctl(m_epollInstanceFd, EPOLL_CTL_DEL, pEpollEventSource->fileDescriptor(), nullptr))
            removeEventSourceFromPendingEvents(pEpollEventSource);
        else
            qFatal("Failed to remove event source from epoll instance. Exiting.");
    }
}

void EpollEventNotifier::processEvents()
{
    if (m_isActive && !m_isProcessingEvents)
    {
        m_isProcessingEvents = true;
        // no problem if we get interrupted by a signal
        auto * const pData = m_epollEventsCache.data();
        m_triggeredEventsCount = ::epoll_wait(m_epollInstanceFd, pData, EpollEventNotifier::m_maxNumberOfTriggeredEvents, 0);
        for (m_idx = 0; m_idx < m_triggeredEventsCount; ++m_idx)
        {
            auto *pEvent = static_cast<EpollEventSource*>(pData[m_idx].data.ptr);
            if (pEvent && pEvent->isEnabled())
                pEvent->onEvent(pData[m_idx].events);
        }
        m_isProcessingEvents = false;
    }
}

void EpollEventNotifier::removeEventSourceFromPendingEvents(EpollEventSource *pEventSource)
{
    if (m_isActive && m_isProcessingEvents)
    {
        auto * const pData = m_epollEventsCache.data();
        for (auto i = (m_idx + 1); i < m_triggeredEventsCount; ++i)
        {
            if (pData[i].data.ptr == pEventSource)
                pData[i].data.ptr = nullptr;
        }
    }
}

void EpollEventNotifier::clear()
{
    if (m_isProcessingEvents)
        qFatal("Failed to destroy EpollEventNotifier. Instance is processing events.");
    else
    {
        delete m_pEpollSocketNotifier;
        m_pEpollSocketNotifier = nullptr;
        m_epollEventsCache.clear();
        m_epollEventsCache.squeeze();
        m_pTimerRegistrar->m_enabled = false;
        remove(m_pTimerRegistrar);
        m_pObjectDeleter->m_enabled = false;
        remove(m_pObjectDeleter);
        m_pReadyEventRegistrar->m_enabled = false;
        remove(m_pReadyEventRegistrar);
        UnixUtils::safeClose(m_epollInstanceFd);
        m_isActive = false;
        delete m_pTimerRegistrar;
        delete m_pObjectDeleter;
        delete m_pReadyEventRegistrar;
    }
}

}
