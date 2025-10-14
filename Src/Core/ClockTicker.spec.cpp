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

#include "ClockTicker.h"
#include <Spectator.h>
#include <QSemaphore>
#include <chrono>

using Kourier::ClockTicker;
using Kourier::Object;
using Spectator::SemaphoreAwaiter;
using namespace std::chrono_literals;


SCENARIO("ClockTicker ticks at specified resolution")
{
    GIVEN("a clock ticker created with a resolution")
    {
        const auto resolution = GENERATE(AS(std::chrono::milliseconds), 1ms, 4ms);
        ClockTicker clockTicker(resolution);

        WHEN("clock ticker is enabled")
        {
            std::vector<std::chrono::milliseconds> tickTimes(2);
            size_t currentTick = 0;
            QSemaphore tickSemaphore;
            Object::connect(&clockTicker, &ClockTicker::tick, [&]()
            {
                tickTimes[currentTick++] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());
                tickSemaphore.release();
                if (currentTick == tickTimes.size())
                    clockTicker.setEnabled(false);
            });
            clockTicker.setEnabled(true);
            const auto startTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());

            THEN("clock ticker ticks at given resolution")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(tickSemaphore, tickTimes.size(), 10));
                auto previousTime = startTime;
                for (const auto &tickTime : tickTimes)
                {
                    REQUIRE((tickTime - previousTime) == resolution);
                    previousTime = tickTime;
                }
            }
        }
    }
}
