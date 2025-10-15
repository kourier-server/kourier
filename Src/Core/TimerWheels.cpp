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

Signal TimerWheels::timedOutTimers(TimerList timers) KOURIER_SIGNAL(&TimerWheels::timedOutTimers, timers)

}
