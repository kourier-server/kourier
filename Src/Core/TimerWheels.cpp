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
    if (pTimer->timeout().count() >= (int64_t(1) << 42)) [[unlikely]]
        pTimer->timeout() = std::chrono::milliseconds((int64_t(1) << 42) - 1);
    m_timerWheels[idxFinder[std::countr_zero<uint64_t>(std::bit_floor<uint64_t>(pTimer->timeout().count()))]].addTimer(pTimer);
    setHighResolutionClockTickerEnabled(!m_timerWheels[0].isEmpty());
}

void TimerWheels::removeTimer(TimerPrivate *pTimer)
{
    assert(pTimer);
    if (pTimer->m_pTimerWheel) [[likely]]
        pTimer->m_pTimerWheel->removeTimer(pTimer);
}

Signal TimerWheels::timedOutTimers(TimerList timers) KOURIER_SIGNAL(&TimerWheels::timedOutTimers, timers);

void Kourier::TimerWheels::onLowResolutionTick()
{
    m_lowResolutionTime += std::chrono::milliseconds(64);
    ++m_lowResolutionTickCounter;
    uint8_t largestWheelToTick = 1;
    if (isMultipleOfPowerOfTwo(m_lowResolutionTickCounter, 30))
        largestWheelToTick = 6;
    else if (isMultipleOfPowerOfTwo(m_lowResolutionTickCounter, 24))
        largestWheelToTick = 5;
    else if (isMultipleOfPowerOfTwo(m_lowResolutionTickCounter, 18))
        largestWheelToTick = 4;
    else if (isMultipleOfPowerOfTwo(m_lowResolutionTickCounter, 12))
        largestWheelToTick = 3;
    else if (isMultipleOfPowerOfTwo(m_lowResolutionTickCounter, 6))
        largestWheelToTick = 2;
    else
        largestWheelToTick = 1;
    for (auto i = largestWheelToTick; i > 1; --i)
        processExpiredTimers(m_timerWheels[i].tick());
    auto expiredTimers = m_timerWheels[1].tick();
    TimerList timers;
    for (auto it = expiredTimers.begin(); it != expiredTimers.end(); ++it)
    {
        if (it.timer()->timeout().count() > 0)
            m_timerWheels[0].addTimer(it.timer());
        else
        {
            it.timer()->m_pTimerWheel = nullptr;
            it.timer()->m_idxTimerWheelSlot = 0;
            timers.pushFront(it.timer());
        }
    }
    setHighResolutionClockTickerEnabled(!m_timerWheels[0].isEmpty());
    if (!timers.isEmpty())
        timedOutTimers(timers);
}

void TimerWheels::onHighResolutionTick()
{
    auto expiredTimers = m_timerWheels[0].tick();
    setHighResolutionClockTickerEnabled(!m_timerWheels[0].isEmpty());
    timedOutTimers(expiredTimers);
}

}
