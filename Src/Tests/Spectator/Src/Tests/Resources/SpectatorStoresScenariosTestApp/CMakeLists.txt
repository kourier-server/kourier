# Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

qt_add_executable(SpectatorStoresScenariosTestApp)
target_sources(SpectatorStoresScenariosTestApp PRIVATE
    main.cpp
    scenarios_1.cpp
    scenarios_2.cpp
    scenarios_3.cpp)
find_package(Qt6 COMPONENTS Core Concurrent REQUIRED)
target_include_directories(SpectatorStoresScenariosTestApp PRIVATE ../../../Lib/)
target_link_libraries(SpectatorStoresScenariosTestApp
    PRIVATE
    SpectatorObjs
    Qt::Core
    Qt::Concurrent)
