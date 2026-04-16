// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#include "ScenarioRunResults.h"

namespace Spectator
{

void ScenarioRunResults::addScenarioPath(QStack<Section const *> pathToLeafSection,
                                         QMap<Section const *, QSet<QString>> infoMessages,
                                         QString fatalMessage,
                                         qsizetype successfulRequireCount,
                                         qsizetype unsuccessfulRequireCount,
                                         qsizetype runCount,
                                         qint64 elapsedTimeInNSecs)
{
    auto & scenarioPath = m_scenarioPaths[pathToLeafSection.top()];
    scenarioPath.pathToLeafSection() = pathToLeafSection;
    scenarioPath.infoMessages() = infoMessages;
    scenarioPath.fatalMessage() = fatalMessage;
    scenarioPath.successfulRequireCount() += successfulRequireCount;
    scenarioPath.unsuccessfulRequireCount() += unsuccessfulRequireCount;
    scenarioPath.runCount() += runCount;
    scenarioPath.elapsedTimeInNSecs() += elapsedTimeInNSecs;
}

}
