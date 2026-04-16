# Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

qt_add_executable(SpectatorKeepsVisitingLeafNodeOfScenarioPathUntilExhaustingDataOfAllGeneratorsOnPathTestApp)
target_sources(SpectatorKeepsVisitingLeafNodeOfScenarioPathUntilExhaustingDataOfAllGeneratorsOnPathTestApp PRIVATE
    ../GeneratorDataRecorder.cpp
    ../GeneratorDataRecorder.h
    main.cpp
    Scenario.cpp
    ../SectionEntryRecorder.cpp
    ../SectionEntryRecorder.h)
find_package(Qt6 COMPONENTS Core Concurrent REQUIRED)
target_include_directories(SpectatorKeepsVisitingLeafNodeOfScenarioPathUntilExhaustingDataOfAllGeneratorsOnPathTestApp PRIVATE ../../../Lib/)
target_link_libraries(SpectatorKeepsVisitingLeafNodeOfScenarioPathUntilExhaustingDataOfAllGeneratorsOnPathTestApp
    PRIVATE
    SpectatorObjs
    Qt::Core
    Qt::Concurrent)
