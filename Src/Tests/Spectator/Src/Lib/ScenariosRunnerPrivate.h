// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#ifndef SPECTATOR_SCENARIOS_RUNNER_PRIVATE_H
#define SPECTATOR_SCENARIOS_RUNNER_PRIVATE_H

#include "Scenario.h"
#include "ScenarioRunResults.h"
#include "Section.h"
#include <QtClassHelperMacros>
#include <QVector>
#include <QThreadPool>
#include <QFutureWatcher>
#include <QObject>
#include <QtTypes>
#include <QMap>
#include <QTextStream>
#include <memory>

namespace Spectator
{

class ScenariosRunnerPrivate : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ScenariosRunnerPrivate)
public:
    ScenariosRunnerPrivate() = default;
    ~ScenariosRunnerPrivate() = default;
    void runScenarios();

private slots:
    void onFinishedRunningScenario();

private:
    QVector<Scenario*> fetchScenarios();
    void scheduleFileteredScenariosExecution();
    void processScenariosResults();
    void printResults();
    void printSuccessfullScenariosPaths(QTextStream & stream);
    void printUnsuccessfullScenariosPaths(QTextStream & stream);
    void printScenarioPathsStats(QTextStream & stream);

private:
    QThreadPool m_threadPool;
    QVector<Scenario*> m_filteredScenarios;
    QVector<std::shared_ptr<QFutureWatcher<ScenarioRunResults>>> m_scenarioWatchers;
    QMap<Section const *, ScenarioPathRunResults> m_results;
    qsizetype m_finishedRunningScenariosCounter = 0;
    qsizetype m_successfulRequireCounter = 0;
    qsizetype m_successfulScenarioPathRunCounter = 0;
    qsizetype m_unsuccessfulRequireCounter = 0;
    qsizetype m_unsuccessfulScenarioPathRunCounter = 0;
    qsizetype m_elapsedTimeInNSecs = 0;
    qsizetype m_repetitionCount = 0;
    qsizetype m_repetitionCounter = 0;
    bool m_hasRanScenarios = false;
};

}

#endif // SPECTATOR_SCENARIOS_RUNNER_PRIVATE_H
