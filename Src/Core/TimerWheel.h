//
// Copyright (C) 2025 Glauco Pacheco <glauco@kourier.io>
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

#ifndef KOURIER_TIMER_WHEEL_H
#define KOURIER_TIMER_WHEEL_H

#include "TimerPrivate_epoll.h"
#include "TimerList.h"
#include <chrono>


namespace Kourier
{

class TimerWheel
{
public:
    TimerWheel(std::chrono::milliseconds resolution);
    ~TimerWheel() = default;
    inline std::chrono::milliseconds resolution() const {return m_resolution;}
    inline std::chrono::milliseconds timeSpan() const {return m_timeSpan;}
    inline bool isEmpty() const {return m_timerCount == 0;}
    inline bool canAdd(TimerPrivate *pTimer) {return (pTimer && m_resolution <= pTimer->m_timeout && pTimer->m_timeout < m_timeSpan);}
    bool addTimer(TimerPrivate *pTimer);
    inline bool contains(TimerPrivate *pTimer) {return (pTimer && pTimer->m_pTimerWheel == this);}
    bool removeTimer(TimerPrivate *pTimer);
    TimerList tick();
    inline size_t getSlotIdx(std::chrono::milliseconds timeout) {return (((uint64_t)timeout.count() >> m_resolutionExponent) + m_idxLastTimersToExpire) & 63;}
    inline uint64_t timerCount() const {return m_timerCount;}

private:
    static std::chrono::milliseconds adjustResolution(std::chrono::milliseconds resolution);

private:
    const std::chrono::milliseconds m_resolution = std::chrono::milliseconds(0);
    const std::chrono::milliseconds m_timeSpan = std::chrono::milliseconds(0);
    const uint64_t m_resolutionExponent = 0;
    uint64_t m_idxLastTimersToExpire = 0;
    uint64_t m_timerCount = 0;
    TimerList m_slots[64];
};

}

#endif // KOURIER_TIMER_WHEEL_H
