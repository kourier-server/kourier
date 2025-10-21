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

#include "TimerWheel.h"
#include <bit>


namespace Kourier
{

TimerWheel::TimerWheel(std::chrono::milliseconds resolution) :
    m_resolution(adjustResolution(resolution)),
    m_timeSpan((uint64_t)m_resolution.count() << 6),
    m_resolutionExponent(std::countr_zero<uint64_t>(m_resolution.count()))
{
}

bool TimerWheel::addTimer(TimerPrivate *pTimer)
{
    if (canAdd(pTimer)) [[likely]]
    {
        ++m_timerCount;
        pTimer->m_pTimerWheel = this;
        pTimer->m_idxTimerWheelSlot = (((uint64_t)pTimer->m_timeout.count() >> m_resolutionExponent) + m_idxLastTimersToExpire) & 63;
        m_slots[pTimer->m_idxTimerWheelSlot].pushFront(pTimer);
        return true;
    }
    else [[unlikely]]
        return false;
}

bool TimerWheel::removeTimer(TimerPrivate *pTimer)
{
    if (contains(pTimer)) [[likely]]
    {
        --m_timerCount;
        m_slots[pTimer->m_idxTimerWheelSlot].remove(pTimer);
        pTimer->m_pTimerWheel = nullptr;
        pTimer->m_idxTimerWheelSlot = 0;
        return true;
    }
    else [[unlikely]]
        return false;
}

TimerList TimerWheel::tick()
{
    if (++m_idxLastTimersToExpire == 64) [[unlikely]]
        m_idxLastTimersToExpire = 0;
    TimerList expiredTimers;
    expiredTimers.swap(m_slots[m_idxLastTimersToExpire]);
    m_timerCount -= expiredTimers.size();
    for (auto it = expiredTimers.begin(); it != expiredTimers.end(); ++it)
    {
        it.timer()->m_pTimerWheel = nullptr;
        it.timer()->m_idxTimerWheelSlot = 0;
        it.timer()->m_timeout = std::chrono::milliseconds((uint64_t)it.timer()->m_timeout.count() & ((uint64_t)(m_resolution.count() - 1)));
    }
    return expiredTimers;
}

std::chrono::milliseconds TimerWheel::adjustResolution(std::chrono::milliseconds resolution)
{
    if (resolution.count() > 0) [[likely]]
    {
        static_assert(std::bit_floor<uint64_t>(std::chrono::milliseconds::max().count()) == (uint64_t(1) << 62));
        // The resolution must be a power of two. Thus, the max time span (64*resolution)
        // is also a power of two. As the largest power of two representeable as
        // std::chrono::milliseconds is 2^62, the max value for the resolution is 2^56.
        if (std::has_single_bit<uint64_t>(resolution.count())
            && std::countr_zero<uint64_t>(resolution.count()) <= 56) [[likely]]
        {
            return resolution;
        }
        else [[unlikely]]
        {
            const uint64_t resolutionInMSecs = resolution.count();
            const int64_t resolutionInMSecsAsNextPowerOfTwo = std::min(uint64_t(1) << 56, std::bit_ceil<uint64_t>(resolutionInMSecs));
            return std::chrono::milliseconds(resolutionInMSecsAsNextPowerOfTwo);
        }
    }
    else [[unlikely]]
        return std::chrono::milliseconds(1);
}

}
