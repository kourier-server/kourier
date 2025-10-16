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

#ifndef KOURIER_TIMER_WHEELS_H
#define KOURIER_TIMER_WHEELS_H

#include "TimerWheel.h"
#include "ClockTicker.h"
#include <memory>


namespace Kourier
{

class TimerWheels : public Object
{
KOURIER_OBJECT(Kourier::TimerWheels)
public:
    TimerWheels(std::shared_ptr<ClockTicker> pLowResolutionClockTicker,
                std::shared_ptr<ClockTicker> pHighResolutionClockTicker);
    ~TimerWheels() = default;
    void addTimer(TimerPrivate *pTimer);
    void removeTimer(TimerPrivate *pTimer);
    inline std::chrono::milliseconds lowResolutionTime() const {return m_lowResolutionTime;}
    Signal timedOutTimers(TimerList timers);

private:
    void onLowResolutionTick();
    void onHighResolutionTick();
    void setHighResolutionClockTickerEnabled(bool enabled);

private:
    std::chrono::milliseconds m_lowResolutionTime;
    std::shared_ptr<ClockTicker> m_pLowResolutionClockTicker;
    std::shared_ptr<ClockTicker> m_pHighResolutionClockTicker;
    TimerWheel m_timerWheels[7] = {TimerWheel(std::chrono::milliseconds(1)),
                                   TimerWheel(std::chrono::milliseconds(uint64_t(1) << 6)),
                                   TimerWheel(std::chrono::milliseconds(uint64_t(1) << 12)),
                                   TimerWheel(std::chrono::milliseconds(uint64_t(1) << 18)),
                                   TimerWheel(std::chrono::milliseconds(uint64_t(1) << 24)),
                                   TimerWheel(std::chrono::milliseconds(uint64_t(1) << 30)),
                                   TimerWheel(std::chrono::milliseconds(uint64_t(1) << 36))};
};

}

#endif // KOURIER_TIMER_WHEELS_H
