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
#include "Timer.h"
#include "TimerPrivate_epoll.h"
#include <Spectator.h>
#include <QSemaphore>
#include <vector>
#include <set>

using Kourier::TimerNotifier;
using Kourier::Timer;
using Kourier::TimerPrivate;
using Kourier::TimerList;
using Kourier::ClockTicker;
using Kourier::Object;
using Spectator::SemaphoreAwaiter;


namespace Test::TimerNotifier
{
    class TimerNotifierTest
    {
    public:
        inline static TimerPrivate *fetchTimerPrivate(Timer *pTimer) {return pTimer->d_ptr.get();}
    };
}

using namespace Test::TimerNotifier;


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
            }
        }
    }
}


SCENARIO("TimerNotifier disables high resolution clock ticker on creation")
{
    GIVEN("enabled low and high resolution clock tickers")
    {
        std::shared_ptr<ClockTicker> pLowResolutionClockTicker(new ClockTicker(std::chrono::milliseconds::max()));
        std::shared_ptr<ClockTicker> pHighResolutionClockTicker(new ClockTicker(std::chrono::milliseconds::max()));
        pLowResolutionClockTicker->setEnabled(true);
        pHighResolutionClockTicker->setEnabled(true);

        WHEN("timer notifier is created with given clock tickers")
        {
            TimerNotifier timerNotifier(pLowResolutionClockTicker, pHighResolutionClockTicker);

            THEN("timer notifier disables high resolution clock ticker")
            {
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
            Timer timer;
            TimerNotifierTest::fetchTimerPrivate(&timer)->setInterval(std::chrono::milliseconds(5));
            timerNotifier.addTimer(TimerNotifierTest::fetchTimerPrivate(&timer));
            pHighResolutionClockTicker->setEnabled(true);
            REQUIRE(pHighResolutionClockTicker->isEnabled())
            pHighResolutionClockTicker->tick();

            THEN("timer notifier does not disable high resolution clock ticker")
            {
                REQUIRE(pHighResolutionClockTicker->isEnabled());

                AND_WHEN("high resolution clock ticker ticks more three times")
                {
                    for (auto i = 0; i < 3; ++i)
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
            Timer timer;
            pHighResolutionClockTicker->setEnabled(true);
            TimerNotifierTest::fetchTimerPrivate(&timer)->setInterval(std::chrono::milliseconds(5));
            timerNotifier.addTimer(TimerNotifierTest::fetchTimerPrivate(&timer));

            THEN("timer notifier does not disable high resolution clock ticker")
            {
                REQUIRE(pHighResolutionClockTicker->isEnabled());

                AND_WHEN("added timer is removed")
                {
                    timerNotifier.removeTimer(TimerNotifierTest::fetchTimerPrivate(&timer));

                    THEN("timer notifier disables high resolution clock ticker")
                    {
                        REQUIRE(!pHighResolutionClockTicker->isEnabled());
                    }
                }
            }
        }
    }
}


SCENARIO("TimerNotifier times out timers having zero interval when control returns to the event loop")
{
    GIVEN("a timer notifier")
    {
        std::shared_ptr<ClockTicker> pLowResolutionClockTicker(new ClockTicker(std::chrono::milliseconds::max()));
        std::shared_ptr<ClockTicker> pHighResolutionClockTicker(new ClockTicker(std::chrono::milliseconds::max()));
        TimerNotifier timerNotifier(pLowResolutionClockTicker, pHighResolutionClockTicker);

        WHEN("timers with zero interval are added to the timer notifier")
        {
            const auto timersWithZeroIntervalToAddToTimerNotifier = GENERATE(AS(size_t), 1, 3, 8);
            std::vector<Timer> timersWithZeroInterval(timersWithZeroIntervalToAddToTimerNotifier);
            QSemaphore timeoutSemaphore;
            std::set<Timer*> timedOutTimers;
            for (auto &timer : timersWithZeroInterval)
            {
                Object::connect(&timer, &Timer::timeout, [&, pTimer = &timer]()
                {
                    REQUIRE(!timedOutTimers.contains(pTimer))
                    timedOutTimers.insert(pTimer);
                    timeoutSemaphore.release();
                });
            }
            const bool setZeroIntervalExplicitly = GENERATE(AS(bool), true, false);
            if (setZeroIntervalExplicitly)
            {
                for (auto &timer : timersWithZeroInterval)
                    TimerNotifierTest::fetchTimerPrivate(&timer)->setInterval(std::chrono::milliseconds(0));
            }
            for (auto &timer : timersWithZeroInterval)
            {
                REQUIRE(timer.interval() == std::chrono::milliseconds(0));
                timerNotifier.addTimer(TimerNotifierTest::fetchTimerPrivate(&timer));
            }

            THEN("time notifier makes added timers emit timeout when control returns to the event loop")
            {
                REQUIRE(!timeoutSemaphore.tryAcquire());
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(timeoutSemaphore, timersWithZeroIntervalToAddToTimerNotifier, 10));
                std::set<Timer*> expectedTimers;
                for (auto &timer : timersWithZeroInterval)
                    expectedTimers.insert(&timer);
                REQUIRE(timedOutTimers == expectedTimers);
            }
        }
    }
}


SCENARIO("TimerNotifier times out timers when their timeout reaches zero")
{
    GIVEN("a timer notifier")
    {
        std::shared_ptr<ClockTicker> pLowResolutionClockTicker(new ClockTicker(std::chrono::milliseconds::max()));
        std::shared_ptr<ClockTicker> pHighResolutionClockTicker(new ClockTicker(std::chrono::milliseconds::max()));
        TimerNotifier timerNotifier(pLowResolutionClockTicker, pHighResolutionClockTicker);

        WHEN("timers with intervals smaller than 64ms are added to the timer notifier")
        {
            std::vector<Timer> timers(3);
            TimerNotifierTest::fetchTimerPrivate(&timers[0])->setInterval(std::chrono::milliseconds(1));
            TimerNotifierTest::fetchTimerPrivate(&timers[1])->setInterval(std::chrono::milliseconds(32));
            TimerNotifierTest::fetchTimerPrivate(&timers[2])->setInterval(std::chrono::milliseconds(63));
            QSemaphore timeoutSemaphore;
            std::set<Timer*> timedOutTimers;
            for (auto &timer : timers)
            {
                timer.setSingleShot(false);
                timerNotifier.addTimer(TimerNotifierTest::fetchTimerPrivate(&timer));
                Object::connect(&timer, &Timer::timeout, [&, pTimer = &timer]()
                {
                    REQUIRE(!timedOutTimers.contains(pTimer))
                    timedOutTimers.insert(pTimer);
                    timeoutSemaphore.release();
                });
            }

            THEN("time notifier times out timer after high resolution clock ticker ticks until timeout reaches zero")
            {
                for (auto tickCounter = 1; tickCounter < 128; ++tickCounter)
                {
                    pHighResolutionClockTicker->tick();
                    for (auto &timer : timers)
                    {
                        if (tickCounter == timer.interval().count())
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(timeoutSemaphore, 1));
                        }
                    }
                }
                REQUIRE(!timeoutSemaphore.tryAcquire());
                std::set<Timer*> expectedTimers;
                for (auto &timer : timers)
                    expectedTimers.insert(&timer);
                REQUIRE(timedOutTimers == expectedTimers);
            }
        }
    }
}
