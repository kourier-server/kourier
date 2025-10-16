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
#include <Spectator.h>

using Kourier::TimerWheel;


SCENARIO("TimerWheel ajusts given resolution")
{
    GIVEN("a timer wheel created with a non-positive resolution")
    {
        const auto resolution = GENERATE(AS(std::chrono::milliseconds),
                                         std::chrono::milliseconds(-1),
                                         std::chrono::milliseconds(0),
                                         std::chrono::milliseconds(-125),
                                         std::chrono::milliseconds(std::chrono::milliseconds::min().count() + 18),
                                         std::chrono::milliseconds::min());
        REQUIRE(resolution.count() <= 0);
        TimerWheel timerWheel(resolution);

        WHEN("resolution is fetched from timer wheel")
        {
            const auto adjustedResolution = timerWheel.resolution();

            THEN("timer wheel adjusts given resolution to 1")
            {
                REQUIRE(adjustedResolution.count() == 1);
            }
        }
    }

    GIVEN("a timer wheel created with positive resolutions not greater than 2^56")
    {
        const auto resolution = GENERATE(AS(std::chrono::milliseconds),
                                         std::chrono::milliseconds(1),
                                         std::chrono::milliseconds(125),
                                         std::chrono::milliseconds(1024*1024),
                                         std::chrono::milliseconds((uint64_t(1) << 56) - 1),
                                         std::chrono::milliseconds((uint64_t(1) << 56)));
        REQUIRE(resolution.count() > 0);
        TimerWheel timerWheel(resolution);

        WHEN("resolution is fetched from timer wheel")
        {
            const auto adjustedResolution = timerWheel.resolution();

            THEN("timer wheel adjusts given resolution to the smallest integral power of two that is not smaller than given resolution")
            {
                REQUIRE(adjustedResolution >= resolution);
                REQUIRE(adjustedResolution.count() == std::bit_ceil<uint64_t>(resolution.count()));
            }
        }
    }

    GIVEN("a timer wheel created with positive resolutions greater than 2^56")
    {
        const auto resolution = GENERATE(AS(std::chrono::milliseconds),
                                         std::chrono::milliseconds((uint64_t(1) << 62) + 1),
                                         std::chrono::milliseconds((uint64_t(1) << 62) + 3),
                                         std::chrono::milliseconds(std::chrono::milliseconds::max().count() - 1),
                                         std::chrono::milliseconds(std::chrono::milliseconds::max().count() - 18),
                                         std::chrono::milliseconds::max());
        REQUIRE(resolution.count() > (uint64_t(1) << 62));
        TimerWheel timerWheel(resolution);

        WHEN("resolution is fetched from timer wheel")
        {
            const auto adjustedResolution = timerWheel.resolution();

            THEN("timer wheel adjusts given resolution to 2^56")
            {
                REQUIRE(adjustedResolution.count() == (uint64_t(1) << 56));
            }
        }
    }
}


// SCENARIO("TimerWheel has 64 slots with given resolution")
// {
//     GIVEN("a timer wheel with specified positive resolution")
//     {
//         const auto resolution = GENERATE(AS(std::chrono::milliseconds),
//                                          std::chrono::milliseconds(1),
//                                          std::chrono::milliseconds(10),
//                                          std::chrono::milliseconds(18),
//                                          std::chrono::milliseconds(1711),
//                                          std::chrono::milliseconds(1000000),
//                                          std::chrono::milliseconds(1000000000));
//         TimerWheel timerWheel(resolution);
//         REQUIRE(resolution == timerWheel.resolution());

//         WHEN("timer wheel period is fetched")
//         {
//             const auto period = timerWheel.period();

//             THEN("timer wheel informs correct period as 64 times the resolution")
//             {
//                 REQUIRE(period.count() == (resolution.count()*64));
//             }
//         }
//     }
// }


// SCENARIO("TimerWheel sets resolution to 1 if given resolution is not positive")
// {
//     GIVEN("a timer wheel with specified non-positive resolution")
//     {
//         const auto resolution = GENERATE(AS(std::chrono::milliseconds),
//                                          std::chrono::milliseconds(-1),
//                                          std::chrono::milliseconds(0),
//                                          std::chrono::milliseconds(-18),
//                                          std::chrono::milliseconds(-1711));
//         TimerWheel timerWheel(resolution);

//         WHEN("timer wheel resolution is fetched")
//         {
//             THEN("resolution is equal to 1 nanosecond")
//             {
//                 REQUIRE(timerWheel.resolution() == std::chrono::milliseconds(1));

//                 AND_WHEN("period is fetched")
//                 {
//                     const auto period = timerWheel.period();

//                     THEN("timer wheel informs correct period as 64 times the resolution")
//                     {
//                         REQUIRE(period == std::chrono::milliseconds(64));
//                     }
//                 }
//             }
//         }
//     }
// }


// SCENARIO("TimerWheel informs when timers expire")
// {
//     TimerWheel timerWheel(std::chrono::milliseconds(10000));

//     GIVEN("a timer wheel with three timers on each slot")
//     {
//         WHEN("a slot expires")
//         {
//             THEN("timers from expired slot are given in timersExpired signal")
//             {

//             }
//         }
//     }
// }
