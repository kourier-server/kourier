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

#ifndef KOURIER_TIMER_PRIVATE_H
#define KOURIER_TIMER_PRIVATE_H

#include "Timer.h"
#include <chrono>


namespace Kourier
{

class TimerPrivate;

struct TimerListNode
{
    TimerListNode *pNext = nullptr;
    TimerListNode *pPrevious = nullptr;
    TimerPrivate* pTimer = nullptr;
};

class TimerPrivate
{
public:
    TimerPrivate();
    ~TimerPrivate() {stop();}
    inline void start() {activateTimer(m_interval);}
    inline void start(std::chrono::milliseconds interval) {activateTimer(interval);}
    inline void stop() {deactivateTimer();}
    inline bool isActive() const {return m_state == State::Active;}
    inline bool isSingleShot() const {return m_isSingleShot;}
    inline void setSingleShot(bool singleShot) {m_isSingleShot = singleShot;}
    inline std::chrono::milliseconds interval() const {return m_interval;}
    inline std::chrono::milliseconds timeout() const {return m_interval;}
    void setInterval(std::chrono::milliseconds interval);

private:
    void activateTimer(std::chrono::milliseconds interval);
    void deactivateTimer();
    void processTimeout();

private:
    Timer *q_ptr = nullptr;
    Q_DECLARE_PUBLIC(Timer)
    TimerListNode m_listNode;
    std::chrono::milliseconds m_interval;
    std::chrono::milliseconds m_timeout;
    EpollEventNotifier * const m_pEventNotifier;
    bool m_isSingleShot = false;
    enum class State : uint8_t {Active, Inactive};
    State m_state = State::Inactive;
    friend class EpollTimerRegistrar;
    friend class TimerWheel;
    friend class TimerList;
};

class TimerList
{
public:
    TimerList() = default;
    ~TimerList()
    {
        while (!isEmpty())
            popFirst();
    }
    inline void swap(TimerList &other)
    {
        if (this != &other) [[likely]]
        {
            TimerList temp = *this;
            *this = other;
            other = temp;
        }
    }
    inline void addTimer(TimerPrivate *pTimer)
    {
        assert(pTimer);
        pTimer->m_listNode.pNext = m_head.pNext;
        m_head.pNext = &pTimer->m_listNode;
        pTimer->m_listNode.pPrevious = &m_head;
    }
    inline static void removeTimer(TimerPrivate *pTimer)
    {
        assert(pTimer);
        if (pTimer->m_listNode.pPrevious)
            pTimer->m_listNode.pPrevious->pNext = pTimer->m_listNode.pNext;
        if (pTimer->m_listNode.pNext)
            pTimer->m_listNode.pNext->pPrevious = pTimer->m_listNode.pPrevious;
    }
    inline TimerPrivate *popFirst()
    {
        TimerPrivate *pTimer = nullptr;
        if (m_head.pNext)
        {
            pTimer = m_head.pNext->pTimer;
            m_head.pNext = m_head.pNext->pNext;
            if (m_head.pNext)
                m_head.pNext->pPrevious = nullptr;
        }
        return pTimer;
    }
    inline bool isEmpty() const {return m_head.pNext == nullptr;}

private:
    TimerListNode m_head;
};

}

#endif // KOURIER_TIMER_PRIVATE_H
