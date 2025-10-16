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

#include "TimerWheels.h"
#include <bit>


namespace Kourier
{

TimerWheels::TimerWheels(std::shared_ptr<ClockTicker> pLowResolutionClockTicker,
                         std::shared_ptr<ClockTicker> pHighResolutionClockTicker)
    : m_lowResolutionTime(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())),
      m_pLowResolutionClockTicker(pLowResolutionClockTicker),
      m_pHighResolutionClockTicker(pHighResolutionClockTicker)

{
    if (!m_pLowResolutionClockTicker) [[unlikely]]
        qFatal("Failed to create timer wheels. Given low resolution clock ticker is null.");
    if (!m_pHighResolutionClockTicker) [[unlikely]]
        qFatal("Failed to create timer wheels. Given high resolution clock ticker is null.");
    Object::connect(m_pLowResolutionClockTicker.get(), &ClockTicker::tick, this, &TimerWheels::onLowResolutionTick);
    Object::connect(m_pHighResolutionClockTicker.get(), &ClockTicker::tick, this, &TimerWheels::onHighResolutionTick);
}

void TimerWheels::addTimer(TimerPrivate *pTimer)
{
    assert(pTimer);
    const uint8_t idxFinder[42] = {0,0,0,0,0,0,
                                   1,1,1,1,1,1,
                                   2,2,2,2,2,2,
                                   3,3,3,3,3,3,
                                   4,4,4,4,4,4,
                                   5,5,5,5,5,5,
                                   6,6,6,6,6,6};
    pTimer->timeout() = std::chrono::milliseconds((uint64_t)pTimer->timeout().count() & ((uint64_t(1) << 42) - 1));
    m_timerWheels[idxFinder[std::countr_zero<uint64_t>(std::bit_floor<uint64_t>(pTimer->timeout().count()))]].addTimer(pTimer);
}

void TimerWheels::removeTimer(TimerPrivate *pTimer)
{
    assert(pTimer);
    if (pTimer->m_pTimerWheel) [[likely]]
        pTimer->m_pTimerWheel->removeTimer(pTimer);
}

Signal TimerWheels::timedOutTimers(TimerList timers) KOURIER_SIGNAL(&TimerWheels::timedOutTimers, timers)

}
