// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#include "GlobalScopeData.h"
#include "NoDestroy.h"
#include <QtTypes>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <QTextStream>
#include <Qt>
#include <cstdio>

using namespace Qt::StringLiterals;

namespace Spectator
{

static std::atomic<qsizetype> & globalSuccessfulRequireCounter()
{
    static constinit NoDestroy<std::atomic<qsizetype>> counter{0};
    return counter();
}

static QMutex & globalInfoMessagesLock()
{
    static NoDestroy<QMutex> lock;
    return lock();
}

static QSet<QString> * globalInfoMessages()
{
    static NoDestroy<QSet<QString>*> pInstance{new QSet<QString>};
    constexpr auto cleanupFcn = [](QSet<QString> * pMessages)
        {
            assert(pMessages);
            QTextStream outputStream(stdout);
            for (const auto & message : *pMessages)
                outputStream << u"INFO: "_s << message << Qt::endl;
        };
    static NoDestroyPtrDeleter<QSet<QString>*> instanceDeleter(pInstance, cleanupFcn);
    return pInstance();
}

void GlobalScopeData::incrementSuccessfulRequireCounter()
{
    ++globalSuccessfulRequireCounter();
}

void GlobalScopeData::recordInfoMessage(QString message)
{
    QMutexLocker locker(&globalInfoMessagesLock());
    auto * pInfoMessages = globalInfoMessages();
    if (pInfoMessages)
        pInfoMessages->insert(message);
    else
        QTextStream(stdout) << u"INFO: "_s << message << Qt::endl;
}

void GlobalScopeData::printGlobalStats(QString & buffer)
{
    QTextStream outputStream(&buffer, QIODeviceBase::WriteOnly);
    outputStream << u"Global Scope"_s << Qt::endl;
    QMutexLocker locker(&globalInfoMessagesLock());
    auto *pGlobalInfoMessages = globalInfoMessages();
    if (pGlobalInfoMessages) [[likely]]
    {
        for (const auto & message : *pGlobalInfoMessages)
            outputStream << u"INFO: "_s << message << Qt::endl;
        pGlobalInfoMessages->clear();
    }
    outputStream << u"Assertions: "_s << globalSuccessfulRequireCounter() << Qt::endl;
}

}
