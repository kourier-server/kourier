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

#ifndef KOURIER_EPOLL_EVENT_SOURCE_H
#define KOURIER_EPOLL_EVENT_SOURCE_H

#include "EpollEventNotifier.h"
#include "Object.h"
#include <cstdint>


namespace Kourier
{

class EpollEventSource : public Object
{
KOURIER_OBJECT(Kourier::EpollEventSource);
public:
    EpollEventSource(uint32_t eventTypes) :
        m_pEventNotifier(EpollEventNotifier::current()),
        m_eventTypes(eventTypes) {}
    EpollEventSource(uint32_t eventTypes, EpollEventNotifier *pEpollEventNotifier) :
        m_pEventNotifier(pEpollEventNotifier),
        m_eventTypes(eventTypes) {assert(m_pEventNotifier != nullptr);}
    ~EpollEventSource() override
    {
        if (m_enabled)
            qCritical("Deleting an enabled event source. Event sources must be disabled prior to deletion.");
    }
    virtual int64_t fileDescriptor() const = 0;
    inline bool isEnabled() const {return m_enabled;}
    inline void setEnabled(bool enabled)
    {
        if (m_enabled == enabled)
            return;
        m_enabled = enabled;
        m_enabled ? m_pEventNotifier->add(this) : m_pEventNotifier->remove(this);
    }
    inline uint32_t eventTypes() const {return m_eventTypes;}
    inline void setEventTypes(uint32_t eventTypes)
    {
        if (m_eventTypes == eventTypes)
            return;
        m_eventTypes = eventTypes;
        if (m_enabled)
            m_pEventNotifier->modify(this);
    }
    inline EpollEventNotifier *eventNotifier() {return m_pEventNotifier;}

protected:
    virtual void onEvent(uint32_t epollEvents) = 0;

private:
    EpollEventNotifier * const m_pEventNotifier;
    EpollEventSource *m_pNext = nullptr;
    EpollEventSource *m_pPrevious = nullptr;
    uint32_t m_eventTypes = 0;
    uint32_t m_postedEventTypes = 0;
    bool m_enabled = false;
    bool m_isInReadyList = false;
    friend class EpollEventNotifier;
    friend class EpollReadyEventSourceRegistrar;
};

}

#endif // KOURIER_EPOLL_EVENT_SOURCE_H
