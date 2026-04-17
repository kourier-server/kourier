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
#include "Core/EpollTimerRegistrar.h"
#include "Core/TimerList.h"
#include "Timer.h"
#include "TimerPrivate_epoll.h"
#include <Spectator.h>
#include <QSemaphore>
#include <QDeadlineTimer>
#include <chrono>
#include <vector>
#include <set>
#include <utility>
#include <stack>

using Kourier::TimerNotifier;
using Kourier::TimerWheel;
using Kourier::TimerList;
using Kourier::Timer;
using Kourier::TimerPrivate;
using Kourier::ClockTicker;
using Kourier::Object;
using namespace std::chrono_literals;
using namespace Spectator;


namespace Test::TimerNotifier
{
    class TimerNotifierTest
    {
    public:
        inline static TimerPrivate *fetchTimerPrivate(Timer *pTimer) {return pTimer->d_ptr.get();}
        inline static void setTimerNotifier(Timer &timer, class TimerNotifier &timerNotifier) {timer.d_ptr->m_pTimerNotifier = &timerNotifier;}
        inline static void setLowResolutionTime(class TimerNotifier &timerNotifer, std::chrono::nanoseconds time) {timerNotifer.m_lowResolutionTime = time;}
        inline static TimerWheel &timerWheel(class TimerNotifier &timerNotifier, size_t idx) {REQUIRE(idx < 7); return timerNotifier.m_timerWheels[idx];}
        inline static void increaseLowResolutionTickCounter(class TimerNotifier &timerNotifier, uint64_t tickCount) {timerNotifier.m_lowResolutionTickCounter += tickCount; timerNotifier.m_lowResolutionTime += std::chrono::milliseconds(tickCount << 6);}
        inline static TimerList &timersToNotify(class TimerNotifier &timerNotifier) {return timerNotifier.m_timersToNotify;}
        inline static std::chrono::nanoseconds highResolutionTime(class TimerNotifier &timerNotifier) {return timerNotifier.m_highResolutionTime;}
    };
}

using namespace Test::TimerNotifier;


SCENARIO("TimerNotifier starts low resolution time to time elapsed since epoch")
{
    GIVEN("a timer notifier")
    {
        std::shared_ptr<ClockTicker> pLowResolutionClockTicker(new ClockTicker(std::chrono::milliseconds::max()));
        std::shared_ptr<ClockTicker> pHighResolutionClockTicker(new ClockTicker(std::chrono::milliseconds::max()));
        const auto timeElapsedSinceEpochBefore = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch());
        TimerNotifier timerNotifier(pLowResolutionClockTicker, pHighResolutionClockTicker);
        const auto timeElapsedSinceEpochAfter = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch());

        WHEN("current low resolution time is fetched from time notifier")
        {
            const auto lowResolutionTime = timerNotifier.lowResolutionTime();

            THEN("timer notifier gives elapsed time since epoch")
            {
                REQUIRE(timeElapsedSinceEpochBefore <= lowResolutionTime && lowResolutionTime <= timeElapsedSinceEpochAfter);
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
            REQUIRE(pHighResolutionClockTicker->isEnabled());
            pHighResolutionClockTicker->tick();

            THEN("timer notifier disables high resolution clock ticker")
            {
                REQUIRE(!pHighResolutionClockTicker->isEnabled());
            }
        }

        WHEN("high resolution clock ticker ticks after starting timer with 5ms interval")
        {
            Timer timer;
            timer.setSingleShot(true);
            TimerNotifierTest::setTimerNotifier(timer, timerNotifier);
            QSemaphore emittedTimeoutSemaphore;
            Object::connect(&timer, &Timer::timeout, [&](){emittedTimeoutSemaphore.release();});
            timer.start(std::chrono::milliseconds(5));
            pHighResolutionClockTicker->tick();

            THEN("timer notifier does not trigger timer nor disables high resolution clock ticker")
            {
                REQUIRE(!TRY_ACQUIRE(emittedTimeoutSemaphore, QDeadlineTimer(1)));
                REQUIRE(pHighResolutionClockTicker->isEnabled());

                AND_WHEN("high resolution clock ticker ticks more three times")
                {
                    for (auto i = 0; i < 3; ++i)
                        pHighResolutionClockTicker->tick();

                    THEN("timer notifier does not trigger timer nor disables high resolution clock ticker")
                    {
                        REQUIRE(!TRY_ACQUIRE(emittedTimeoutSemaphore, QDeadlineTimer(1)));
                        REQUIRE(pHighResolutionClockTicker->isEnabled());

                        AND_WHEN("high resolution clock ticker ticks one more time")
                        {
                            pHighResolutionClockTicker->tick();

                            THEN("timer notifier triggers timer and disables high resolution clock ticker")
                            {
                                REQUIRE(TRY_ACQUIRE(emittedTimeoutSemaphore, 10));
                                REQUIRE(!pHighResolutionClockTicker->isEnabled());
                                REQUIRE(!TRY_ACQUIRE(emittedTimeoutSemaphore, QDeadlineTimer(1)));
                            }
                        }
                    }
                }
            }
        }

        WHEN("high resolution clock ticker ticks after stopping timer with 5ms interval")
        {
            pHighResolutionClockTicker->setEnabled(true);
            Timer timer;
            timer.setSingleShot(true);
            TimerNotifierTest::setTimerNotifier(timer, timerNotifier);
            bool hasTimedOut = false;
            Object::connect(&timer, &Timer::timeout, [&](){hasTimedOut = true;});
            timer.start(std::chrono::milliseconds(5));

            THEN("timer notifier does not disable high resolution clock ticker")
            {
                REQUIRE(pHighResolutionClockTicker->isEnabled());

                AND_WHEN("added timer is stopped")
                {
                    timer.stop();

                    THEN("timer notifier removes timer from 64ms timer wheel making it empty and disables high resolution clock ticker")
                    {
                        REQUIRE(!hasTimedOut);
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

        WHEN("timers with zero interval are started")
        {
            const auto timersWithZeroIntervalToAddToTimerNotifier = GENERATE(AS(size_t), 1, 3, 8);
            std::vector<Timer> timersWithZeroInterval(timersWithZeroIntervalToAddToTimerNotifier);
            QSemaphore timeoutSemaphore;
            std::set<Timer*> timedOutTimers;
            for (auto &timer : timersWithZeroInterval)
            {
                timer.setSingleShot(true);
                TimerNotifierTest::setTimerNotifier(timer, timerNotifier);
                Object::connect(&timer, &Timer::timeout, [&, pTimer = &timer]()
                {
                    REQUIRE(!timedOutTimers.contains(pTimer));
                    timedOutTimers.insert(pTimer);
                    timeoutSemaphore.release();
                });
            }
            const bool setZeroIntervalExplicitly = GENERATE(AS(bool), true, false);
            for (auto &timer : timersWithZeroInterval)
            {
                if (setZeroIntervalExplicitly)
                    timer.setInterval(std::chrono::milliseconds(0));
                REQUIRE(timer.interval() == std::chrono::milliseconds(0));
                timer.start();
            }

            THEN("time notifier triggers added timers when control returns to the event loop")
            {
                REQUIRE(!timeoutSemaphore.tryAcquire());
                REQUIRE(TRY_ACQUIRE(timeoutSemaphore, timersWithZeroIntervalToAddToTimerNotifier, 10));
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

        WHEN("timers with intervals up to 64ms are added to the timer notifier")
        {
            std::vector<std::pair<std::chrono::milliseconds::rep, Timer>> timersInfo(3);
            timersInfo[0].second.setInterval(std::chrono::milliseconds(1));
            timersInfo[1].second.setInterval(std::chrono::milliseconds(31));
            timersInfo[2].second.setInterval(std::chrono::milliseconds(64));
            QSemaphore timeoutSemaphore;
            std::set<Timer*> timedOutTimers;
            for (auto &timerInfo : timersInfo)
            {
                timerInfo.second.setSingleShot(true);
                TimerNotifierTest::setTimerNotifier(timerInfo.second, timerNotifier);
                Object::connect(&timerInfo.second, &Timer::timeout, [&, pTimer = &timerInfo.second]()
                {
                    REQUIRE(!timedOutTimers.contains(pTimer));
                    timedOutTimers.insert(pTimer);
                    timeoutSemaphore.release();
                });
                timerInfo.second.start();
                auto *pTimerPrivate = TimerNotifierTest::fetchTimerPrivate(&timerInfo.second);
                // OS Scheduler may interrupt execution, leaving TimerNotifier's high resolution 
                // time behind current time for more than 1ms, making TimeNotifier add an extra 
                // timeout under these circunstances.
                timerInfo.first = (pTimerPrivate->timeout() + pTimerPrivate->extraTimeout()).count();
            }

            THEN("time notifier triggers timer after high resolution clock ticker ticks until timeout + extra timeout reaches zero")
            {
                size_t maxTickValue = 0;
                for (auto &timerInfo : timersInfo)
                    maxTickValue = timerInfo.first > maxTickValue ? timerInfo.first : maxTickValue;
                for (auto tickCounter = 1; tickCounter < std::max<size_t>(128, maxTickValue + 16); ++tickCounter)
                {
                    pHighResolutionClockTicker->tick();
                    for (auto &timerInfo : timersInfo)
                    {
                        if (tickCounter == timerInfo.first)
                        {
                            REQUIRE(TRY_ACQUIRE(timeoutSemaphore, 10));
                        }
                    }
                }
                REQUIRE(!timeoutSemaphore.tryAcquire());
                std::set<Timer*> expectedTimers;
                for (auto &timerInfo : timersInfo)
                    expectedTimers.insert(&timerInfo.second);
                REQUIRE(timedOutTimers == expectedTimers);
            }
        }
    }
}


SCENARIO("TimerNotifier ajdusts timeout of added timers")
{
    GIVEN("a timer notifier whose low resolution clock ticker ticked some time ago")
    {
        std::shared_ptr<ClockTicker> pLowResolutionClockTicker(new ClockTicker(std::chrono::milliseconds::max()));
        std::shared_ptr<ClockTicker> pHighResolutionClockTicker(new ClockTicker(std::chrono::milliseconds::max()));
        TimerNotifier timerNotifier(pLowResolutionClockTicker, pHighResolutionClockTicker);
        const auto timeAgoInMs = GENERATE(AS(std::chrono::milliseconds),
                                          std::chrono::milliseconds(0),
                                          std::chrono::milliseconds(1),
                                          std::chrono::milliseconds(18),
                                          std::chrono::milliseconds(63));
        const auto nsecsSinceEpochToSet = timerNotifier.lowResolutionTime() - std::chrono::duration_cast<std::chrono::nanoseconds>(timeAgoInMs);
        TimerNotifierTest::setLowResolutionTime(timerNotifier, nsecsSinceEpochToSet);
        REQUIRE(timerNotifier.lowResolutionTime() == nsecsSinceEpochToSet);

        WHEN("a timer with positive interval up to 64ms is started")
        {
            const auto intervalToSet = GENERATE(AS(std::chrono::milliseconds),
                                                std::chrono::milliseconds(1),
                                                std::chrono::milliseconds(5),
                                                std::chrono::milliseconds(23),
                                                std::chrono::milliseconds(64));
            const auto timerType = GENERATE(AS(Timer::TimerType),
                                            Timer::TimerType::Precise,
                                            Timer::TimerType::Coarse,
                                            Timer::TimerType::VeryCoarse);
            Timer timer;
            TimerNotifierTest::setTimerNotifier(timer, timerNotifier);
            timer.setTimerType(timerType);
            timer.setInterval(intervalToSet);
            const auto extraTimeoutDueToHighResolutionTimeBefore = std::chrono::duration_cast<std::chrono::milliseconds>(TimerNotifier::nsecsSinceEpoch() - TimerNotifierTest::highResolutionTime(timerNotifier));
            timer.start();
            const auto extraTimeoutDueToHighResolutionTimeAfter = std::chrono::duration_cast<std::chrono::milliseconds>(TimerNotifier::nsecsSinceEpoch() - TimerNotifierTest::highResolutionTime(timerNotifier));

            THEN("low resolution time does not influence timeout adjustment")
            {
                auto *pTimerPrivate = TimerNotifierTest::fetchTimerPrivate(&timer);
                REQUIRE(pTimerPrivate->timeout() == intervalToSet);
                REQUIRE(pTimerPrivate->extraTimeout().count() == 0
                        || ((extraTimeoutDueToHighResolutionTimeBefore <= pTimerPrivate->extraTimeout()) 
                             && (pTimerPrivate->extraTimeout() <= extraTimeoutDueToHighResolutionTimeAfter)));
            }
        }

        WHEN("a coarse or very coarse timer with positive interval greater or equal to 4096ms is started")
        {
            const auto intervalToSet = GENERATE(AS(std::chrono::milliseconds),
                                                std::chrono::milliseconds(4096),
                                                std::chrono::milliseconds(5003),
                                                std::chrono::milliseconds(uint64_t(1) << 26),
                                                std::chrono::milliseconds(215177));
            const auto timerType = GENERATE(AS(Timer::TimerType),
                                            Timer::TimerType::Coarse,
                                            Timer::TimerType::VeryCoarse);
            Timer timer;
            TimerNotifierTest::setTimerNotifier(timer, timerNotifier);
            timer.setTimerType(timerType);
            timer.setInterval(intervalToSet);
            timer.start();

            THEN("timer notifier does not adjust timeout")
            {
                auto *pTimerPrivate = TimerNotifierTest::fetchTimerPrivate(&timer);
                REQUIRE(pTimerPrivate->timeout() == intervalToSet);
                REQUIRE(pTimerPrivate->extraTimeout().count() == 0);
            }
        }

        WHEN("a timer with positive interval belonging to the interval [65ms, 4096ms[ is added to timer notifier")
        {
            const auto intervalToSet = GENERATE(AS(std::chrono::milliseconds),
                                                std::chrono::milliseconds(65),
                                                std::chrono::milliseconds(72),
                                                std::chrono::milliseconds(1025),
                                                std::chrono::milliseconds(4095));
            const auto timerType = GENERATE(AS(Timer::TimerType),
                                            Timer::TimerType::Precise,
                                            Timer::TimerType::Coarse,
                                            Timer::TimerType::VeryCoarse);
            Timer timer;
            TimerNotifierTest::setTimerNotifier(timer, timerNotifier);
            timer.setTimerType(timerType);
            timer.setInterval(intervalToSet);
            const auto extraTimeoutBefore = std::chrono::duration_cast<std::chrono::milliseconds>(TimerNotifier::nsecsSinceEpoch() - timerNotifier.lowResolutionTime());
            timer.start();
            const auto extraTimeoutAfter = std::chrono::duration_cast<std::chrono::milliseconds>(TimerNotifier::nsecsSinceEpoch() - timerNotifier.lowResolutionTime());

            THEN("timer notifier adjusts timeout")
            {
                const auto extraTimeout = TimerNotifierTest::fetchTimerPrivate(&timer)->extraTimeout();
                REQUIRE(extraTimeoutBefore <= extraTimeout && extraTimeout <= extraTimeoutAfter);
            }
        }


        WHEN("a precise timer with positive interval greater or equal to 4096ms is added to timer notifier")
        {
            const auto intervalToSet = GENERATE(AS(std::chrono::milliseconds),
                                                std::chrono::milliseconds(4096),
                                                std::chrono::milliseconds(5003),
                                                std::chrono::milliseconds(uint64_t(1) << 26),
                                                std::chrono::milliseconds(215177));
            const auto timerType = GENERATE(AS(Timer::TimerType), Timer::TimerType::Precise);
            Timer timer;
            TimerNotifierTest::setTimerNotifier(timer, timerNotifier);
            timer.setTimerType(timerType);
            timer.setInterval(intervalToSet);
            const auto extraTimeoutBefore = std::chrono::duration_cast<std::chrono::milliseconds>(TimerNotifier::nsecsSinceEpoch() - timerNotifier.lowResolutionTime());
            timer.start();
            const auto extraTimeoutAfter = std::chrono::duration_cast<std::chrono::milliseconds>(TimerNotifier::nsecsSinceEpoch() - timerNotifier.lowResolutionTime());

            THEN("timer notifier adjusts timeout")
            {
                const auto extraTimeout = TimerNotifierTest::fetchTimerPrivate(&timer)->extraTimeout();
                REQUIRE(extraTimeoutBefore <= extraTimeout && extraTimeout <= extraTimeoutAfter);
            }
        }
    }
}


SCENARIO("TimerNotifier ticks all timer wheels having a resolution that is a multiple of the elapsed time whenever the low resolution clock ticker ticks")
{
    GIVEN("a time notifier")
    {
        std::shared_ptr<ClockTicker> pLowResolutionClockTicker(new ClockTicker(std::chrono::milliseconds::max()));
        std::shared_ptr<ClockTicker> pHighResolutionClockTicker(new ClockTicker(std::chrono::milliseconds::max()));
        TimerNotifier timerNotifier(pLowResolutionClockTicker, pHighResolutionClockTicker);

        WHEN("low resolution clock ticker ticks")
        {
            THEN("timer notifier ticks all wheels whose resolution are a multiple of the elapsed time")
            {
                const auto initialElapsedTime = GENERATE(AS(uint64_t),
                                                        0,
                                                        (1ull << 6) - 64,
                                                        (1ull << 12) - 64,
                                                        (1ull << 18) - 64,
                                                        (1ull << 24) - 64,
                                                        (1ull << 30) - 64,
                                                        (1ull << 36) - 64,
                                                        (1ull << 42) - 64,
                                                        (1ull << 50) - 64);
                TimerNotifierTest::increaseLowResolutionTickCounter(timerNotifier, initialElapsedTime >> 6);
                uint64_t elapsedTime = initialElapsedTime;
                uint64_t currentWheelTickCount[7] = {0};
                for (auto i = 0; i < 128; ++i)
                {
                    elapsedTime += 64;
                    pLowResolutionClockTicker->tick();
                    for (auto i = 1; i < 7; ++i)
                    {
                        const auto wheelResolution = TimerNotifierTest::timerWheel(timerNotifier, i).resolution().count();
                        const auto wheelTickCount = TimerNotifierTest::timerWheel(timerNotifier, i).tickCount();
                        if (elapsedTime & (wheelResolution - 1))
                        {
                            REQUIRE(currentWheelTickCount[i] == wheelTickCount);
                        }
                        else
                        {
                            REQUIRE(++currentWheelTickCount[i] == wheelTickCount);
                        }
                    }
                }
            }
        }
    }
}


SCENARIO("TimerNotifier moves timer on timer wheels until timer's timeout reaches zero")
{
    GIVEN("a timer notifier")
    {
        std::shared_ptr<ClockTicker> pLowResolutionClockTicker(new ClockTicker(std::chrono::milliseconds::max()));
        std::shared_ptr<ClockTicker> pHighResolutionClockTicker(new ClockTicker(std::chrono::milliseconds::max()));
        TimerNotifier timerNotifier(pLowResolutionClockTicker, pHighResolutionClockTicker);

        WHEN("a timer is added to timer notifier")
        {
            const auto moveToWheel0 = GENERATE(AS(bool), true, false);
            const auto moveToWheel1 = GENERATE(AS(bool), true, false);
            const auto moveToWheel2 = GENERATE(AS(bool), true, false);
            const auto moveToWheel3 = GENERATE(AS(bool), true, false);
            const auto moveToWheel4 = GENERATE(AS(bool), true, false);
            const auto moveToWheel5 = GENERATE(AS(bool), true, false);
            const auto moveToWheel6 = GENERATE(AS(bool), true, false);
            std::chrono::milliseconds interval;
            if (moveToWheel0)
                interval += std::chrono::milliseconds((1ull << 0) << 2);
            if (moveToWheel1)
                interval += std::chrono::milliseconds((1ull << 6) << 2);
            if (moveToWheel2)
                interval += std::chrono::milliseconds((1ull << 12) << 2);
            if (moveToWheel3)
                interval += std::chrono::milliseconds((1ull << 18) << 2);
            if (moveToWheel4)
                interval += std::chrono::milliseconds((1ull << 24) << 2);
            if (moveToWheel5)
                interval += std::chrono::milliseconds((1ull << 30) << 2);
            if (moveToWheel6)
                interval += std::chrono::milliseconds((1ull << 36) << 2);
            Timer timer;
            TimerNotifierTest::setTimerNotifier(timer, timerNotifier);
            timer.setSingleShot(true);
            timer.start(interval);

            THEN("timer notifier adds timer to largest wheel to move or to timers to notify if there are no wheels to move")
            {
                int64_t addedTimerWheelIdx = -1;
                if (moveToWheel6)
                    addedTimerWheelIdx = 6;
                else if (moveToWheel5)
                    addedTimerWheelIdx = 5;
                else if (moveToWheel4)
                    addedTimerWheelIdx = 4;
                else if (moveToWheel3)
                    addedTimerWheelIdx = 3;
                else if (moveToWheel2)
                    addedTimerWheelIdx = 2;
                else if (moveToWheel1)
                    addedTimerWheelIdx = 1;
                else if (moveToWheel0)
                    addedTimerWheelIdx = 0;
                REQUIRE(TimerNotifierTest::timersToNotify(timerNotifier).size() == ((addedTimerWheelIdx >= 0) ? 0 : 1));
                for (auto idx = 0; idx < 7; ++idx)
                {
                    REQUIRE(TimerNotifierTest::timerWheel(timerNotifier, idx).timerCount() == ((idx == addedTimerWheelIdx) ? 1 : 0));
                }

                AND_WHEN("all wheels that timer should be moved to are fetched")
                {
                    std::stack<int64_t> idxOfWheelsToMoveTo;
                    if (moveToWheel1)
                        idxOfWheelsToMoveTo.push(1);
                    if (moveToWheel2)
                        idxOfWheelsToMoveTo.push(2);
                    if (moveToWheel3)
                        idxOfWheelsToMoveTo.push(3);
                    if (moveToWheel4)
                        idxOfWheelsToMoveTo.push(4);
                    if (moveToWheel5)
                        idxOfWheelsToMoveTo.push(5);
                    if (moveToWheel6)
                        idxOfWheelsToMoveTo.push(6);

                    THEN("timer is moved to lower wheel after four ticks of the upper wheel")
                    {
                        while (!idxOfWheelsToMoveTo.empty())
                        {
                            const auto idxTopWheel = idxOfWheelsToMoveTo.top();
                            idxOfWheelsToMoveTo.pop();
                            TimerList expiredTimers;
                            const uint64_t lowResolutionTicksToMoveToLowerWheel = ((1ull << (idxTopWheel * 6)) >> 6);
                            for (auto i = 0; i < 3; ++i)
                            {
                                TimerNotifierTest::increaseLowResolutionTickCounter(timerNotifier, lowResolutionTicksToMoveToLowerWheel - 1);
                                pLowResolutionClockTicker->tick();
                                REQUIRE(TimerNotifierTest::timersToNotify(timerNotifier).size() == 0);
                                for (auto idx = 0; idx < 7; ++idx)
                                {
                                    REQUIRE(TimerNotifierTest::timerWheel(timerNotifier, idx).timerCount() == ((idx == idxTopWheel) ? 1 : 0));
                                }
                            }
                            TimerNotifierTest::increaseLowResolutionTickCounter(timerNotifier, lowResolutionTicksToMoveToLowerWheel - 1);
                            REQUIRE(TimerNotifierTest::timerWheel(timerNotifier, idxTopWheel).timerCount() == 1);
                            pLowResolutionClockTicker->tick();
                            REQUIRE(TimerNotifierTest::timerWheel(timerNotifier, idxTopWheel).timerCount() == 0);
                        }

                        AND_THEN("timer is moved either to lowest wheel or directly to timers to notify")
                        {
                            if (moveToWheel0)
                            {
                                REQUIRE(TimerNotifierTest::timersToNotify(timerNotifier).size() == 0);
                                REQUIRE(TimerNotifierTest::timerWheel(timerNotifier, 0).timerCount() == 1);
                                for (auto idx = 1; idx < 7; ++idx)
                                {
                                    REQUIRE(TimerNotifierTest::timerWheel(timerNotifier, idx).timerCount() == 0);
                                }
                                auto * const pTimer = TimerNotifierTest::fetchTimerPrivate(&timer);
                                REQUIRE(1 <= pTimer->timeout().count() && pTimer->timeout().count() <= 64);
                                for (auto i = 0; i < (pTimer->timeout().count() - 1); ++i)
                                {
                                    pHighResolutionClockTicker->tick();
                                    REQUIRE(TimerNotifierTest::timersToNotify(timerNotifier).size() == 0);
                                    REQUIRE(TimerNotifierTest::timerWheel(timerNotifier, 0).timerCount() == 1);
                                    for (auto idx = 1; idx < 7; ++idx)
                                    {
                                        REQUIRE(TimerNotifierTest::timerWheel(timerNotifier, idx).timerCount() == 0);
                                    }
                                }
                                QSemaphore emittedTimeoutSemaphore;
                                Object::connect(&timer, &Timer::timeout, [&](){emittedTimeoutSemaphore.release();});
                                pHighResolutionClockTicker->tick();
                                for (auto idx = 0; idx < 7; ++idx)
                                {
                                    REQUIRE(TimerNotifierTest::timerWheel(timerNotifier, idx).timerCount() == 0);
                                }
                                REQUIRE(TimerNotifierTest::timersToNotify(timerNotifier).size() == 1);
                                REQUIRE(TRY_ACQUIRE(emittedTimeoutSemaphore, QDeadlineTimer(1)));
                                REQUIRE(TimerNotifierTest::timersToNotify(timerNotifier).size() == 0);
                            }
                            else
                            {
                                for (auto idx = 0; idx < 7; ++idx)
                                {
                                    REQUIRE(TimerNotifierTest::timerWheel(timerNotifier, idx).timerCount() == 0);
                                }
                                REQUIRE(TimerNotifierTest::timersToNotify(timerNotifier).size() == 1);
                            }
                        }
                    }
                }
            }
        }
    }
}
