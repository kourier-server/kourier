// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#ifndef SPECTATOR_SCENARIO_FILTER_H
#define SPECTATOR_SCENARIO_FILTER_H

#include "Settings.h"
#include "Scenario.h"
#include <QString>
#include <QStringList>

namespace Spectator
{

class ScenarioFilter
{
public:
    ScenarioFilter(const Settings &settings) :
        m_sourceFileToRun(settings.filePathFilter()),
        m_scenarioNameToRun(settings.scenarioNameFilter()),
        m_tagsToRun(settings.scenarioTagsFilter()) {}
    ~ScenarioFilter() = default;
    bool hasToRunScenario(Scenario const * const pScenario) const
    {
        return (m_sourceFileToRun.isEmpty() || pScenario->sourceFile() == m_sourceFileToRun)
                && (m_scenarioNameToRun.isEmpty() || m_scenarioNameToRun == pScenario->name())
                && (m_tagsToRun.isEmpty() || hasTag(pScenario));
    }

private:
    bool hasTag(Scenario const * const pScenario) const
    {
        for (qsizetype idx = 0; idx < pScenario->tagCount(); ++idx)
        {
            if (m_tagsToRun.contains(pScenario->tagAt(idx)))
                return true;
        }
        return false;
    }

private:
    QString m_sourceFileToRun;
    QString m_scenarioNameToRun;
    QStringList m_tagsToRun;
};

}

#endif // SPECTATOR_SCENARIO_FILTER_H
