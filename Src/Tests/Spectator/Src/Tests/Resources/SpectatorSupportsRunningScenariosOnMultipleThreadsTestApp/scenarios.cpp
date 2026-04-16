// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#include <Spectator.h>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QtLogging>
#include <QDeadlineTimer>
#include <atomic>
#include <qdeadlinetimer.h>

static void testThreadId(Qt::HANDLE currentId)
{
    static bool firstRun = true;
    static QMutex lock;
    static Qt::HANDLE lastId = 0;
    static std::atomic_flag hasEntered{false};
    QMutexLocker locker(&lock);
    if (firstRun)
    {
        firstRun = false;
        lastId = currentId;
        locker.unlock();
        QDeadlineTimer deadline(5000);
        while (!hasEntered.test() && !deadline.hasExpired())
        {
            QThread::msleep(1);
        }
        if (deadline.hasExpired())
            qFatal("Failed to fetch thread id from another thread. Scenarios are not running on multiple threads.");
    }
    else
    {
        hasEntered.test_and_set();
        hasEntered.notify_all();
        if (lastId == currentId)
            qFatal("Thread ids match. Scenarios are not running on multiple threads.");
    }
}

SCENARIO("Scenario 1")
{
    testThreadId(QThread::currentThreadId());
}

SCENARIO("Scenario 2")
{
    testThreadId(QThread::currentThreadId());
}
