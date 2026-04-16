// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#ifndef SPECTATOR_SCENARIO_RUNNER_H
#define SPECTATOR_SCENARIO_RUNNER_H

#include "ScenarioRunResults.h"
#include "SpectatorException.h"
#include <QStack>
#include <QSet>
#include <QMap>
#include <QString>
#include <QtTypes>
#include <utility>

namespace Spectator
{

class Scenario;
class Section;
class Generator;

class ScenarioRunner
{
public:
    ScenarioRunner() = default;
    ScenarioRunner(Scenario * pScenario);
    ScenarioRunner & operator=(const ScenarioRunner &) = default;
    ~ScenarioRunner() = default;
    bool tryPushSection(Section const * const pSection);
    void popSection(Section const * const pSection);
    qsizetype getGeneratorIndex(Generator const * const generator);
    static ScenarioRunner & current();
    inline void incrementSuccessfulRequireCounter() {++m_successfulRequireCounter;}
    inline void incrementUnsuccessfulRequireCounter(QString failureMessage)
    {
        ++m_unsuccessfulRequireCounter;
        throw SpectatorException(failureMessage);
    }
    inline void recordInfoMessage(QString message)
    {
        m_infoMessages[m_sectionsStack.top()].insert(message);
    }
    static ScenarioRunResults runScenario(Scenario & scenario);
    inline static bool hasCurrent() {return m_pCurrentRunner != nullptr;}

private:
    void runScenarioPath();
    void tryToAdvanceGeneratorsOnCurrentPath();
    inline bool hasConsumedAllGeneratorDataOnCurrentPath() const {return m_hasConsumedAllGeneratorDataOnCurrentPath;}
    inline bool hasVisitedAllLeafNodes() const {return m_hasVisitedAllLeafNodes && m_untouchedGenerators.isEmpty();}

private:
    static constinit thread_local ScenarioRunner * m_pCurrentRunner;
    Scenario * m_pScenario = nullptr;
    enum class State {HeadingForLeaf, HeadingForRoot};
    State m_state = State::HeadingForLeaf;
    ScenarioRunResults m_scenarioRunResults;
    QStack<Section const *> m_sectionsStack;
    QStack<Section const *> m_pathToLeafSection;
    QMap<Section const *, QSet<QString>> m_infoMessages;
    QStack<std::pair<Generator const *, qsizetype>> m_generatorsStack;
    qsizetype m_idxNextGenerator = 0;
    bool m_isValidatingGeneratorStack = false;
    QSet<Generator const *> m_touchedGenerators;
    QSet<Generator const *> m_untouchedGenerators;
    QSet<Section const *> m_sectionsWithFullyVisitedChildren;
    bool m_scenarioHasUnvisitedChildren = false;
    bool m_hasVisitedAllLeafNodes = false;
    qsizetype m_successfulRequireCounter = 0;
    qsizetype m_unsuccessfulRequireCounter = 0;
    bool m_hasConsumedAllGeneratorDataOnCurrentPath = false;
};

}

#endif // SPECTATOR_SCENARIO_RUNNER_H
