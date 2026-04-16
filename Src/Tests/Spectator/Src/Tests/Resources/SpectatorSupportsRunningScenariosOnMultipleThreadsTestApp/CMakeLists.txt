# Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

qt_add_executable(SpectatorSupportsRunningScenariosOnMultipleThreadsTestApp)
target_sources(SpectatorSupportsRunningScenariosOnMultipleThreadsTestApp PRIVATE
    main.cpp
    scenarios.cpp)
find_package(Qt6 COMPONENTS Core Concurrent REQUIRED)
target_include_directories(SpectatorSupportsRunningScenariosOnMultipleThreadsTestApp PRIVATE ../../../Lib/)
target_link_libraries(SpectatorSupportsRunningScenariosOnMultipleThreadsTestApp
    PRIVATE
    SpectatorObjs
    Qt::Core
    Qt::Concurrent)
