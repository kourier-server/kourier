// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#ifndef SPECTATOR_SCENARIO_H
#define SPECTATOR_SCENARIO_H

#include "SpectatorGlobals.h"
#include "MacroHelpers.h"
#include "Section.h"
#include <QSemaphore>
#include <QDeadlineTimer>
#include <QStringView>
#include <QString>
#include <QtTypes>
#include <QtClassHelperMacros>
#include <array>
#include <source_location>

namespace Spectator
{

class ScenarioRunner;

class SPECTATOR_EXPORT Scenario : public Section
{
    Q_DISABLE_COPY_MOVE(Scenario)
public:
    Scenario(QStringView sourceFile, qint32 sourceLine, QStringView scenarioName);
    virtual ~Scenario();
    void REQUIRE(bool expr, const std::source_location location = std::source_location::current());
    void INFO(QString message);
    void FAIL(QString message, const std::source_location location = std::source_location::current());
    static bool TRY_ACQUIRE(QSemaphore &semaphore, int resourceCount, QDeadlineTimer deadlineTimer);
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
    virtual qsizetype tagCount() const = 0;
    virtual QStringView tagAt(qsizetype idx) const = 0;
    virtual void scenarioFunction() = 0;

private:
    ScenarioRunner * scenarioRunner();
    void processFailedRequire(const std::source_location location);

private:
    friend class Generator;
    friend class SectionGuard;
    friend class ScenarioRunner;
    ScenarioRunner * m_pScenarioRunner;
};

}

template <int *ptr>
class SpectatorScenarioImpl : private ::Spectator::Scenario {};
template <int *ptr>
class SpectatorScenarioFinal : public SpectatorScenarioImpl<ptr> {};

#define TAG(t) _SPECTATOR_TO_UTF_16_STRING_LITERAL(t)
#define SCENARIO(SCENARIO_NAME, ...) \
    static constinit int _SPECTATOR_CONCATENATE_(SpectatorStaticVariable_, __LINE__){0}; \
    template<> \
    class SpectatorScenarioImpl<& _SPECTATOR_CONCATENATE_(SpectatorStaticVariable_, __LINE__)> : private ::Spectator::Scenario \
    { \
    public: \
        SpectatorScenarioImpl(QStringView sourceFile, qint32 sourceLine, QStringView scenarioName) : ::Spectator::Scenario(sourceFile, sourceLine, scenarioName) {} \
        ~SpectatorScenarioImpl() override = default; \
        inline void REQUIRE(bool expr, const std::source_location location = std::source_location::current()) {::Spectator::Scenario::REQUIRE(expr, location);} \
        inline void INFO(QString message) {::Spectator::Scenario::INFO(message);} \
        inline void FAIL(QString message, const std::source_location location = std::source_location::current()) {::Spectator::Scenario::FAIL(message, location);} \
        inline bool TRY_ACQUIRE(QSemaphore &semaphore, int resourceCount, QDeadlineTimer deadlineTimer) {return ::Spectator::Scenario::TRY_ACQUIRE(semaphore, resourceCount, deadlineTimer);} \
        inline bool TRY_ACQUIRE(QSemaphore &semaphore, QDeadlineTimer deadlineTimer) {return ::Spectator::Scenario::TRY_ACQUIRE(semaphore, deadlineTimer);} \
        inline bool TRY_ACQUIRE(QSemaphore &semaphore, int timeoutInSecs) {return ::Spectator::Scenario::TRY_ACQUIRE(semaphore, timeoutInSecs);} \
        inline bool TRY_ACQUIRE(QSemaphore &semaphore, int resourceCount, int timeoutInSecs) {return ::Spectator::Scenario::TRY_ACQUIRE(semaphore, resourceCount, timeoutInSecs);} \
    protected: \
        virtual void _scenarioFunction() = 0; \
    private: \
        qsizetype tagCount() const override {return m_tags.size() - 1;} \
        QStringView tagAt(qsizetype idx) const override {return m_tags[idx + 1];} \
        void scenarioFunction() override {_scenarioFunction();} \
    private: \
        static constexpr auto m_tags = std::to_array({u"" __VA_OPT__(,) __VA_ARGS__}); \
    }; \
    template <> \
    class SpectatorScenarioFinal<& _SPECTATOR_CONCATENATE_(SpectatorStaticVariable_, __LINE__)> : public SpectatorScenarioImpl<& _SPECTATOR_CONCATENATE_(SpectatorStaticVariable_, __LINE__)> \
    { \
    public: \
        SpectatorScenarioFinal(QStringView sourceFile, qint32 sourceLine, QStringView scenarioName) : SpectatorScenarioImpl(sourceFile, sourceLine, scenarioName) {} \
        ~SpectatorScenarioFinal() override = default; \
    private: \
        void _scenarioFunction() override; \
    }; \
    static SpectatorScenarioFinal<& _SPECTATOR_CONCATENATE_(SpectatorStaticVariable_, __LINE__)> _SPECTATOR_CONCATENATE_(_spectator_scenario, __LINE__)(_SPECTATOR_TO_UTF_16_STRING_LITERAL(__FILE__), __LINE__, _SPECTATOR_TO_UTF_16_STRING_LITERAL("Scenario: " SCENARIO_NAME)); \
    void SpectatorScenarioFinal<& _SPECTATOR_CONCATENATE_(SpectatorStaticVariable_, __LINE__)>::_scenarioFunction()

#endif // SPECTATOR_SCENARIO_H
