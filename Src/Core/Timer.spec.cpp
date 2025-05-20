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
#include <QTimer>
#include <QCoreApplication>
#include <QThread>
#include <QDateTime>
#include <QSemaphore>
#include <QElapsedTimer>
#include <cmath>
#include <Spectator.h>

using Kourier::Timer;
using Kourier::Object;
using Spectator::SemaphoreAwaiter;


SCENARIO("Timer with non-zero interval times out after given interval but before 1024ms after given interval")
{
    GIVEN("a timer set to expire")
    {
        Timer timer;
        const auto intervalInMSecs = GENERATE(AS(qint64), 0, 1, 180, 3500);
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
            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(semaphore, 10));

            THEN("timer timeout after given interval but before 1024ms after given interval")
            {
                REQUIRE(intervalInMSecs <= elapsedTimeInMSecs && elapsedTimeInMSecs <= (1024 + intervalInMSecs));
            }
        }
    }
}


SCENARIO("Active Timer reschedules its timeout upon changes on timer's interval")
{
    GIVEN("a single-shot timer with a given interval")
    {
        Timer timer;
        const auto intervalInMSecs = GENERATE(AS(qint64), 0, 1, 180, 3500);
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
                    const auto newIntervalInMSecs = GENERATE(AS(qint64), 0, 1, 180, 3500);
                    elapsedTimer.start();
                    timer.setInterval(newIntervalInMSecs);
                    REQUIRE(newIntervalInMSecs == timer.interval())
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(semaphore, 10));

                    THEN("timer timeout after given interval but before 1024ms after given interval")
                    {
                        REQUIRE(newIntervalInMSecs <= elapsedTimeInMSecs && elapsedTimeInMSecs <= (1024 + newIntervalInMSecs));
                    }
                }
            }
        }
    }

    GIVEN("a single-shot timer with an interval of 5000ms")
    {
        Timer timer;
        const qint64 intervalInMSecs = 5000;
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

        WHEN("timer is started and we wait for 4800ms")
        {
            elapsedTimer.start();
            timer.start();
            QThread::msleep(3000);
            QCoreApplication::processEvents();

            THEN("timer does not emit timeout")
            {
                REQUIRE(0 == expirationCount);

                AND_WHEN("we wait until timer expires after timer interval is changed")
                {
                    elapsedTimer.start();
                    timer.setInterval(intervalInMSecs);
                    REQUIRE(intervalInMSecs == timer.interval())
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(semaphore, 10));

                    THEN("timer timeout after given interval but before 1024ms after given interval")
                    {
                        REQUIRE(intervalInMSecs <= elapsedTimeInMSecs && elapsedTimeInMSecs <= (1024 + intervalInMSecs));
                    }
                }
            }
        }
    }
}


SCENARIO("Active Timer reschedules its timeout if it is started again")
{
    GIVEN("a single-shot timer with a given interval")
    {
        Timer timer;
        const auto intervalInMSecs = GENERATE(AS(qint64), 0, 1, 180, 3500);
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
                    const auto newIntervalInMSecs = GENERATE(AS(qint64), 0, 1, 180, 3500);
                    elapsedTimer.start();
                    timer.start(newIntervalInMSecs);
                    REQUIRE(newIntervalInMSecs == timer.interval())
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(semaphore, 10));

                    THEN("timer timeout after given interval but before 1024ms after given interval")
                    {
                        REQUIRE(newIntervalInMSecs <= elapsedTimeInMSecs && elapsedTimeInMSecs <= (1024 + newIntervalInMSecs));
                    }
                }
            }
        }
    }


    GIVEN("a single-shot timer with an interval of 5000ms")
    {
        Timer timer;
        const qint64 intervalInMSecs = 5000;
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

        WHEN("timer is started and we wait for 4800ms")
        {
            elapsedTimer.start();
            timer.start();
            QThread::msleep(3000);
            QCoreApplication::processEvents();

            THEN("timer does not emit timeout")
            {
                REQUIRE(0 == expirationCount);

                AND_WHEN("we wait until timer expires after starting timer")
                {
                    elapsedTimer.start();
                    timer.start();
                    REQUIRE(intervalInMSecs == timer.interval())
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(semaphore, 10));

                    THEN("timer timeout after given interval but before 1024ms after given interval")
                    {
                        REQUIRE(intervalInMSecs <= elapsedTimeInMSecs && elapsedTimeInMSecs <= (1024 + intervalInMSecs));
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
        const auto intervalInMSecs = GENERATE(AS(qint64), 0, 1, 180, 3500);
        timer.setInterval(intervalInMSecs);
        timer.setSingleShot(false);
        Object::connect(&timer, &Timer::timeout, [](){FAIL("This code is supposed to be unreachable.");});
        timer.start();

        WHEN("timer is stopped")
        {
            timer.stop();

            THEN("timer does not emit timeout signal")
            {
                QSemaphore semaphore;
                REQUIRE(!SemaphoreAwaiter::signalSlotAwareWait(semaphore, (intervalInMSecs + 1024 + 999)/1000));
            }
        }
    }
}


SCENARIO("Expired Timer does not emit timeout if it is stopped")
{
    GIVEN("an expired timer with a given interval")
    {
        Timer timer;
        const auto intervalInMSecs = GENERATE(AS(qint64), 0, 1, 180, 3500);
        timer.setInterval(intervalInMSecs);
        timer.setSingleShot(false);
        Object::connect(&timer, &Timer::timeout, [](){FAIL("This code is supposed to be unreachable.");});
        timer.start();
        QThread::sleep((intervalInMSecs + 1025 + 999)/1000);

        WHEN("timer is stopped")
        {
            timer.stop();

            THEN("timer does not emit timeout signal when control returns to event loop")
            {
                QSemaphore semaphore;
                REQUIRE(!SemaphoreAwaiter::signalSlotAwareWait(semaphore, (intervalInMSecs + 1024 + 999)/1000));
            }
        }
    }
}


SCENARIO("Expired Timer emits timeout when control returns to the event loop")
{
    GIVEN("a timer")
    {
        const auto isSingleShot = GENERATE(AS(bool), true, false);
        const auto interval = GENERATE(AS(qint64), 0, 1, 350, 1240, 3822);
        const auto setInterval = GENERATE(AS(bool), true, false);
        const auto setIntervalWhenStarting = GENERATE(AS(bool), true, false);
        Timer timer;
        int timeoutEmissionCounter = 0;
        Object::connect(&timer, &Timer::timeout, [&timeoutEmissionCounter](){++timeoutEmissionCounter;});
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
            const auto timeInMSecsToExpire = interval + 1025;
            QCoreApplication::processEvents();
            REQUIRE(0 == timeoutEmissionCounter);
            QThread::msleep(timeInMSecsToExpire);

            THEN("timer emits timeout")
            {
                QCoreApplication::processEvents();
                REQUIRE(1 == timeoutEmissionCounter);
            }
        }
    }
}


SCENARIO("Expired Timer does not emit timeout when restarted if Timer Registrar's main timer expires")
{
    GIVEN("a timer")
    {
        const auto isSingleShot = GENERATE(AS(bool), true, false);
        const auto interval = GENERATE(AS(qint64), 1240, 3822);
        const auto setInterval = GENERATE(AS(bool), true, false);
        const auto setIntervalWhenStarting = GENERATE(AS(bool), true, false);
        const auto expireMainTimer = GENERATE(AS(bool), true, false);
        Timer timer;
        int timeoutEmissionCounter = 0;
        Object::connect(&timer, &Timer::timeout, [&timeoutEmissionCounter](){++timeoutEmissionCounter;});
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
            const auto timeInMSecsToWaitForMainTimerToExpire = (expireMainTimer ? (interval > 1024 ? interval - 512 : 512) : 0);
            if (expireMainTimer)
                QThread::msleep(timeInMSecsToWaitForMainTimerToExpire);
            const auto timeInMSecsToExpire = interval + 1025 - timeInMSecsToWaitForMainTimerToExpire;
            QCoreApplication::processEvents();
            REQUIRE(0 == timeoutEmissionCounter);
            QThread::msleep(timeInMSecsToExpire);
            timer.start();

            THEN("timer does not emit timeout if main timer expires")
            {
                QCoreApplication::processEvents();
                REQUIRE(timeoutEmissionCounter == (expireMainTimer ? 0 : 1));
            }
        }
    }
}


SCENARIO("Single-shot Timer emits timeout only once")
{
    GIVEN("a single shot timer")
    {
        const auto interval = GENERATE(AS(qint64), 0, 1, 350, 1240, 3822);
        const auto setInterval = GENERATE(AS(bool), true, false);
        const auto setIntervalWhenStarting = GENERATE(AS(bool), true, false);
        Timer timer;
        int timeoutEmissionCounter = 0;
        Object::connect(&timer, &Timer::timeout, [&timeoutEmissionCounter](){++timeoutEmissionCounter;});
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
            const auto timeInMSecsToExpire = interval + 1025;
            QCoreApplication::processEvents();
            REQUIRE(0 == timeoutEmissionCounter);
            QThread::msleep(timeInMSecsToExpire);

            THEN("timer emits timeout")
            {
                QCoreApplication::processEvents();
                REQUIRE(1 == timeoutEmissionCounter);

                AND_WHEN("control returns to the event loop after timer expires again")
                {
                    QThread::msleep(timeInMSecsToExpire);
                    QCoreApplication::processEvents();

                    THEN("timer does not emit timeout")
                    {    
                        REQUIRE(1 == timeoutEmissionCounter);
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
        const auto interval = GENERATE(AS(qint64), 0, 1, 350, 1240, 3822);
        const auto setInterval = GENERATE(AS(bool), true, false);
        const auto setIntervalWhenStarting = GENERATE(AS(bool), true, false);
        const auto changeInterval = GENERATE(AS(bool), true, false);
        const auto newInterval = GENERATE(AS(qint64), 0, 1, 350, 1240, 3822);
        Timer timer;
        int timeoutEmissionCounter = 0;
        Object::connect(&timer, &Timer::timeout, [&timeoutEmissionCounter](){++timeoutEmissionCounter;});
        timer.setSingleShot(false);
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
            const auto timeInMSecsToExpire = interval + 1025;
            QCoreApplication::processEvents();
            REQUIRE(0 == timeoutEmissionCounter);
            QThread::msleep(timeInMSecsToExpire);

            THEN("timer emits timeout")
            {
                QCoreApplication::processEvents();
                REQUIRE(1 == timeoutEmissionCounter);

                AND_WHEN("control returns to the event loop after timer expires again")
                {
                    if (changeInterval)
                    {
                        timer.setInterval(newInterval);
                        const auto newTimeInMSecsToExpire = newInterval + 1025;
                        QThread::msleep(newTimeInMSecsToExpire);
                    }
                    else
                        QThread::msleep(timeInMSecsToExpire);
                    QCoreApplication::processEvents();

                    THEN("timer emits timeout")
                    {
                        REQUIRE(2 == timeoutEmissionCounter);
                    }
                }
            }
        }
    }
}


SCENARIO("Changing active Timer to single shot makes it emit timeout signal only one more time")
{
    GIVEN("a timer with given interval")
    {
        Timer timer;
        const auto interval = GENERATE(AS(qint64), 0, 1, 350, 1240, 3822);
        timer.setInterval(interval);
        timer.setSingleShot(false);
        QSemaphore timeoutSemaphore;
        Object::connect(&timer, &Timer::timeout, [&timeoutSemaphore](){timeoutSemaphore.release();});
        QElapsedTimer elapsedTimer;
        elapsedTimer.start();
        timer.start();

        WHEN("time expires")
        {
            THEN("timeout is emitted")
            {
                for (auto i = 0; i < 3; ++i)
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(timeoutSemaphore, 5));
                    const auto elapsedTimeInMSecs = elapsedTimer.restart();
                    REQUIRE(interval <= elapsedTimeInMSecs && elapsedTimeInMSecs <= (interval + 1024));
                }

                AND_WHEN("timer is set as single shot")
                {
                    timer.setSingleShot(true);

                    THEN("timer emits timeout only one more time")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(timeoutSemaphore, 5));
                        REQUIRE(!SemaphoreAwaiter::signalSlotAwareWait(timeoutSemaphore, 5));
                    }
                }
            }
        }
    }
}


SCENARIO("Changing expired Timer to single shot makes it emit timeout signal only one more time")
{
    GIVEN("a timer with given interval")
    {
        Timer timer;
        const auto interval = GENERATE(AS(qint64), 0, 1, 350, 1240, 3822);
        timer.setInterval(interval);
        timer.setSingleShot(false);
        QSemaphore timeoutSemaphore;
        Object::connect(&timer, &Timer::timeout, [&timeoutSemaphore](){timeoutSemaphore.release();});
        QElapsedTimer elapsedTimer;
        elapsedTimer.start();
        timer.start();

        WHEN("time expires")
        {
            THEN("timeout is emitted")
            {
                for (auto i = 0; i < 3; ++i)
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(timeoutSemaphore, 5));
                    const auto elapsedTimeInMSecs = elapsedTimer.restart();
                    REQUIRE(interval <= elapsedTimeInMSecs && elapsedTimeInMSecs <= (interval + 1024));
                }

                AND_WHEN("timer is set as single shot")
                {
                    QThread::msleep(interval + 1025);
                    timer.setSingleShot(true);

                    THEN("timer emits timeout only one more time")
                    {
                        REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(timeoutSemaphore, 5));
                        REQUIRE(!SemaphoreAwaiter::signalSlotAwareWait(timeoutSemaphore, 5));
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
        const auto interval = GENERATE(AS(qint64), 0, 1, 350, 1240, 3822);
        timer.setInterval(interval);
        timer.setSingleShot(true);
        QSemaphore timeoutSemaphore;
        Object::connect(&timer, &Timer::timeout, [&timeoutSemaphore](){timeoutSemaphore.release();});
        QElapsedTimer elapsedTimer;
        elapsedTimer.start();
        timer.start();

        WHEN("timer is set to non single shot before time expires")
        {
            timer.setSingleShot(false);

            THEN("timeout is emitted")
            {
                for (auto i = 0; i < 3; ++i)
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(timeoutSemaphore, 5));
                    const auto elapsedTimeInMSecs = elapsedTimer.restart();
                    REQUIRE(interval <= elapsedTimeInMSecs && elapsedTimeInMSecs <= (interval + 1024));
                }
            }
        }
    }
}


SCENARIO("Changing expired Timer to non single-shot makes it emit timeout repeatedly")
{
    GIVEN("a timer with given interval")
    {
        Timer timer;
        const auto interval = GENERATE(AS(qint64), 0, 1, 350, 1240, 3822);
        timer.setInterval(interval);
        timer.setSingleShot(true);
        QSemaphore timeoutSemaphore;
        Object::connect(&timer, &Timer::timeout, [&timeoutSemaphore](){timeoutSemaphore.release();});
        QElapsedTimer elapsedTimer;
        elapsedTimer.start();
        timer.start();
        QThread::msleep(interval + 1025);

        WHEN("timer is set to non single shot before time expires")
        {
            timer.setSingleShot(false);

            THEN("timeout is emitted")
            {
                for (auto i = 0; i < 3; ++i)
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(timeoutSemaphore, 5));
                    const auto elapsedTimeInMSecs = elapsedTimer.restart();
                    if (i > 0)
                    {
                        REQUIRE(interval <= elapsedTimeInMSecs && elapsedTimeInMSecs <= (interval + 1024));
                    }
                }
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
        for (auto &pTimer : pTimers)
        {
            pTimer->setSingleShot(true);
            pTimer->start(0);
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
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(semaphore, 2));
                }
                REQUIRE(!SemaphoreAwaiter::signalSlotAwareWait(semaphore, 2));
            }
        }
    }
}


SCENARIO("Expired timers can be deleted before expiration")
{
    GIVEN("four expired single-shot timers with same interval")
    {
        std::unique_ptr<Timer> pTimers[4] = {std::make_unique<Timer>(),
                                             std::make_unique<Timer>(),
                                             std::make_unique<Timer>(),
                                             std::make_unique<Timer>()};
        QSemaphore semaphore;
        for (auto &pTimer : pTimers)
        {
            pTimer->setSingleShot(true);
            pTimer->start(0);
            Object::connect(pTimer.get(), &Timer::timeout, [&semaphore](){semaphore.release();});
        }
        QThread::sleep(2);

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
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(semaphore, 2));
                }
                REQUIRE(!SemaphoreAwaiter::signalSlotAwareWait(semaphore, 2));
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
        for (auto &pTimer : pTimers)
        {
            pTimer->setSingleShot(true);
            pTimer->start(0);
        }
        QSemaphore semaphore;

        WHEN("first timer to emit timeout deletes one of the others")
        {
            for (auto &pTimer : pTimers)
            {
                Object::connect(pTimer.get(), &Timer::timeout, [&pTimers, &pTimer, &semaphore]()
                {
                    static bool firstRun = true;
                    if (firstRun)
                    {
                        firstRun = false;
                        for (auto &it : pTimers)
                        {
                            if (it.get() == pTimer.get())
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
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(semaphore, 2));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(semaphore, 1));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(semaphore, 1));
                REQUIRE(!SemaphoreAwaiter::signalSlotAwareWait(semaphore, 1));
            }
        }

        WHEN("first timer to emit timeout deletes all of the others")
        {
            for (auto &pTimer : pTimers)
            {
                Object::connect(pTimer.get(), &Timer::timeout, [&pTimers, &pTimer, &semaphore]()
                {
                    static bool firstRun = true;
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

            THEN("timeout is emitted only once")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(semaphore, 2));
                REQUIRE(!SemaphoreAwaiter::signalSlotAwareWait(semaphore, 1));
            }
        }

        WHEN("first timer to emit timeout add a new Timer before deleting all of the others")
        {
            std::unique_ptr<Timer> addedTimer;

            for (auto &pTimer : pTimers)
            {
                Object::connect(pTimer.get(), &Timer::timeout, [&pTimers, &pTimer, &semaphore, &addedTimer]()
                {
                    addedTimer.reset(new Timer);
                    Object::connect(addedTimer.get(), &Timer::timeout, [&semaphore](){semaphore.release();});
                    addedTimer->setSingleShot(true);
                    addedTimer->start(0);
                    static bool firstRun = true;
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
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(semaphore, 2));
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(semaphore, 2));
                REQUIRE(!SemaphoreAwaiter::signalSlotAwareWait(semaphore, 1));
            }
        }
    }
}


SCENARIO("Timers fire at the right time")
{
    GIVEN("multiple timers each with a different interval")
    {
        static constexpr size_t timersCount = 384;
        QSemaphore semaphore;
        size_t counter = 0;
        Timer timers[timersCount];
        QElapsedTimer elapsedTimer;
        for (auto i = 0; i < timersCount; ++i)
        {
            timers[i].setSingleShot(true);
            timers[i].setInterval(i * 1000);
            auto *pTimer = &timers[i];
            Object::connect(pTimer, &Timer::timeout, [&counter, &semaphore, &elapsedTimer, pTimer]()
            {
                const auto elapsedTime = elapsedTimer.elapsed();
                REQUIRE(pTimer->interval() <= elapsedTime && elapsedTime <= (pTimer->interval() + 1024));
                if (++counter == timersCount)
                    semaphore.release();
            });
        }

        WHEN("timers are started")
        {
            elapsedTimer.start();
            for (auto &timer : timers)
                timer.start();

            THEN("all timers fire at expected time")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(semaphore, 512));
            }
        }
    }
}


/*
SCENARIO("Memory consumption of Qt timers")
{
    const size_t counter = 1;
    std::vector<QTimer> timers(counter);
    static constexpr auto pFcn = []() {std::cout << "I'm here";};
    for (auto &timer : timers)
        QObject::connect(&timer, &QTimer::timeout, pFcn);
    for (auto &timer : timers)
        timer.start(100000);
    timers.clear();
}

SCENARIO("Memory consumption of Kourier timers")
{
    const size_t counter = 1;
    std::vector<Timer> timers(counter);
    static constexpr auto pFcn = []() {std::cout << "I'm here";};
    for (auto &timer : timers)
        Object::connect(&timer, &Timer::timeout, pFcn);
    for (auto &timer : timers)
        timer.start(100000);
    timers.clear();
}
*/
