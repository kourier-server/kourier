// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#include <Spectator.h>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QtLogging>

static void testThreadId(Qt::HANDLE currentId)
{
    static bool firstRun = true;
    static QMutex lock;
    static Qt::HANDLE lastId = 0;
    QMutexLocker locker(&lock);
    if (firstRun)
    {
        firstRun = false;
        lastId = currentId;
    }
    else if (lastId != currentId)
        qFatal("Thread ids do not match. Scenarios are not running on a single thread.");
}

SCENARIO("Scenario 1")
{
    QThread::sleep(1);
    testThreadId(QThread::currentThreadId());
}

SCENARIO("Scenario 2")
{
    QThread::sleep(1);
    testThreadId(QThread::currentThreadId());
}
