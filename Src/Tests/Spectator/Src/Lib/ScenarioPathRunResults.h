// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#ifndef SPECTATOR_SCENARIO_PATH_RUN_RESULTS_H
#define SPECTATOR_SCENARIO_PATH_RUN_RESULTS_H

#include <QStack>
#include <QMap>
#include <QSet>
#include <QString>
#include <QtTypes>
#include <QSharedData>
#include <QSharedDataPointer>

namespace Spectator
{

class Section;

struct ScenarioPathRunResultsData : public QSharedData
{
    QStack<Section const *> pathToLeafSection;
    QMap<Section const *, QSet<QString>> infoMessages;
    QString fatalMessage;
    qsizetype successfulRequireCount = 0;
    qsizetype unsuccessfulRequireCount = 0;
    qsizetype runCount = 0;
    qint64 elapsedTimeInNSecs = 0;
};

class ScenarioPathRunResults
{
public:
    ScenarioPathRunResults();
    ScenarioPathRunResults(const ScenarioPathRunResults &) = default;
    ~ScenarioPathRunResults() = default;
    QStack<Section const *> pathToLeafSection() const {return m_d->pathToLeafSection;}
    QStack<Section const *> & pathToLeafSection() {return m_d->pathToLeafSection;}
    QMap<Section const *, QSet<QString>> infoMessages() const {return m_d->infoMessages;}
    QMap<Section const *, QSet<QString>> & infoMessages() {return m_d->infoMessages;}
    QString fatalMessage() const {return m_d->fatalMessage;}
    QString & fatalMessage() {return m_d->fatalMessage;}
    qsizetype successfulRequireCount() const {return m_d->successfulRequireCount;}
    qsizetype & successfulRequireCount() {return m_d->successfulRequireCount;}
    qsizetype unsuccessfulRequireCount() const {return m_d->unsuccessfulRequireCount;}
    qsizetype & unsuccessfulRequireCount() {return m_d->unsuccessfulRequireCount;}
    qsizetype runCount() const {return m_d->runCount;}
    qsizetype & runCount() {return m_d->runCount;}
    qint64 elapsedTimeInNSecs() const {return m_d->elapsedTimeInNSecs;}
    qint64 & elapsedTimeInNSecs() {return m_d->elapsedTimeInNSecs;}

private:
    QSharedDataPointer<ScenarioPathRunResultsData> m_d;
};

}

#endif // SPECTATOR_SCENARIO_PATH_RUN_RESULTS_H
