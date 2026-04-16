# Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

qt_add_executable(SpectatorSupportsScenarioFilteringTestApp)
target_sources(SpectatorSupportsScenarioFilteringTestApp PRIVATE
    main.cpp
    scenarios.cpp
    scenario_on_another_file.cpp)
find_package(Qt6 COMPONENTS Core Concurrent REQUIRED)
target_include_directories(SpectatorSupportsScenarioFilteringTestApp PRIVATE ../../../Lib/)
target_link_libraries(SpectatorSupportsScenarioFilteringTestApp
    PRIVATE
    SpectatorObjs
    Qt::Core
    Qt::Concurrent)
