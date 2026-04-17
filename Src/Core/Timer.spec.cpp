//
// Copyright (C) 2023 Glauco Pacheco <glauco@kourier.io>
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

#include "Timer.h"
#include <QCoreApplication>
#include <QThread>
#include <QSemaphore>
#include <QElapsedTimer>
#include <QDeadlineTimer>
#include <chrono>
#include <cmath>
#include <Spectator>

using Kourier::Timer;
using Kourier::Object;
using namespace std::chrono_literals;


SCENARIO("Timer times out within 1ms of error")
{
    GIVEN("a timer set to expire")
    {
        Timer timer;
        const auto intervalInMSecs = GENERATE(AS(std::chrono::milliseconds), 0ms, 3ms, 8ms);
        QSemaphore semaphore;
        QElapsedTimer elapsedTimer;
        qint64 elapsedTimeInMSecs = 0;
        Object::connect(&timer, &Timer::timeout, [&elapsedTimeInMSecs, &elapsedTimer, &semaphore]()
        {
            elapsedTimeInMSecs = elapsedTimer.elapsed();
            semaphore.release();
        });
        elapsedTimer.start();
        timer.start(intervalInMSecs);

        WHEN("we wait until timer expires")
        {
            REQUIRE(TRY_ACQUIRE(semaphore, 10));

            THEN("timer timeout after given interval with max error of 1ms")
            {
                REQUIRE(std::abs(intervalInMSecs.count() - elapsedTimeInMSecs) <= 1);
            }
        }
    }
}


SCENARIO("Active Timer reschedules it's timeout upon changes to the timer's interval")
{
    GIVEN("a single-shot timer with a given interval")
    {
        Timer timer;
        const auto intervalInMSecs = GENERATE(AS(std::chrono::milliseconds), 0ms, 3ms, 8ms);
        timer.setInterval(intervalInMSecs);
        timer.setSingleShot(true);
        qint64 expirationCount = 0;
        QElapsedTimer elapsedTimer;
        qint64 elapsedTimeInMSecs = 0;
        QSemaphore semaphore;
        Object::connect(&timer, &Timer::timeout, [&expirationCount, &elapsedTimeInMSecs, &elapsedTimer, &semaphore]()
        {
            ++expirationCount;
            elapsedTimeInMSecs = elapsedTimer.elapsed();
            semaphore.release();
        });

        WHEN("timer is started")
        {
            elapsedTimer.start();
            timer.start();

            THEN("timer does not emit timeout")
            {
                REQUIRE(0 == expirationCount);

                AND_WHEN("we wait until timer expires after timer interval is changed")
                {
                    const auto newIntervalInMSecs = GENERATE(AS(std::chrono::milliseconds), 0ms, 3ms, 8ms);
                    elapsedTimer.start();
                    timer.setInterval(newIntervalInMSecs);
                    REQUIRE(newIntervalInMSecs == timer.interval());
                    REQUIRE(TRY_ACQUIRE(semaphore, 10));

                    THEN("timer timeout after changed interval")
                    {
                        REQUIRE(std::abs(newIntervalInMSecs.count() - elapsedTimeInMSecs) <= 1);
                    }
                }
            }
        }
    }

    GIVEN("a single-shot timer with an interval of 5ms")
    {
        Timer timer;
        const auto intervalInMSecs = 5ms;
        timer.setInterval(intervalInMSecs);
        timer.setSingleShot(true);
        qint64 expirationCount = 0;
        QElapsedTimer elapsedTimer;
        qint64 elapsedTimeInMSecs = 0;
        QSemaphore semaphore;
        Object::connect(&timer, &Timer::timeout, [&expirationCount, &elapsedTimeInMSecs, &elapsedTimer, &semaphore]()
        {
            ++expirationCount;
            elapsedTimeInMSecs = elapsedTimer.elapsed();
            semaphore.release();
        });

        WHEN("timer is started and we wait for 2ms")
        {
            elapsedTimer.start();
            timer.start();
            QThread::msleep(2);
            QCoreApplication::processEvents();

            THEN("timer does not emit timeout")
            {
                REQUIRE(0 == expirationCount);

                AND_WHEN("we wait until timer expires after timer interval is changed")
                {
                    elapsedTimer.start();
                    timer.setInterval(intervalInMSecs);
                    REQUIRE(intervalInMSecs == timer.interval());
                    REQUIRE(TRY_ACQUIRE(semaphore, 10));

                    THEN("timer timeout after given interval with 1ms max error")
                    {
                        REQUIRE(std::abs(intervalInMSecs.count() - elapsedTimeInMSecs) <= 1);
                    }
                }
            }
        }
    }
}


SCENARIO("Active Timer reschedules it's timeout if timer is started again")
{
    GIVEN("a single-shot timer with a given interval")
    {
        Timer timer;
        const auto intervalInMSecs = GENERATE(AS(std::chrono::milliseconds), 0ms, 3ms, 8ms);
        timer.setInterval(intervalInMSecs);
        timer.setSingleShot(true);
        qint64 expirationCount = 0;
        QElapsedTimer elapsedTimer;
        qint64 elapsedTimeInMSecs = 0;
        QSemaphore semaphore;
        Object::connect(&timer, &Timer::timeout, [&expirationCount, &elapsedTimeInMSecs, &elapsedTimer, &semaphore]()
        {
            ++expirationCount;
            elapsedTimeInMSecs = elapsedTimer.elapsed();
            semaphore.release();
        });

        WHEN("timer is started")
        {
            elapsedTimer.start();
            timer.start();

            THEN("timer does not emit timeout")
            {
                REQUIRE(0 == expirationCount);

                AND_WHEN("we wait until timer expires after starting timer again")
                {
                    const auto newIntervalInMSecs = GENERATE(AS(std::chrono::milliseconds), 0ms, 3ms, 8ms);
                    elapsedTimer.start();
                    timer.start(newIntervalInMSecs);
                    REQUIRE(newIntervalInMSecs == timer.interval());
                    REQUIRE(TRY_ACQUIRE(semaphore, 10));

                    THEN("timer timeout after given interval")
                    {
                        REQUIRE(std::abs(newIntervalInMSecs.count() - elapsedTimeInMSecs) <= 1);
                    }
                }
            }
        }
    }


    GIVEN("a single-shot timer with an interval of 5ms")
    {
        Timer timer;
        const auto intervalInMSecs = 5ms;
        timer.setInterval(intervalInMSecs);
        timer.setSingleShot(true);
        qint64 expirationCount = 0;
        QElapsedTimer elapsedTimer;
        qint64 elapsedTimeInMSecs = 0;
        QSemaphore semaphore;
        Object::connect(&timer, &Timer::timeout, [&expirationCount, &elapsedTimeInMSecs, &elapsedTimer, &semaphore]()
        {
            ++expirationCount;
            elapsedTimeInMSecs = elapsedTimer.elapsed();
            semaphore.release();
        });

        WHEN("timer is started and we wait for 2ms")
        {
            elapsedTimer.start();
            timer.start();
            QThread::msleep(2);
            QCoreApplication::processEvents();

            THEN("timer does not emit timeout")
            {
                REQUIRE(0 == expirationCount);

                AND_WHEN("we wait until timer expires after starting timer")
                {
                    elapsedTimer.start();
                    timer.start();
                    REQUIRE(intervalInMSecs == timer.interval());
                    REQUIRE(TRY_ACQUIRE(semaphore, 10));

                    THEN("timer timeout after given interval")
                    {
                        REQUIRE(std::abs(intervalInMSecs.count() - elapsedTimeInMSecs) <= 1);
                    }
                }
            }
        }
    }
}


SCENARIO("Active Timer does not emit timeout if it is stopped")
{
    GIVEN("an active timer with a given interval")
    {
        Timer timer;
        const auto intervalInMSecs = GENERATE(AS(std::chrono::milliseconds), 0ms, 3ms, 8ms);
        timer.setInterval(intervalInMSecs);
        timer.setSingleShot(false);
        Object::connect(&timer, &Timer::timeout, [](){Spectator::FAIL("This code is supposed to be unreachable.");});
        timer.start();

        WHEN("timer is stopped")
        {
            timer.stop();

            THEN("timer does not emit timeout signal")
            {
                QSemaphore semaphore;
                REQUIRE(!TRY_ACQUIRE(semaphore, QDeadlineTimer(intervalInMSecs.count() + 2)));
            }
        }
    }
}


SCENARIO("Expired Timer does not emit timeout if it is stopped")
{
    GIVEN("an expired timer with a given interval")
    {
        Timer timer;
        const auto intervalInMSecs = GENERATE(AS(std::chrono::milliseconds), 0ms, 3ms, 8ms);
        timer.setInterval(intervalInMSecs);
        timer.setSingleShot(false);
        Object::connect(&timer, &Timer::timeout, [](){Spectator::FAIL("This code is supposed to be unreachable.");});
        timer.start();
        QThread::msleep(intervalInMSecs.count() + 2);

        WHEN("timer is stopped")
        {
            timer.stop();

            THEN("timer does not emit timeout signal when control returns to event loop")
            {
                QSemaphore semaphore;
                REQUIRE(!TRY_ACQUIRE(semaphore, QDeadlineTimer(intervalInMSecs.count() + 2)));
            }
        }
    }
}


SCENARIO("Expired Timer emits timeout when control returns to the event loop")
{
    GIVEN("a timer")
    {
        const auto isSingleShot = GENERATE(AS(bool), true, false);
        const auto interval = GENERATE(AS(std::chrono::milliseconds), 3ms, 8ms);
        const auto setInterval = GENERATE(AS(bool), true, false);
        const auto setIntervalWhenStarting = GENERATE(AS(bool), true, false);
        Timer timer;
        int timeoutEmissionCounter = 0;
        QSemaphore emittedTimeoutSemaphore;
        Object::connect(&timer, &Timer::timeout, [&](){++timeoutEmissionCounter; emittedTimeoutSemaphore.release();});
        timer.setSingleShot(isSingleShot);
        if (setIntervalWhenStarting)
        {
            if (setInterval)
                timer.setInterval(interval);
            timer.start(interval);
        }
        else
        {
            timer.setInterval(interval);
            timer.start();
        }

        WHEN("control returns to the event loop after timer expires")
        {
            const auto timeInMSecsToExpire = interval.count() + 2;
            QCoreApplication::processEvents();
            REQUIRE(0 == timeoutEmissionCounter);
            QThread::msleep(timeInMSecsToExpire);

            THEN("timer emits timeout")
            {
                REQUIRE(TRY_ACQUIRE(emittedTimeoutSemaphore, 10));
                REQUIRE(1 == timeoutEmissionCounter);
            }
        }
    }
}


SCENARIO("Expired Timer does not emit timeout when restarted")
{
    GIVEN("a timer")
    {
        const auto isSingleShot = GENERATE(AS(bool), true, false);
        const auto interval = GENERATE(AS(std::chrono::milliseconds), 3ms, 8ms);
        const auto setInterval = GENERATE(AS(bool), true, false);
        const auto setIntervalWhenStarting = GENERATE(AS(bool), true, false);
        Timer timer;
        int timeoutEmissionCounter = 0;
        QSemaphore emittedTimeoutSemaphore;
        Object::connect(&timer, &Timer::timeout, [&](){++timeoutEmissionCounter; emittedTimeoutSemaphore.release();});
        timer.setSingleShot(isSingleShot);
        if (setIntervalWhenStarting)
        {
            if (setInterval)
                timer.setInterval(interval);
            timer.start(interval);
        }
        else
        {
            timer.setInterval(interval);
            timer.start();
        }

        WHEN("timer is restarted before control returns to the event loop")
        {
            const auto timeInMSecsToExpire = interval.count() + 2;
            QThread::msleep(timeInMSecsToExpire);
            timer.start(interval);

            THEN("timer does not emit timeout")
            {
                const auto timeInMSecsBeforeExpire = std::max(0ll, interval.count() - 1ll);
                REQUIRE(!TRY_ACQUIRE(emittedTimeoutSemaphore, QDeadlineTimer(timeInMSecsBeforeExpire)));
                REQUIRE(timeoutEmissionCounter == 0);
            }
        }
    }
}


SCENARIO("Single-shot Timer emits timeout only once")
{
    GIVEN("a single shot timer")
    {
        const auto interval = GENERATE(AS(std::chrono::milliseconds), 3ms, 5ms);
        const auto setInterval = GENERATE(AS(bool), true, false);
        const auto setIntervalWhenStarting = GENERATE(AS(bool), true, false);
        Timer timer;
        int timeoutEmissionCounter = 0;
        QSemaphore emittedTimeoutSemaphore;
        Object::connect(&timer, &Timer::timeout, [&](){++timeoutEmissionCounter; emittedTimeoutSemaphore.release();});
        timer.setSingleShot(true);
        if (setIntervalWhenStarting)
        {
            if (setInterval)
                timer.setInterval(interval);
            timer.start(interval);
        }
        else
        {
            timer.setInterval(interval);
            timer.start();
        }

        WHEN("control returns to the event loop after timer expires")
        {
            const auto timeInMSecsToExpire = interval.count() + 2;
            QCoreApplication::processEvents();
            REQUIRE(0 == timeoutEmissionCounter);
            QThread::msleep(timeInMSecsToExpire);
            REQUIRE(TRY_ACQUIRE(emittedTimeoutSemaphore, 10));

            THEN("timer emits timeout")
            {
                REQUIRE(1 == timeoutEmissionCounter);

                AND_WHEN("control returns to the event loop and stays there for an interval greater than the timer's interval")
                {
                    REQUIRE(!TRY_ACQUIRE(emittedTimeoutSemaphore, QDeadlineTimer(timeInMSecsToExpire)));

                    THEN("timer does not emit timeout again")
                    {    
                        REQUIRE(1 == timeoutEmissionCounter);
                    }
                }
            }
        }
    }
}


SCENARIO("Timer with 0 interval expires once, despite it's single shot status")
{
    GIVEN("a timer with zero interval")
    {
        Timer timer;
        REQUIRE(timer.interval().count() == 0);
        const bool isSingleShot = GENERATE(AS(bool), true, false);
        timer.setSingleShot(isSingleShot);

        WHEN("timer is started")
        {
            bool hasEmittedTimeout = false;
            QSemaphore emittedTimeoutSemaphore;
            Object::connect(&timer, &Timer::timeout, [&]()
            {
                REQUIRE(!hasEmittedTimeout);
                hasEmittedTimeout = true;
                emittedTimeoutSemaphore.release();
            });
            timer.start();

            THEN("timer emits timeout only once")
            {
                REQUIRE(TRY_ACQUIRE(emittedTimeoutSemaphore, 10));
                REQUIRE(!TRY_ACQUIRE(emittedTimeoutSemaphore, QDeadlineTimer(2)));
            }
        }
        
    }
}


SCENARIO("Precise Timer does not suffer drifts from user sleeps")
{
    GIVEN("a timer to expire in 3ms")
    {
        Timer timer;
        timer.setTimerType(Timer::TimerType::Precise);
        QSemaphore emittedTimeoutSemaphore;
        QElapsedTimer elapsedTimer;
        qint64 elapsedTimeInMSecs = 0;
        Object::connect(&timer, &Timer::timeout, [&](){elapsedTimeInMSecs = elapsedTimer.elapsed(); emittedTimeoutSemaphore.release();});
        timer.start(3ms);

        WHEN("user sleeps for 5ms before restarting the timer")
        {
            QThread::msleep(5);
            elapsedTimer.start();
            timer.start();

            THEN("returning to the event loop and processing the 5ms slept does not trigger the restarted timer with 3ms interval")
            {
                REQUIRE(!TRY_ACQUIRE(emittedTimeoutSemaphore, 2ms));

                AND_WHEN("we wait until timer times out")
                {
                    REQUIRE(TRY_ACQUIRE(emittedTimeoutSemaphore, 10));

                    THEN("timer expires in 3ms")
                    {
                        REQUIRE(std::abs(elapsedTimeInMSecs - 3ll) <= 1);
                    }
                }
            }
        }
    }
}


SCENARIO("Non single-shot Timer emits timeout repeatedly")
{
    GIVEN("a non single shot timer")
    {
        const auto interval = GENERATE(AS(std::chrono::milliseconds), 1ms, 3ms, 5ms);
        const auto changeInterval = GENERATE(AS(bool), true, false);
        const auto newInterval = GENERATE(AS(std::chrono::milliseconds), 0ms, 1ms, 4ms);
        Timer timer;
        timer.setInterval(interval);
        int timeoutEmissionCounter = 0;
        QSemaphore emittedTimeoutSemaphore;
        QElapsedTimer elapsedTimer;
        qint64 elapsedTime = 0;
        Object::connect(&timer, &Timer::timeout, 
            [&]()
            {
                ++timeoutEmissionCounter;
                elapsedTime = elapsedTimer.elapsed();
                emittedTimeoutSemaphore.release();
            });
        timer.setSingleShot(false);

        WHEN("timer is started")
        {
            elapsedTimer.start();
            timer.start();

            THEN("timer times out")
            {
                REQUIRE(TRY_ACQUIRE(emittedTimeoutSemaphore, 10));
                REQUIRE(timeoutEmissionCounter == 1);
                REQUIRE(std::abs(elapsedTime - interval.count()) <= 1);
                elapsedTime = 0;
                elapsedTimer.start();

                AND_WHEN("we wait for another timeout possibly changing interval")
                {
                    if (changeInterval)
                        timer.setInterval(newInterval);

                    THEN("timer emits timeout")
                    {
                        REQUIRE(TRY_ACQUIRE(emittedTimeoutSemaphore, 10));
                        REQUIRE(timeoutEmissionCounter == 2);
                        REQUIRE(std::abs(elapsedTime - (changeInterval ? newInterval.count() : interval.count())) <= 1);
                    }
                }
            }
        }
    }
}


SCENARIO("Changing active Timer to single shot makes it emit timeout signal only once more")
{
    GIVEN("a timer with given interval")
    {
        const auto interval = GENERATE(AS(std::chrono::milliseconds), 1ms, 3ms);
        const auto changeInterval = GENERATE(AS(bool), true, false);
        const auto newInterval = GENERATE(AS(std::chrono::milliseconds), 0ms, 2ms, 4ms);
        Timer timer;
        timer.setInterval(interval);
        int timeoutEmissionCounter = 0;
        QSemaphore emittedTimeoutSemaphore;
        QElapsedTimer elapsedTimer;
        qint64 elapsedTime = 0;
        Object::connect(&timer, &Timer::timeout,
            [&]()
            {
                ++timeoutEmissionCounter;
                elapsedTime = elapsedTimer.elapsed();
                emittedTimeoutSemaphore.release();
            });
        timer.setSingleShot(false);

        WHEN("timer is started")
        {
            elapsedTimer.start();
            timer.start();

            THEN("timer times out repeatedly")
            {
                for (auto i = 1; i <= 3; ++i)
                {
                    REQUIRE(TRY_ACQUIRE(emittedTimeoutSemaphore, 10));
                    REQUIRE(timeoutEmissionCounter == i);
                    REQUIRE(std::abs(elapsedTime - interval.count()) <= 1);
                    elapsedTime = 0;
                    elapsedTimer.start();
                }

                AND_WHEN("Timer is set as single shot while possibly changing interval")
                {
                    timer.setSingleShot(true);
                    if (changeInterval)
                        timer.setInterval(newInterval);

                    THEN("timer emits timeout only once more")
                    {
                        REQUIRE(TRY_ACQUIRE(emittedTimeoutSemaphore, 10));
                        REQUIRE(timeoutEmissionCounter == 4);
                        REQUIRE(std::abs(elapsedTime - (changeInterval ? newInterval.count() : interval.count())) <= 1);
                        REQUIRE(!TRY_ACQUIRE(emittedTimeoutSemaphore, QDeadlineTimer((changeInterval ? newInterval.count() : interval.count()) + 1)));
                    }
                }
            }
        }
    }
}


SCENARIO("Changing expired Timer to single shot makes it emit timeout signal only once more")
{
    GIVEN("a timer with given interval")
    {
        Timer timer;
        const auto interval = GENERATE(AS(std::chrono::milliseconds), 1ms, 3ms);
        timer.setInterval(interval);
        timer.setSingleShot(false);
        int timeoutEmissionCounter = 0;
        QSemaphore emittedTimeoutSemaphore;
        QElapsedTimer elapsedTimer;
        qint64 elapsedTime = 0;
        Object::connect(&timer, &Timer::timeout,
            [&]()
            {
                ++timeoutEmissionCounter;
                elapsedTime = elapsedTimer.elapsed();
                emittedTimeoutSemaphore.release();
            });
        elapsedTimer.start();
        timer.start();

        WHEN("timer is started")
        {
            elapsedTimer.start();
            timer.start();

            THEN("timer times out repeatedly")
            {
                for (auto i = 1; i <= 3; ++i)
                {
                    REQUIRE(TRY_ACQUIRE(emittedTimeoutSemaphore, 10));
                    REQUIRE(timeoutEmissionCounter == i);
                    REQUIRE(std::abs(elapsedTime - interval.count()) <= 1);
                    elapsedTime = 0;
                    elapsedTimer.start();
                }

                AND_WHEN("Timer is set as single shot after user sleeps until timer expires")
                {
                    QThread::msleep(interval.count() + 1);
                    timer.setSingleShot(true);

                    THEN("timer emits timeout only once more")
                    {
                        REQUIRE(TRY_ACQUIRE(emittedTimeoutSemaphore, 10));
                        REQUIRE(timeoutEmissionCounter == 4);
                        REQUIRE(std::abs(elapsedTime - interval.count()) <= 1);
                        REQUIRE(!TRY_ACQUIRE(emittedTimeoutSemaphore, QDeadlineTimer(interval.count() + 1)));
                    }
                }
            }
        }
    }
}


SCENARIO("Changing active Timer to non single-shot makes it emit timeout repeatedly")
{
    GIVEN("a timer with given interval")
    {
        Timer timer;
        const auto interval = GENERATE(AS(std::chrono::milliseconds), 1ms, 3ms);
        timer.setInterval(interval);
        timer.setSingleShot(true);
        int timeoutEmissionCounter = 0;
        QSemaphore emittedTimeoutSemaphore;
        QElapsedTimer elapsedTimer;
        qint64 elapsedTime = 0;
        Object::connect(&timer, &Timer::timeout,
            [&]()
            {
                ++timeoutEmissionCounter;
                elapsedTime = elapsedTimer.elapsed();
                emittedTimeoutSemaphore.release();
            });
        elapsedTimer.start();
        timer.start();

        WHEN("timer is set to non single-shot before time expires")
        {
            timer.setSingleShot(false);

            THEN("timeout is emitted repeatedly")
            {
                for (auto i = 1; i <= 3; ++i)
                {
                    REQUIRE(TRY_ACQUIRE(emittedTimeoutSemaphore, 10));
                    REQUIRE(timeoutEmissionCounter == i);
                    REQUIRE(std::abs(elapsedTime - interval.count()) <= 1);
                    elapsedTime = 0;
                    elapsedTimer.start();
                }
            }
        }
    }
}


SCENARIO("Changing expired Timer to non single-shot makes it emit timeout repeatedly")
{
    GIVEN("an expired timer with given interval")
    {
        Timer timer;
        const auto interval = GENERATE(AS(std::chrono::milliseconds), 1ms, 3ms);
        timer.setInterval(interval);
        timer.setSingleShot(true);
        QSemaphore emittedTimeoutSemaphore;
        Object::connect(&timer, &Timer::timeout, [&](){emittedTimeoutSemaphore.release();});
        timer.start();
        QThread::msleep(interval.count() + 1);

        WHEN("timer is set to non single shot before timeout is emitted")
        {
            timer.setSingleShot(false);

            THEN("timeout is emitted repeatedly")
            {
                REQUIRE(TRY_ACQUIRE(emittedTimeoutSemaphore, 3, 10));
            }
        }
    }
}


SCENARIO("Active timers can be deleted before expiration")
{
    GIVEN("four single-shot timers with same interval")
    {
        std::unique_ptr<Timer> pTimers[4] = {std::make_unique<Timer>(),
                                             std::make_unique<Timer>(),
                                             std::make_unique<Timer>(),
                                             std::make_unique<Timer>()};
        QSemaphore semaphore;
        const auto interval = GENERATE(AS(std::chrono::milliseconds), 0ms, 3ms);
        for (auto &pTimer : pTimers)
        {
            pTimer->setSingleShot(true);
            pTimer->start(interval);
            Object::connect(pTimer.get(), &Timer::timeout, [&semaphore](){semaphore.release();});
        }

        WHEN("timers are deleted")
        {
            const auto timersToDelete = GENERATE_RANGE(AS(size_t), 0, 4);
            size_t deletedTimers = 0;
            for (auto &pTimer : pTimers)
            {
                if (++deletedTimers <= timersToDelete)
                    delete pTimer.release();
                else
                    break;
            }

            THEN("non-deleted timers emit timeout")
            {
                const auto remainingTimers = 4 - timersToDelete;
                for (auto i = 0; i < remainingTimers; ++i)
                {
                    REQUIRE(TRY_ACQUIRE(semaphore, 10));
                }
                REQUIRE(!TRY_ACQUIRE(semaphore, QDeadlineTimer(interval.count() + 1)));
            }
        }
    }
}


SCENARIO("Expired timers can be deleted before emitting timeout")
{
    GIVEN("four expired single-shot timers with same interval")
    {
        std::unique_ptr<Timer> pTimers[4] = {std::make_unique<Timer>(),
                                             std::make_unique<Timer>(),
                                             std::make_unique<Timer>(),
                                             std::make_unique<Timer>()};
        QSemaphore semaphore;
        const auto interval = GENERATE(AS(std::chrono::milliseconds), 0ms, 3ms);
        for (auto &pTimer : pTimers)
        {
            pTimer->setSingleShot(true);
            pTimer->start(interval);
            Object::connect(pTimer.get(), &Timer::timeout, [&semaphore](){semaphore.release();});
        }
        QThread::msleep(interval.count() + 1);

        WHEN("timers are deleted")
        {
            const auto timersToDelete = GENERATE_RANGE(AS(size_t), 0, 4);
            size_t deletedTimers = 0;
            for (auto &pTimer : pTimers)
            {
                if (++deletedTimers <= timersToDelete)
                    delete pTimer.release();
                else
                    break;
            }

            THEN("non-deleted timers emit timeout")
            {
                const auto remainingTimers = 4 - timersToDelete;
                for (auto i = 0; i < remainingTimers; ++i)
                {
                    REQUIRE(TRY_ACQUIRE(semaphore, 10));
                }
                REQUIRE(!TRY_ACQUIRE(semaphore, QDeadlineTimer(interval.count() + 1)));
            }
        }
    }
}


SCENARIO("Timers can be deleted while being processed after expiration")
{
    GIVEN("four single-shot timers with same interval")
    {
        std::unique_ptr<Timer> pTimers[4] = {std::make_unique<Timer>(),
                                             std::make_unique<Timer>(),
                                             std::make_unique<Timer>(),
                                             std::make_unique<Timer>()};
        const auto interval = GENERATE(AS(std::chrono::milliseconds), 0ms, 3ms);
        for (auto &pTimer : pTimers)
        {
            pTimer->setSingleShot(true);
            pTimer->start(interval);
        }
        QSemaphore semaphore;
        bool firstRun = true;

        WHEN("first timer to emit timeout deletes one of the others")
        {
            for (auto &pTimer : pTimers)
            {
                Object::connect(pTimer.get(), &Timer::timeout, [&pTimers, ptrTimer = &pTimer, &firstRun, &semaphore]()
                {
                    if (firstRun)
                    {
                        firstRun = false;
                        for (auto &it : pTimers)
                        {
                            if (it.get() == ptrTimer->get())
                                continue;
                            else
                            {
                                delete it.release();
                                break;
                            }
                        }
                    }
                    semaphore.release();
                });
            }

            THEN("remaining two emit timeout")
            {
                REQUIRE(TRY_ACQUIRE(semaphore, 3, 10));
                REQUIRE(!TRY_ACQUIRE(semaphore, QDeadlineTimer(interval.count() + 1)));
            }
        }

        WHEN("first timer to emit timeout deletes all of the others")
        {
            for (auto &pTimer : pTimers)
            {
                Object::connect(pTimer.get(), &Timer::timeout, [this, &pTimers, ptrTimer = &pTimer, &firstRun, &semaphore]()
                {
                    REQUIRE(firstRun);
                    firstRun = false;
                    for (auto &it : pTimers)
                    {
                        if (it.get() == ptrTimer->get())
                            continue;
                        else
                            delete it.release();
                    }
                    semaphore.release();
                });
            }

            THEN("timeout is emitted only once")
            {
                REQUIRE(TRY_ACQUIRE(semaphore, 10));
                REQUIRE(!TRY_ACQUIRE(semaphore, QDeadlineTimer(interval.count() + 1)));
            }
        }

        WHEN("first timer to emit timeout add a new Timer before deleting all of the others")
        {
            std::unique_ptr<Timer> addedTimer;

            for (auto &pTimer : pTimers)
            {
                Object::connect(pTimer.get(), &Timer::timeout, [&]()
                {
                    addedTimer.reset(new Timer);
                    Object::connect(addedTimer.get(), &Timer::timeout, [&firstRun, &semaphore](){semaphore.release();});
                    addedTimer->setSingleShot(true);
                    addedTimer->start(interval);
                    REQUIRE(firstRun);
                    firstRun = false;
                    for (auto &it : pTimers)
                    {
                        if (it.get() == pTimer.get())
                            continue;
                        else
                            delete it.release();
                    }
                    semaphore.release();
                });
            }

            THEN("timeout is emitted twice")
            {
                REQUIRE(TRY_ACQUIRE(semaphore, 2, 10));
                REQUIRE(!TRY_ACQUIRE(semaphore, QDeadlineTimer(interval.count() + 1)));
            }
        }
    }
}
