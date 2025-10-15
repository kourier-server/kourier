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
    m_slotFinderExponent(6 + std::countr_zero<uint64_t>(m_resolution.count()))
{
}

bool TimerWheel::addTimer(TimerPrivate *pTimer)
{
    assert(pTimer);
    if (pTimer->m_timeout < m_timeSpan) [[likely]]
    {
        ++m_timerCount;
        pTimer->m_pTimerWheel = this;
        m_slots[(uint64_t)pTimer->m_timeout.count() >> m_slotFinderExponent].addTimer(pTimer);
        return true;
    }
    else
        return false;
}

bool TimerWheel::removeTimer(TimerPrivate *pTimer)
{
    assert(pTimer);
    if (pTimer->m_timeout < m_timeSpan) [[likely]]
    {
        --m_timerCount;
        pTimer->m_pTimerWheel = nullptr;
        m_slots[(uint64_t)pTimer->m_timeout.count() >> m_slotFinderExponent].removeTimer(pTimer);
        return true;
    }
    else
        return false;
}

TimerList TimerWheel::tick()
{
    TimerList expiredTimers;
    expiredTimers.swap(m_slots[m_idxNextTimersToExpire++]);
    if (m_idxNextTimersToExpire == 64) [[unlikely]]
        m_idxNextTimersToExpire = 0;
    m_timerCount -= expiredTimers.size();
    return expiredTimers;
}

std::chrono::milliseconds TimerWheel::adjustResolution(std::chrono::milliseconds resolution)
{
    if (resolution.count() > 0) [[likely]]
    {
        static_assert(std::bit_floor<uint64_t>(std::chrono::milliseconds::max().count()) == (uint64_t(1) << 62));
        // std::chrono::milliseconds::max.count() == 1 << 62. Thus,
        // as timer wheels have 64 slots, then (resolution * (1 << 6)) <= (1 << 62).
        // As resolution must be a power of two, the max value for it's exponent is 56
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
