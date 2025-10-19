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

#include "TimerNotifier.h"
#include "TimerPrivate_epoll.h"
#include <Spectator.h>
#include <QSemaphore>
#include <vector>
#include <set>

using Kourier::TimerNotifier;
using Kourier::TimerPrivate;
using Kourier::TimerList;
using Kourier::ClockTicker;
using Kourier::Object;
using Spectator::SemaphoreAwaiter;


SCENARIO("TimerNotifier starts low resolution time to time elapsed since epoch")
{
    GIVEN("a timer notifier")
    {
        std::shared_ptr<ClockTicker> pLowResolutionClockTicker(new ClockTicker(std::chrono::milliseconds::max()));
        std::shared_ptr<ClockTicker> pHighResolutionClockTicker(new ClockTicker(std::chrono::milliseconds::max()));
        TimerNotifier timerNotifier(pLowResolutionClockTicker, pHighResolutionClockTicker);
        const auto timeElapsedSinceEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());

        WHEN("current low resolution time is fetched for time wheels")
        {
            const auto lowResolutionTime = timerNotifier.lowResolutionTime();

            THEN("timer wheel gives elapsed time since epoch")
            {
                REQUIRE(std::abs((lowResolutionTime - timeElapsedSinceEpoch).count()) <= 1);
            }
        }
    }
}


SCENARIO("TimerNotifier increases low resolution time by 64ms whenever low resolution clock ticker ticks")
{
    GIVEN("a timer notifier")
    {
        std::shared_ptr<ClockTicker> pLowResolutionClockTicker(new ClockTicker(std::chrono::milliseconds::max()));
        std::shared_ptr<ClockTicker> pHighResolutionClockTicker(new ClockTicker(std::chrono::milliseconds::max()));
        TimerNotifier timerNotifier(pLowResolutionClockTicker, pHighResolutionClockTicker);

        WHEN("low resolution clock ticker ticks")
        {
            const auto lowResolutionTime = timerNotifier.lowResolutionTime();
            const auto tickCount = GENERATE(AS(size_t), 1, 3, 8);
            for (auto i = 0; i < tickCount; ++i)
                pLowResolutionClockTicker->tick();

            THEN("low resolution time increases 64ms per tick")
            {
                REQUIRE(timerNotifier.lowResolutionTime() == (lowResolutionTime + std::chrono::milliseconds(tickCount*64)));
            }
        }
    }
}


SCENARIO("TimerNotifier enables low resolution clock ticker on creation")
{
    GIVEN("disabled low and high resolution clock tickers")
    {
        std::shared_ptr<ClockTicker> pLowResolutionClockTicker(new ClockTicker(std::chrono::milliseconds::max()));
        std::shared_ptr<ClockTicker> pHighResolutionClockTicker(new ClockTicker(std::chrono::milliseconds::max()));
        REQUIRE(!pLowResolutionClockTicker->isEnabled());
        REQUIRE(!pHighResolutionClockTicker->isEnabled());

        WHEN("timer notifier is created with given clock tickers")
        {
            TimerNotifier timerNotifier(pLowResolutionClockTicker, pHighResolutionClockTicker);

            THEN("timer notifier enables low resolution clock ticker")
            {
                REQUIRE(pLowResolutionClockTicker->isEnabled());
                REQUIRE(!pHighResolutionClockTicker->isEnabled());
            }
        }
    }
}


SCENARIO("TimerNotifier disables high resolution clock ticker if wheel with 64ms time span becomes empty")
{

    GIVEN("a timer notifier with no timer on wheel having 64ms time span")
    {
        std::shared_ptr<ClockTicker> pLowResolutionClockTicker(new ClockTicker(std::chrono::milliseconds::max()));
        std::shared_ptr<ClockTicker> pHighResolutionClockTicker(new ClockTicker(std::chrono::milliseconds::max()));
        TimerNotifier timerNotifier(pLowResolutionClockTicker, pHighResolutionClockTicker);

        WHEN("high resolution clock ticker ticks")
        {
            pHighResolutionClockTicker->setEnabled(true);
            REQUIRE(pHighResolutionClockTicker->isEnabled())
            pHighResolutionClockTicker->tick();

            THEN("timer notifier disables high resolution clock ticker")
            {
                REQUIRE(!pHighResolutionClockTicker->isEnabled())
            }
        }

        WHEN("high resolution clock ticker ticks after adding timer on wheel with 64ms time span that will expire after 5ms")
        {
            TimerPrivate timer;
            timer.setInterval(std::chrono::milliseconds(5));
            timerNotifier.addTimer(&timer);
            pHighResolutionClockTicker->setEnabled(true);
            REQUIRE(pHighResolutionClockTicker->isEnabled())
            pHighResolutionClockTicker->tick();

            THEN("timer notifier does not disable high resolution clock ticker")
            {
                REQUIRE(pHighResolutionClockTicker->isEnabled());

                AND_WHEN("high resolution clock ticker ticks more three times")
                {
                    for (auto i = 0; i <= 3; ++i)
                        pHighResolutionClockTicker->tick();

                    THEN("timer notifier does not disable high resolution clock ticker")
                    {
                        REQUIRE(pHighResolutionClockTicker->isEnabled());

                        AND_WHEN("high resolution clock ticker ticks one more time")
                        {
                            pHighResolutionClockTicker->tick();

                            THEN("timer notifier disables high resolution clock ticker")
                            {
                                REQUIRE(!pHighResolutionClockTicker->isEnabled());
                            }
                        }
                    }
                }
            }
        }

        WHEN("a timer that will expire after 5ms is added to wheel with 64ms time span")
        {
            TimerPrivate timer;
            timer.setInterval(std::chrono::milliseconds(5));
            timerNotifier.addTimer(&timer);
            pHighResolutionClockTicker->setEnabled(true);
            REQUIRE(pHighResolutionClockTicker->isEnabled())

            THEN("timer notifier does not disable high resolution clock ticker")
            {
                REQUIRE(pHighResolutionClockTicker->isEnabled());

                AND_WHEN("added timer is removed")
                {
                    timerNotifier.removeTimer(&timer);

                    THEN("timer notifier disables high resolution clock ticker")
                    {
                        REQUIRE(!pHighResolutionClockTicker->isEnabled());
                    }
                }
            }
        }
    }
}


SCENARIO("TimerNotifier emits timedOutTimers for timers having zero interval when control returns to the event loop")
{
    GIVEN("a timer notifier")
    {
        std::shared_ptr<ClockTicker> pLowResolutionClockTicker(new ClockTicker(std::chrono::milliseconds::max()));
        std::shared_ptr<ClockTicker> pHighResolutionClockTicker(new ClockTicker(std::chrono::milliseconds::max()));
        TimerNotifier timerNotifier(pLowResolutionClockTicker, pHighResolutionClockTicker);
        QSemaphore timedOutTimersSignalEmittedSemaphore;
        TimerList expiredTimers;
        Object::connect(&timerNotifier, &TimerNotifier::timedOutTimers, [&](TimerList timers)
        {
            expiredTimers.swap(timers);
            timedOutTimersSignalEmittedSemaphore.release();
        });

        WHEN("timers with zero interval are added to the timer notifier")
        {
            const auto timersWithZeroIntervalToAddToTimerNotifier = GENERATE(AS(size_t), 1, 3, 8);
            std::vector<TimerPrivate> timersWithZeroInterval(timersWithZeroIntervalToAddToTimerNotifier);
            const bool setZeroIntervalExplicitly = GENERATE(AS(bool), true, false);
            if (setZeroIntervalExplicitly)
            {
                for (auto &timer : timersWithZeroInterval)
                    timer.setInterval(std::chrono::milliseconds(0));
            }
            for (auto &timer : timersWithZeroInterval)
            {
                REQUIRE(timer.interval() == std::chrono::milliseconds(0));
                timerNotifier.addTimer(&timer);
            }

            THEN("timer notifier emits timedOutTimers signal when control returns to the event loop")
            {
                REQUIRE(!timedOutTimersSignalEmittedSemaphore.tryAcquire());
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(timedOutTimersSignalEmittedSemaphore, 1));
                std::set<TimerPrivate*> expectedTimers;
                for (auto &timer : timersWithZeroInterval)
                    expectedTimers.insert(&timer);
                std::set<TimerPrivate*> emittedTimers;
                for (auto it = expiredTimers.begin(); it != expiredTimers.end(); ++it)
                    emittedTimers.insert(it.timer());
                REQUIRE(emittedTimers == expectedTimers);
            }
        }
    }
}
