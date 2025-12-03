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
#include <stack>

using Kourier::TimerNotifier;
using Kourier::TimerWheel;
using Kourier::TimerList;
using Kourier::Timer;
using Kourier::TimerPrivate;
using Kourier::ClockTicker;
using Kourier::Object;
using Spectator::SemaphoreAwaiter;
using namespace std::chrono_literals;


namespace Test::TimerNotifier
{
    class TimerNotifierTest
    {
    public:
        inline static TimerPrivate *fetchTimerPrivate(Timer *pTimer) {return pTimer->d_ptr.get();}
        inline static void setTimerNotifier(Timer &timer, class TimerNotifier &timerNotifier) {timer.d_ptr->m_pTimerNotifier = &timerNotifier;}
        inline static void setLowResolutionTime(class TimerNotifier &timerNotifer, std::chrono::milliseconds time) {timerNotifer.m_lowResolutionTime = time;}
        inline static TimerWheel &timerWheel(class TimerNotifier &timerNotifier, size_t idx) {REQUIRE(idx < 7); return timerNotifier.m_timerWheels[idx];}
        inline static void increaseLowResolutionTickCounter(class TimerNotifier &timerNotifier, uint64_t tickCount) {timerNotifier.m_lowResolutionTickCounter += tickCount; timerNotifier.m_lowResolutionTime += std::chrono::milliseconds(tickCount << 6);}
        inline static TimerList &timersToNotify(class TimerNotifier &timerNotifier) {return timerNotifier.m_timersToNotify;}
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

        WHEN("high resolution clock ticker ticks after starting timer with 5ms interval")
        {
            Timer timer;
            timer.setSingleShot(true);
            TimerNotifierTest::setTimerNotifier(timer, timerNotifier);
            QSemaphore emittedTimeoutSemaphore;
            Object::connect(&timer, &Timer::timeout, [&](){emittedTimeoutSemaphore.release();});
            timer.start(std::chrono::milliseconds(5));
            pHighResolutionClockTicker->tick();

            THEN("timer notifier does not trigger timer nor disable high resolution clock ticker")
            {
                REQUIRE(!SemaphoreAwaiter::signalSlotAwareWait(emittedTimeoutSemaphore, QDeadlineTimer(1)));
                REQUIRE(pHighResolutionClockTicker->isEnabled());

                AND_WHEN("high resolution clock ticker ticks more three times")
                {
                    for (auto i = 0; i < 3; ++i)
                        pHighResolutionClockTicker->tick();

                    THEN("timer notifier does not trigger timer nor disable high resolution clock ticker")
                    {
                        REQUIRE(!SemaphoreAwaiter::signalSlotAwareWait(emittedTimeoutSemaphore, QDeadlineTimer(1)));
                        REQUIRE(pHighResolutionClockTicker->isEnabled());

                        AND_WHEN("high resolution clock ticker ticks one more time")
                        {
                            pHighResolutionClockTicker->tick();

                            THEN("timer notifier triggers timer and disables high resolution clock ticker")
                            {
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(emittedTimeoutSemaphore, 10));
                                REQUIRE(!pHighResolutionClockTicker->isEnabled());
                                REQUIRE(!SemaphoreAwaiter::signalSlotAwareWait(emittedTimeoutSemaphore, QDeadlineTimer(1)));
                            }
                        }
                    }
                }
            }
        }

        WHEN("a timer with 5ms timeout is added to wheel with 64ms time span")
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
                    REQUIRE(!timedOutTimers.contains(pTimer))
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

        WHEN("timers with intervals up to 64ms are added to the timer notifier")
        {
            std::vector<Timer> timers(3);
            timers[0].setInterval(std::chrono::milliseconds(1));
            timers[1].setInterval(std::chrono::milliseconds(31));
            timers[2].setInterval(std::chrono::milliseconds(64));
            QSemaphore timeoutSemaphore;
            std::set<Timer*> timedOutTimers;
            for (auto &timer : timers)
            {
                timer.setSingleShot(true);
                TimerNotifierTest::setTimerNotifier(timer, timerNotifier);
                Object::connect(&timer, &Timer::timeout, [&, pTimer = &timer]()
                {
                    REQUIRE(!timedOutTimers.contains(pTimer))
                    timedOutTimers.insert(pTimer);
                    timeoutSemaphore.release();
                });
                timer.start();
            }

            THEN("time notifier triggers timer after high resolution clock ticker ticks until timeout reaches zero")
            {
                for (auto tickCounter = 1; tickCounter < 128; ++tickCounter)
                {
                    pHighResolutionClockTicker->tick();
                    for (auto &timer : timers)
                    {
                        if (tickCounter == timer.interval().count())
                        {
                            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(timeoutSemaphore, 10));
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
        const auto msSinceEpochToSet = timerNotifier.lowResolutionTime() - timeAgoInMs;
        TimerNotifierTest::setLowResolutionTime(timerNotifier, msSinceEpochToSet);
        REQUIRE(timerNotifier.lowResolutionTime() == msSinceEpochToSet);

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
            timer.start();

            THEN("timer notifier does not adjust timeout")
            {
                auto *pTimerPrivate = TimerNotifierTest::fetchTimerPrivate(&timer);
                REQUIRE(pTimerPrivate->timeout() == intervalToSet);
                REQUIRE(pTimerPrivate->extraTimeout().count() == 0);
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

        WHEN("a timer with positive interval belonging to the set [65ms, 4096ms[ is added to timer notifier")
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
            const auto msSinceEpochBeforeStarting = TimerNotifier::msSinceEpoch();
            timer.start();
            const auto msSinceEpochAfterStarting = TimerNotifier::msSinceEpoch();

            THEN("timer notifier adjusts timeout")
            {
                auto *pTimerPrivate = TimerNotifierTest::fetchTimerPrivate(&timer);
                const auto timeout = pTimerPrivate->timeout() + pTimerPrivate->extraTimeout();
                bool hasAdjustedTimeout = false;
                for (auto i = msSinceEpochBeforeStarting; i <= msSinceEpochAfterStarting; ++i)
                {
                    const auto expectedAdjustedInterval = intervalToSet + (i - timerNotifier.lowResolutionTime());
                    if (timeout == expectedAdjustedInterval)
                    {
                        hasAdjustedTimeout = true;
                        break;
                    }
                }
                REQUIRE(hasAdjustedTimeout);
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
            const auto msSinceEpochBeforeStarting = TimerNotifier::msSinceEpoch();
            timer.start();
            const auto msSinceEpochAfterStarting = TimerNotifier::msSinceEpoch();

            THEN("timer notifier adjusts timeout")
            {
                auto *pTimerPrivate = TimerNotifierTest::fetchTimerPrivate(&timer);
                const auto timeout = pTimerPrivate->timeout() + pTimerPrivate->extraTimeout();
                bool hasAdjustedTimeout = false;
                for (auto i = msSinceEpochBeforeStarting; i <= msSinceEpochAfterStarting; ++i)
                {
                    const auto expectedAdjustedInterval = intervalToSet + (i - timerNotifier.lowResolutionTime());
                    if (timeout == expectedAdjustedInterval)
                    {
                        hasAdjustedTimeout = true;
                        break;
                    }
                }
                REQUIRE(hasAdjustedTimeout);
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
                            REQUIRE(currentWheelTickCount[i] == wheelTickCount)
                        }
                        else
                        {
                            REQUIRE(++currentWheelTickCount[i] == wheelTickCount)
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

                        AND_THEN("timer is moved either to lowest wheel of directly to timers to notify")
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
                                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(emittedTimeoutSemaphore, QDeadlineTimer(1)));
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
