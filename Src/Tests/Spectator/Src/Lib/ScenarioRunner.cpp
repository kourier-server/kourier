// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#include "Scenario.h"
#include "ScenarioRunner.h"
#include "Generator.h"
#include <QtLogging>
#include <QDebug>
#include <QElapsedTimer>
#include <QString>
#include <QByteArray>
#include <QMutex>
#include <QMutexLocker>
#include <Qt>
#include <QTextStream>
#include <exception>

using namespace Qt::StringLiterals;

namespace Spectator
{

constinit thread_local ScenarioRunner * ScenarioRunner::m_pCurrentRunner = nullptr;

ScenarioRunner::ScenarioRunner(Scenario * pScenario) :
    m_pScenario(pScenario)
{
    if (!m_pScenario) [[unlikely]]
        qFatal("Failed to create scenario runner. Given scenario is null.");
    m_sectionsStack.reserve(16);
    m_generatorsStack.reserve(8);
}

bool ScenarioRunner::tryPushSection(Section const * const pSection)
{
    switch (m_state)
    {
        case State::HeadingForLeaf:
            if (m_sectionsWithFullyVisitedChildren.contains(pSection)
                && (pSection != m_pScenario || (m_untouchedGenerators.isEmpty() && hasConsumedAllGeneratorDataOnCurrentPath())))
                return false;
            else
            {
                m_sectionsStack.push(pSection);
                return true;
            }
        case State::HeadingForRoot:
            m_scenarioHasUnvisitedChildren = m_scenarioHasUnvisitedChildren
                                             || !m_sectionsWithFullyVisitedChildren.contains(pSection);
            return false;
    }
}

void ScenarioRunner::popSection(Section const * const pSection)
{
    assert(m_sectionsStack.top() == pSection);
    if (m_state == State::HeadingForLeaf)
    {
        m_state = State::HeadingForRoot;
        tryToAdvanceGeneratorsOnCurrentPath();
        m_pathToLeafSection = m_sectionsStack;
        m_scenarioHasUnvisitedChildren = !hasConsumedAllGeneratorDataOnCurrentPath();
    }
    if (!m_scenarioHasUnvisitedChildren)
        m_sectionsWithFullyVisitedChildren.insert(pSection);
    m_sectionsStack.pop();
    m_hasVisitedAllLeafNodes = m_sectionsStack.isEmpty()
                               && !m_scenarioHasUnvisitedChildren;
}

qsizetype ScenarioRunner::getGeneratorIndex(Generator const * const generator)
{
    switch (m_state)
    {
        case State::HeadingForLeaf:
            if (m_untouchedGenerators.contains(generator))
                m_untouchedGenerators.remove(generator);
            m_touchedGenerators.insert(generator);
            break;
        case State::HeadingForRoot:
            if (!m_touchedGenerators.contains(generator))
                m_untouchedGenerators.insert(generator);
            return 0;
    }
    if (!m_isValidatingGeneratorStack)
        m_generatorsStack.push({generator, 0});
    else if (m_idxNextGenerator >= m_generatorsStack.size() || m_generatorsStack[m_idxNextGenerator].first != generator)
    {
        qFatal().noquote() << QString(u"Generators must be declared in the section's outermost scope "
                                       "(the entire section body). Generator at %1:%2 was not."_s)
                                       .arg(generator->sourceFile()).arg(generator->sourceLine());
    }
    return m_generatorsStack[m_idxNextGenerator++].second;
}

ScenarioRunner & ScenarioRunner::current()
{
    if (m_pCurrentRunner)
        return *m_pCurrentRunner;
    else
        qFatal("Failed to fetch current scenario runner. No scenario runner has been set for this thread.");
}

ScenarioRunResults ScenarioRunner::runScenario(Scenario & scenario)
{
    if (m_pCurrentRunner != nullptr) [[unlikely]]
        qFatal("Failed to set current scenario runner. There is another scenario being ran on this thread and only one scenario can be run at a time per thread.");
    ScenarioRunner scenarioRunner(&scenario);
    scenario.m_pScenarioRunner = &scenarioRunner;
    m_pCurrentRunner = &scenarioRunner;
    do
    {
        scenarioRunner.runScenarioPath();
    } while (!scenarioRunner.hasVisitedAllLeafNodes());
    m_pCurrentRunner = nullptr;
    scenario.m_pScenarioRunner = nullptr;
    return scenarioRunner.m_scenarioRunResults;
}

void ScenarioRunner::runScenarioPath()
{
    QElapsedTimer elapsedTimer;
    elapsedTimer.start();
    qsizetype runCount = 0;
    m_successfulRequireCounter = 0;
    QString failureMessage;
    do
    {
        ++runCount;
        m_state = State::HeadingForLeaf;
        m_idxNextGenerator = 0;
        m_isValidatingGeneratorStack = !m_generatorsStack.isEmpty();
        try
        {
            if (!tryPushSection(m_pScenario))
                qFatal().noquote() << "Failed to push scenario section named "
                                   << m_pScenario->name()
                                   << "." << Qt::endl << "This is unexpected and is an internal error of Spectator.";
            m_pScenario->scenarioFunction();
            popSection(m_pScenario);
        }
        catch (const SpectatorException &ex)
        {
            failureMessage = ex.message();
            break;
        }
        catch (const std::exception &ex)
        {
            failureMessage = QString().append(u"\nTest code has thrown an unhandled std::exception with message: "_s)
                                        .append(QString::fromUtf8(ex.what()));
            break;
        }
        catch (...)
        {
            failureMessage = QString().append(u"\nTest code has thrown an unhandled non-standard exception."_s);
            break;
        }
    } while (!hasConsumedAllGeneratorDataOnCurrentPath());
    const auto elapsedTimeInNSecs = elapsedTimer.nsecsElapsed();
    m_scenarioRunResults.addScenarioPath(m_pathToLeafSection,
                                         m_infoMessages,
                                         failureMessage,
                                         m_successfulRequireCounter,
                                         m_unsuccessfulRequireCounter,
                                         runCount,
                                         elapsedTimeInNSecs);
}

void ScenarioRunner::tryToAdvanceGeneratorsOnCurrentPath()
{
    m_hasConsumedAllGeneratorDataOnCurrentPath = true;
    if (!m_generatorsStack.isEmpty())
    {
        for (qsizetype idx = (m_generatorsStack.size() - 1); idx >= 0; --idx)
        {
            auto & currentGenerator = m_generatorsStack[idx];
            if (currentGenerator.second < (currentGenerator.first->size() - 1))
            {
                ++currentGenerator.second;
                for (qsizetype idxToReset = (idx + 1); idxToReset < m_generatorsStack.size(); ++idxToReset)
                    m_generatorsStack[idxToReset].second = 0;
                m_hasConsumedAllGeneratorDataOnCurrentPath = false;
                return;
            }
        }
        m_generatorsStack.clear();
    }
}

}
