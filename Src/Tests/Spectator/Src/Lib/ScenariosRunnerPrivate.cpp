// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#include "ScenariosRunnerPrivate.h"
#include "ScenarioRepository.h"
#include "Scenario.h"
#include "ScenarioRunner.h"
#include "ScenarioRunResults.h"
#include "Settings.h"
#include "ScenarioFilter.h"
#include <QCoreApplication>
#include <QtLogging>
#include <QtConcurrent>
#include <QFuture>
#include <QDeadlineTimer>
#include <QEventLoop>
#include <QTimer>
#include <QMetaObject>
#include <QString>
#include <Qt>
#include <cstdio>

namespace Spectator
{

void ScenariosRunnerPrivate::runScenarios()
{
    if (!m_hasRanScenarios)
        m_hasRanScenarios = true;
    else
        qFatal("Failed to run scenarios. ScenariosRunner::runScenarios can only be called once.");
    auto *pApp = QCoreApplication::instance();
    if (!pApp) [[unlikely]]
        qFatal("Failed to run scenarios. Current QCoreApplication instance is null. Please, create a QCoreInstance before trying to run the scenarios.");
    const auto settings = Settings::fromCmdLine();
    m_repetitionCount = settings.repetitionCount();
    if (m_repetitionCount > 0) [[unlikely]]
        QTextStream(stdout) << "Repeating tests " << m_repetitionCount << " times." << Qt::endl;
    m_threadPool.setMaxThreadCount(settings.threadCount());
    auto allScenarios = fetchScenarios();
    ScenarioFilter scenarioFilter(settings);
    m_filteredScenarios.reserve(allScenarios.size());
    for (qsizetype i = 0; i < allScenarios.size(); ++i)
    {
        if (scenarioFilter.hasToRunScenario(allScenarios[i]))
            m_filteredScenarios.append(allScenarios[i]);
    }
    if (m_filteredScenarios.isEmpty())
        qFatal("Failed to run scenarios. There are no scenarios to run.");
    scheduleFileteredScenariosExecution();
}

void ScenariosRunnerPrivate::onFinishedRunningScenario()
{
    if (++m_finishedRunningScenariosCounter == m_scenarioWatchers.size())
    {
        QDeadlineTimer deadline(5000);
        m_threadPool.waitForDone(deadline);
        if (deadline.hasExpired())
            qFatal("Failed to wait for the threads of the thread pool responsible for running scenarios to stop.");
        processScenariosResults();
        if (m_repetitionCount == 0
            || ++m_repetitionCounter == m_repetitionCount
            || m_unsuccessfulScenarioPathRunCounter != 0) [[likely]]
        {
            printResults();
            if (!QMetaObject::invokeMethod(QCoreApplication::instance(), &QCoreApplication::quit, Qt::QueuedConnection))
                qFatal("Failed schedule call to QCoreApplication::quit.");
        }
        else [[unlikely]]
            scheduleFileteredScenariosExecution();
    }
}

QVector<Scenario*> ScenariosRunnerPrivate::fetchScenarios()
{
    return ScenarioRepository::global().getAll();
}

void ScenariosRunnerPrivate::scheduleFileteredScenariosExecution()
{
    for (const auto & watcher : m_scenarioWatchers)
    {
        if (!watcher->isFinished())
            qFatal("Failed to schedule execution for filtered scenarios. There are scenarios from previous iteration still running.");
    }
    m_finishedRunningScenariosCounter = 0;
    m_scenarioWatchers.clear();
    m_scenarioWatchers.resize(m_filteredScenarios.size());
    for (auto i = 0; i < m_filteredScenarios.size(); ++i)
    {
        m_scenarioWatchers[i].reset(new QFutureWatcher<ScenarioRunResults>{});
        QObject::connect(m_scenarioWatchers[i].get(), &QFutureWatcher<ScenarioRunResults>::finished, this, &ScenariosRunnerPrivate::onFinishedRunningScenario, Qt::QueuedConnection);
        auto * pScenario = m_filteredScenarios[i];
        m_scenarioWatchers[i]->setFuture(QtConcurrent::run(&m_threadPool, [pScenario]()
            {
                QEventLoop eventLoop;
                QObject ctxObject;
                ScenarioRunResults results;
                QTimer::singleShot(0, &ctxObject, [&results, &eventLoop, pScenario](){results = ScenarioRunner::runScenario(*pScenario); eventLoop.exit();});
                eventLoop.exec();
                return results;
            }));
    }
}

void ScenariosRunnerPrivate::processScenariosResults()
{
    for (auto & watcher : m_scenarioWatchers)
    {
        auto scenarioPaths = watcher->result().scenarioPaths();
        for (auto it = scenarioPaths.cbegin(), end = scenarioPaths.cend(); it != end; ++it)
        {
            auto & scenarioPath = m_results[it->pathToLeafSection().top()];
            scenarioPath.pathToLeafSection() = it->pathToLeafSection();
            scenarioPath.infoMessages() = it->infoMessages();
            scenarioPath.fatalMessage() = it->fatalMessage();
            if (it->fatalMessage().isEmpty()) [[likely]]
                ++m_successfulScenarioPathRunCounter;
            else [[unlikely]]
                ++m_unsuccessfulScenarioPathRunCounter;
            scenarioPath.successfulRequireCount() += it->successfulRequireCount();
            m_successfulRequireCounter += it->successfulRequireCount();
            scenarioPath.unsuccessfulRequireCount() += it->unsuccessfulRequireCount();
            m_unsuccessfulRequireCounter += it->unsuccessfulRequireCount();
            scenarioPath.runCount() += it->runCount();
            scenarioPath.elapsedTimeInNSecs() += it->elapsedTimeInNSecs();
            m_elapsedTimeInNSecs += it->elapsedTimeInNSecs();
        }
    }
}

void ScenariosRunnerPrivate::printResults()
{
    QString buffer;
    buffer.reserve(1ul << 20);
    QTextStream bufferedStream(&buffer, QIODevice::WriteOnly);
    if (m_repetitionCount > 0) [[unlikely]]
        QTextStream(stdout) << "Repeated tests " << m_repetitionCounter << " times." << Qt::endl;
    printSuccessfullScenariosPaths(bufferedStream);
    printUnsuccessfullScenariosPaths(bufferedStream);
    printScenarioPathsStats(bufferedStream);
    QTextStream(stdout) << buffer << Qt::endl;
}

void ScenariosRunnerPrivate::printSuccessfullScenariosPaths(QTextStream & stream)
{
    if (m_successfulScenarioPathRunCounter == 0) [[unlikely]]
        return;
    stream << "\n------------------------------------------\n";
    stream << "Passed scenario paths\n";
    stream << "------------------------------------------\n";
    for (auto it = m_results.cbegin(), end = m_results.cend(); it != end; ++it)
    {
        if (it->fatalMessage().isEmpty()) [[likely]]
        {
            for (const auto & section : it->pathToLeafSection())
            {
                stream << section->name() << Qt::endl;
                const auto infoMessages = it->infoMessages()[section];
                for (const auto & infoMsg : infoMessages)
                    stream << infoMsg << Qt::endl;
            }
            stream << "Stats: [Time: " << QString::number(it->elapsedTimeInNSecs()/1000000.0, 'g', 3) << "ms; Run count: " << it->runCount() << "; Require count: " << it->successfulRequireCount() << ']' << Qt::endl;
            stream << Qt::endl;
        }
    }
}

void ScenariosRunnerPrivate::printUnsuccessfullScenariosPaths(QTextStream & stream)
{
    if (m_unsuccessfulScenarioPathRunCounter == 0) [[likely]]
        return;
    stream << "\n------------------------------------------\n";
    stream << "Failed scenario paths\n";
    stream << "------------------------------------------\n";
    for (auto it = m_results.cbegin(), end = m_results.cend(); it != end; ++it)
    {
        if (!it->fatalMessage().isEmpty()) [[likely]]
        {
            for (const auto & section : it->pathToLeafSection())
                stream << section->name() << Qt::endl;
            stream << it->fatalMessage() << Qt::endl;
            stream << "Stats: [Time: " << QString::number(it->elapsedTimeInNSecs()/1000000.0, 'g', 3) << "ms; Run count: " << it->runCount() << "; Successful require count: " << it->successfulRequireCount() << ']' << Qt::endl;
            stream << Qt::endl;
        }
    }
}

void ScenariosRunnerPrivate::printScenarioPathsStats(QTextStream & stream)
{
    stream << "\n------------------------------------------\n";
    stream << "Scenario paths stats\n";
    stream << "------------------------------------------\n";
    stream << "Total scenarios paths ran: " << m_successfulScenarioPathRunCounter + m_unsuccessfulScenarioPathRunCounter << '\n';
    stream << "Successful scenarios paths ran: " << m_successfulScenarioPathRunCounter << '\n';
    stream << "Unsuccessful scenarios paths ran: " << m_unsuccessfulScenarioPathRunCounter << '\n';
    stream << "Total requires in scenarios paths: " << m_successfulRequireCounter + m_unsuccessfulRequireCounter << '\n';
    stream << "Successful requires in scenarios paths: " << m_successfulRequireCounter << '\n';
    stream << "Unsuccessful requires in scenarios paths: " << m_unsuccessfulRequireCounter << '\n';
    stream << "Time: " << QString::number(m_elapsedTimeInNSecs/1000000.0, 'g', 3) << "ms\n";
    if (m_unsuccessfulScenarioPathRunCounter == 0 && m_unsuccessfulRequireCounter == 0)
        stream << "\nAll " << m_successfulScenarioPathRunCounter << " scenarios paths passed.\n\n";
    else
        stream << "\nThere are " << m_unsuccessfulScenarioPathRunCounter << " failing scenarios paths.\n\n";
}

}
