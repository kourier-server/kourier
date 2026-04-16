// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#include "ScenarioRepository.h"
#include "NoDestroy.h"
#include <QMutexLocker>
#include <QtLogging>

namespace Spectator
{

void ScenarioRepository::addScenario(Scenario * pScenario)
{
    assert(pScenario);
    QMutexLocker locker(&m_scenarioAddLock);
    if (!m_scenarios.contains(pScenario)) [[likely]]
        m_scenarios.append(pScenario);
    else [[unlikely]]
        qFatal("Failed to add scenario to repository. Scenario has already been added and scenarios can only be added once.");
}

qsizetype ScenarioRepository::size()
{
    return m_scenarios.size();
}

Scenario & ScenarioRepository::operator[](qsizetype index)
{
    assert(0 <= index && index < m_scenarios.size());
    return *m_scenarios[index];
}

ScenarioRepository & ScenarioRepository::global()
{
    static NoDestroy<ScenarioRepository*> instance(new ScenarioRepository);
    static NoDestroyPtrDeleter<ScenarioRepository*> instanceDeleter(instance);
    auto *pInstance = instance();
    if (pInstance) [[likely]]
        return *pInstance;
    else [[unlikely]]
        qFatal("Failed to fetch global instance of scenario repository. Program is terminating and the instance has already been destroyed.");
}

}
