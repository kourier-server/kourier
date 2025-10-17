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
#include "TimerPrivate_epoll.h"
#include <Spectator.h>
#include <vector>

using Kourier::TimerWheel;
using Kourier::TimerPrivate;


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


SCENARIO("TimerWheel has 64 slots with given resolution")
{
    GIVEN("a timer wheel with specified positive resolution")
    {
        const auto resolution = GENERATE(AS(std::chrono::milliseconds),
                                         std::chrono::milliseconds(1),
                                         std::chrono::milliseconds(10),
                                         std::chrono::milliseconds(18),
                                         std::chrono::milliseconds(1711),
                                         std::chrono::milliseconds(1000000),
                                         std::chrono::milliseconds(1000000000));
        TimerWheel timerWheel(resolution);
        const auto adjustedResolution = timerWheel.resolution();

        WHEN("time span for timer wheel is fetched")
        {
            const auto timeSpan = timerWheel.timeSpan();

            THEN("timer wheel informs correct time span as 64 times the adjusted resolution")
            {
                REQUIRE(timeSpan.count() == (int64_t)((uint64_t)adjustedResolution.count() << 6));
            }
        }
    }
}


SCENARIO("TimerWheel fails to add timers having a timeout greater than its time span")
{
    GIVEN("a timer wheel with a given resolution")
    {
        const auto resolution = GENERATE(AS(std::chrono::milliseconds),
                                         std::chrono::milliseconds(1),
                                         std::chrono::milliseconds(1 << 6),
                                         std::chrono::milliseconds(1 << 12),
                                         std::chrono::milliseconds(1 << 18),
                                         std::chrono::milliseconds(1 << 24),
                                         std::chrono::milliseconds(1 << 30),
                                         std::chrono::milliseconds(int64_t(1) << 36));
        TimerWheel timerWheel(resolution);

        WHEN("we try to add to wheel a timer with an interval greater than the wheel's time span")
        {
            TimerPrivate timer;
            const auto deltaOverMaxValue = GENERATE(AS(int64_t), 1, 3, 102417);
            timer.setInterval(std::chrono::milliseconds(timerWheel.timeSpan().count() + deltaOverMaxValue));

            THEN("timer wheel fails to add timer")
            {
                REQUIRE(timerWheel.isEmpty());
                REQUIRE(!timerWheel.addTimer(&timer));
                REQUIRE(timerWheel.isEmpty());
            }
        }
    }
}


SCENARIO("Timer wheel returns expired timers on tick")
{
    GIVEN("a timer wheel with a given resolution")
    {
        const auto resolution = GENERATE(AS(std::chrono::milliseconds),
                                         std::chrono::milliseconds(1),
                                         std::chrono::milliseconds(1 << 6),
                                         std::chrono::milliseconds(1 << 12),
                                         std::chrono::milliseconds(1 << 18),
                                         std::chrono::milliseconds(1 << 24),
                                         std::chrono::milliseconds(1 << 30),
                                         std::chrono::milliseconds(int64_t(1) << 36));
        TimerWheel timerWheel(resolution);
        REQUIRE(timerWheel.isEmpty());
        const auto previousTickCount = GENERATE(AS(size_t), 0, 12, 70);
        for (auto i = 0; i < previousTickCount; ++i)
            timerWheel.tick();

        WHEN("three timers are added to a slot")
        {
            const auto slotIndex = GENERATE_RANGE(AS(size_t), 0, 63);
            std::vector<TimerPrivate> timers(3);
            timers[0].setInterval(std::chrono::milliseconds(timerWheel.resolution().count()*slotIndex));
            timers[1].setInterval(std::chrono::milliseconds(timerWheel.resolution().count()*slotIndex + timerWheel.resolution().count()/2));
            timers[2].setInterval(std::chrono::milliseconds(timerWheel.resolution().count()*slotIndex + timerWheel.resolution().count() - 1));
            for (auto &timer : timers)
            {
                REQUIRE(!timerWheel.contains(&timer));
                REQUIRE(timerWheel.addTimer(&timer));
                REQUIRE(timerWheel.contains(&timer));
                REQUIRE(!timerWheel.isEmpty());
            }

            THEN("timers are returned as expired after slotIndex + 1 ticks")
            {
                for (auto i = 0; i < slotIndex; ++i)
                {
                    REQUIRE(timerWheel.tick().isEmpty());
                }
                auto expiredTimers = timerWheel.tick();
                REQUIRE(!expiredTimers.isEmpty());
                REQUIRE(timerWheel.isEmpty());

                AND_THEN("timer wheel releases ownership of expired timers and subtracts from each timer's timeout the time spent on wheel")
                {
                    for (auto it = expiredTimers.begin(); it != expiredTimers.end(); ++it)
                    {
                        REQUIRE(!timerWheel.contains(it.timer()));
                        REQUIRE(it.timer()->timeout().count() == (it.timer()->interval().count() - (slotIndex * timerWheel.resolution().count())));
                    }
                }
            }
        }
    }

    GIVEN("a timer wheel with a resolution of 1ms")
    {
        TimerWheel timerWheel(std::chrono::milliseconds(1));

        WHEN("a timer with an interval of 60ms is added to the timer wheel")
        {
            TimerPrivate timer60ms;
            timer60ms.setInterval(std::chrono::milliseconds(60));

            THEN("timer wheel successfully adds the timer")
            {
                REQUIRE(timerWheel.addTimer(&timer60ms));

                AND_WHEN("a timer with 5ms is added after 55 ticks")
                {
                    for (auto i = 0; i < 55; ++i)
                    {
                        REQUIRE(timerWheel.tick().isEmpty());
                    }
                    TimerPrivate timer5ms;
                    timer5ms.setInterval(std::chrono::milliseconds(5));
                    REQUIRE(timerWheel.addTimer(&timer5ms));

                    THEN("both timers are returned as expired after 5 ticks")
                    {
                        for (auto i = 0; i < 5; ++i)
                        {
                            REQUIRE(timerWheel.tick().isEmpty());
                        }
                        auto timerList = timerWheel.tick();
                        REQUIRE(timerList.size() == 2);
                        auto *pFirstTimer = timerList.popFirst();
                        auto *pSecondTimer = timerList.popFirst();
                        REQUIRE((pFirstTimer == &timer60ms && pSecondTimer == &timer5ms)
                                || (pFirstTimer == &timer5ms && pSecondTimer == &timer60ms));
                    }
                }
            }
        }
    }
}
