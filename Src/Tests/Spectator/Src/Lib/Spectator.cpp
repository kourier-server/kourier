// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#include "Spectator.h"
#include "ScenarioRunner.h"
#include "GlobalScopeData.h"
#include "SpectatorException.h"
#include <QString>
#include <QtTypes>
#include <QtLogging>
#include <QDebug>

using namespace Qt::StringLiterals;

namespace Spectator
{

SPECTATOR_EXPORT void REQUIRE(bool expr, const std::source_location location)
{
    if (expr) [[likely]]
    {
        if (ScenarioRunner::hasCurrent()) [[likely]]
            ScenarioRunner::current().incrementSuccessfulRequireCounter();
        else [[unlikely]]
            GlobalScopeData::incrementSuccessfulRequireCounter();
    }
    else [[unlikely]]
    {
        QString failureMessage = QString().append(u"REQUIRE failed at file://"_s)
                                        .append(QString::fromUtf8(location.file_name()))
                                        .append(':').append(QString::number(location.line()))
                                        .append('.');
        if (ScenarioRunner::hasCurrent()) [[likely]]
            ScenarioRunner::current().incrementUnsuccessfulRequireCounter(failureMessage);
        else [[unlikely]]
            qFatal().noquote() << failureMessage << Qt::endl;
    }
}

SPECTATOR_EXPORT void INFO(QString message)
{
    if (ScenarioRunner::hasCurrent()) [[likely]]
        ScenarioRunner::current().recordInfoMessage(message);
    else [[unlikely]]
        GlobalScopeData::recordInfoMessage(message);
}

SPECTATOR_EXPORT void FAIL(QString message, const std::source_location location)
{
    throw SpectatorException(message, QString::fromUtf8(location.file_name()), location.line());
}

[[noreturn]] SPECTATOR_EXPORT void FATAL(QString message, const std::source_location location)
{
    qFatal() << u"Spectator FATAL message: "_s << SpectatorException(message, QString::fromUtf8(location.file_name()), location.line()).message() << Qt::endl;
    std::exit(-1);
}

}
