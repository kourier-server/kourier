// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#include "SectionGuard.h"
#include "Section.h"
#include "Scenario.h"
#include "ScenarioRunner.h"

namespace Spectator
{

SectionGuard::SectionGuard(Section const * const section, Scenario * pScenario) :
    m_pSection(section),
    m_pScenario(pScenario),
    m_hasEntered(m_pScenario->scenarioRunner()->tryPushSection(m_pSection))
{
    assert(m_pSection);
}
    
SectionGuard::~SectionGuard()
{
    if (m_hasEntered)
        m_pScenario->scenarioRunner()->popSection(m_pSection);
}

}
