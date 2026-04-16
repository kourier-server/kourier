// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#include "ScenariosRunner.h"
#include "ScenariosRunnerPrivate.h"

namespace Spectator
{

ScenariosRunner::ScenariosRunner() : m_pScenariosRunnerPrivate(new ScenariosRunnerPrivate) {}

ScenariosRunner::~ScenariosRunner() = default;

void ScenariosRunner::runScenarios()
{
    m_pScenariosRunnerPrivate->runScenarios();
}

}
