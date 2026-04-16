# Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

qt_add_executable(SpectatorSupportsRequireOutsideScenarioScopeTestApp)
target_sources(SpectatorSupportsRequireOutsideScenarioScopeTestApp PRIVATE
    main.cpp)
find_package(Qt6 COMPONENTS Core Concurrent REQUIRED)
target_include_directories(SpectatorSupportsRequireOutsideScenarioScopeTestApp PRIVATE ../../../Lib/)
target_link_libraries(SpectatorSupportsRequireOutsideScenarioScopeTestApp
    PRIVATE
    SpectatorObjs
    Qt::Core
    Qt::Concurrent)
