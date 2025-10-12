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

#include "TimerWheel.h"
#include <Spectator.h>

using Kourier::TimerWheel;


SCENARIO("TimerWheel has 64 slots with given resolution")
{
    GIVEN("a timer wheel with specified positive resolution")
    {
        const auto resolution = GENERATE(AS(std::chrono::nanoseconds),
                                         std::chrono::nanoseconds(1),
                                         std::chrono::nanoseconds(10),
                                         std::chrono::nanoseconds(18),
                                         std::chrono::nanoseconds(1711),
                                         std::chrono::nanoseconds(1000000),
                                         std::chrono::nanoseconds(1000000000));
        TimerWheel timerWheel(resolution);
        REQUIRE(resolution == timerWheel.resolution());

        WHEN("timer wheel period is fetched")
        {
            const auto period = timerWheel.period();

            THEN("timer wheel informs correct period as 64 times the resolution")
            {
                REQUIRE(period.count() == (resolution.count()*64));
            }
        }
    }
}


SCENARIO("TimerWheel sets resolution to 1 if given resolution is not positive")
{
    GIVEN("a timer wheel with specified non-positive resolution")
    {
        const auto resolution = GENERATE(AS(std::chrono::nanoseconds),
                                         std::chrono::nanoseconds(-1),
                                         std::chrono::nanoseconds(0),
                                         std::chrono::nanoseconds(-18),
                                         std::chrono::nanoseconds(-1711));
        TimerWheel timerWheel(resolution);

        WHEN("timer wheel resolution is fetched")
        {
            THEN("resolution is equal to 1 nanosecond")
            {
                REQUIRE(timerWheel.resolution() == std::chrono::nanoseconds(1));

                AND_WHEN("period is fetched")
                {
                    const auto period = timerWheel.period();

                    THEN("timer wheel informs correct period as 64 times the resolution")
                    {
                        REQUIRE(period == std::chrono::nanoseconds(64));
                    }
                }
            }
        }
    }
}
