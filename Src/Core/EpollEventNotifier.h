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

#ifndef KOURIER_EPOLL_EVENT_NOTIFIER_H
#define KOURIER_EPOLL_EVENT_NOTIFIER_H

#include "SDK.h"
#include <QSocketNotifier>
#include <sys/epoll.h>
#include <memory.h>


namespace Kourier
{

class EpollTimerRegistrar;
class EpollEventSource;
class TimerPrivate;
class EpollObjectDeleter;
class Object;
class EpollReadyEventSourceRegistrar;

class KOURIER_EXPORT EpollEventNotifier
{
public:
    EpollEventNotifier();
    ~EpollEventNotifier();
    void registerTimer(TimerPrivate *pTimer);
    void unregisterTimer(TimerPrivate *pTimer);
    void scheduleForDeletion(Object *pObject);
    void postEvent(EpollEventSource *pEpollEventSource, uint32_t events);
    void removePostedEvents(EpollEventSource *pEpollEventSource);

private:
    static EpollEventNotifier *current();
    void add(EpollEventSource *pEpollEventSource) const;
    void modify(EpollEventSource *pEpollEventSource);
    void remove(EpollEventSource *pEpollEventSource);
    void discard(EpollEventSource *pEpollEventSource);
    void processEvents();
    void removeEventSourceFromPendingEvents(EpollEventSource *pEventSource);
    void clear();

private:
    static constexpr size_t m_maxNumberOfTriggeredEvents = static_cast<size_t>(1) << 16;
    EpollTimerRegistrar *m_pTimerRegistrar = nullptr;
    EpollObjectDeleter *m_pObjectDeleter = nullptr;
    EpollReadyEventSourceRegistrar *m_pReadyEventRegistrar = nullptr;
    const int m_epollInstanceFd = -1;
    int m_triggeredEventsCount = 0;
    int m_idx = 0;
    QSocketNotifier *m_pEpollSocketNotifier = nullptr;
    QVector<epoll_event> m_epollEventsCache;
    bool m_isProcessingEvents = false;
    bool m_isActive = true;
    friend class EpollEventSource;
    friend class EpollEventNotifierCleaner;
    friend class Object;
    friend class TimerPrivate;
    friend class EpollObjectDeleter;
    friend class EpollReadyEventSourceRegistrar;
    friend class EpollTimerRegistrar;
};

}

#endif // KOURIER_EPOLL_EVENT_NOTIFIER_H
