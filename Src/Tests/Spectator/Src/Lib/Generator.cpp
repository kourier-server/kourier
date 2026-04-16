// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#include "Generator.h"
#include "Scenario.h"
#include "ScenarioRunner.h"

namespace Spectator
{

Generator::Generator(qsizetype size, QStringView sourceFile, qint32 sourceLine) :
    m_size(size),
    m_sourceFile(sourceFile),
    m_sourceLine(sourceLine)
{
}

qsizetype Generator::getCurrentIndex(Scenario *pScenario, Generator const * const pGenerator)
{
    return pScenario->scenarioRunner()->getGeneratorIndex(pGenerator);
}

}
