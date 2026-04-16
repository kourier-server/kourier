# Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

qt_add_executable(SpectatorVisitsAllLeafNodesOfScenarioPathWithoutGeneratorsOnceTestApp)
target_sources(SpectatorVisitsAllLeafNodesOfScenarioPathWithoutGeneratorsOnceTestApp PRIVATE
    main.cpp
    Scenario.cpp
    ../SectionEntryRecorder.cpp
    ../SectionEntryRecorder.h)
find_package(Qt6 COMPONENTS Core Concurrent REQUIRED)
target_include_directories(SpectatorVisitsAllLeafNodesOfScenarioPathWithoutGeneratorsOnceTestApp PRIVATE ../../../Lib/)
target_link_libraries(SpectatorVisitsAllLeafNodesOfScenarioPathWithoutGeneratorsOnceTestApp
    PRIVATE
    SpectatorObjs
    Qt::Core
    Qt::Concurrent)
