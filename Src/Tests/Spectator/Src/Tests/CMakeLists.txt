# Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

add_subdirectory(Resources)
qt_add_executable(Tests)
target_sources(Tests PRIVATE main.cpp)
find_package(Qt6 COMPONENTS Core Concurrent REQUIRED)
add_dependencies(Tests 
    SettingsFromCmdLineTestApp
    SpectatorStoresScenariosTestApp
    SpectatorSupportsInfoMessagesOutsideScenarioScopeTestApp
    SpectatorSupportsRequireOutsideScenarioScopeTestApp
    SpectatorVisitsAllLeafNodesOfScenarioPathWithoutGeneratorsOnceTestApp
    SpectatorKeepsVisitingLeafNodeOfScenarioPathUntilExhaustingDataOfAllGeneratorsOnPathTestApp
    SpectatorOutputsAllScenarioPathsRanTestApp
    SpectatorRunsAllScenariosOnSingleThreadByDefaultTestApp
    SpectatorSupportsRunningScenariosOnMultipleThreadsTestApp
    SpectatorSupportsScenarioFilteringTestApp)
target_link_libraries(Tests
    PRIVATE
    Spectator
    Qt::Core
    Qt::Concurrent)
