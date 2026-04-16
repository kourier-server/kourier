// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#ifndef SPECTATOR_SCENARIOS_RUNNER_H
#define SPECTATOR_SCENARIOS_RUNNER_H

#include "SpectatorGlobals.h"
#include <memory>

namespace Spectator
{

class ScenariosRunnerPrivate;

class SPECTATOR_EXPORT ScenariosRunner
{
public:
    ScenariosRunner();
    ~ScenariosRunner();
    void runScenarios();

private:
    std::unique_ptr<ScenariosRunnerPrivate> m_pScenariosRunnerPrivate;
};

}

#endif // SPECTATOR_SCENARIOS_RUNNER_H
