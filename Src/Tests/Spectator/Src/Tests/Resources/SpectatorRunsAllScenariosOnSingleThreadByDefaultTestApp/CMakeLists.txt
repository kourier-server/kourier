# Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

qt_add_executable(SpectatorRunsAllScenariosOnSingleThreadByDefaultTestApp)
target_sources(SpectatorRunsAllScenariosOnSingleThreadByDefaultTestApp PRIVATE
    main.cpp
    scenarios.cpp)
find_package(Qt6 COMPONENTS Core Concurrent REQUIRED)
target_include_directories(SpectatorRunsAllScenariosOnSingleThreadByDefaultTestApp PRIVATE ../../../Lib/)
target_link_libraries(SpectatorRunsAllScenariosOnSingleThreadByDefaultTestApp
    PRIVATE
    SpectatorObjs
    Qt::Core
    Qt::Concurrent)
