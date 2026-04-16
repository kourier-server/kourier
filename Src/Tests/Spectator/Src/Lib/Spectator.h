// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#ifndef SPECTATOR_H
#define SPECTATOR_H

#include "SpectatorGlobals.h"
#include "MacroHelpers.h"
#include "Generator.h"
#include "Scenario.h"
#include "ScenariosRunner.h"
#include "Section.h"
#include "SectionGuard.h"
#include <QCoreApplication>
#include <QString>
#include <QSemaphore>
#include <QDeadlineTimer>
#include <QDebug>
#include <source_location>

#define SPECTATOR_MAIN \
    int main(int argc, char ** argv) \
    { \
        QCoreApplication app(argc, argv); \
        ::Spectator::ScenariosRunner scenariosRunner; \
        scenariosRunner.runScenarios(); \
        return QCoreApplication::exec(); \
    }

#define SECTION(NAME) \
    static const ::Spectator::Section _SPECTATOR_CONCATENATE_(_spectator_section, __LINE__)(_SPECTATOR_TO_UTF_16_STRING_LITERAL(__FILE__), __LINE__, _SPECTATOR_TO_UTF_16_STRING_LITERAL(NAME)); \
    /* The explicit cast below on the this pointer is permitted per C++20 Standard 11.9.2 (4.4) */ \
    if (::Spectator::SectionGuard _SPECTATOR_CONCATENATE_(_spectator_section_guard, __LINE__)(& _SPECTATOR_CONCATENATE_(_spectator_section, __LINE__), (::Spectator::Scenario*)this); _SPECTATOR_CONCATENATE_(_spectator_section_guard, __LINE__).hasEntered())
#define GIVEN(NAME) SECTION("Given: " NAME)
#define WHEN(NAME) SECTION("When: " NAME)
#define THEN(NAME) SECTION("Then: " NAME)
#define AND_WHEN(NAME) SECTION("And When: " NAME)
#define AND_THEN(NAME) SECTION("And Then: " NAME)

namespace Spectator
{
    SPECTATOR_EXPORT void REQUIRE(bool expr, const std::source_location location = std::source_location::current());
    SPECTATOR_EXPORT void INFO(QString message);
    SPECTATOR_EXPORT void FAIL(QString message, const std::source_location location = std::source_location::current());
    inline bool TRY_ACQUIRE(QSemaphore &semaphore, int resourceCount, QDeadlineTimer deadlineTimer)
    {
        return Scenario::TRY_ACQUIRE(semaphore, resourceCount, deadlineTimer);
    }
    inline static bool TRY_ACQUIRE(QSemaphore &semaphore, QDeadlineTimer deadlineTimer)
    {
        return TRY_ACQUIRE(semaphore, 1, deadlineTimer);
    }
    inline static bool TRY_ACQUIRE(QSemaphore &semaphore, int timeoutInSecs)
    {
        return TRY_ACQUIRE(semaphore, QDeadlineTimer(1000*timeoutInSecs));
    }
    inline static bool TRY_ACQUIRE(QSemaphore &semaphore, int resourceCount, int timeoutInSecs)
    {
        return TRY_ACQUIRE(semaphore, resourceCount, QDeadlineTimer(1000*timeoutInSecs));
    }
}

#endif // SPECTATOR_H
