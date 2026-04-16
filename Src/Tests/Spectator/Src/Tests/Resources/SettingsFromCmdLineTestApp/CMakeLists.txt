# Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

qt_add_executable(SettingsFromCmdLineTestApp)
target_sources(SettingsFromCmdLineTestApp PRIVATE main.cpp)
find_package(Qt6 COMPONENTS Core Concurrent REQUIRED)
target_include_directories(SettingsFromCmdLineTestApp PRIVATE ../../../Lib/)
target_link_libraries(SettingsFromCmdLineTestApp
    PRIVATE
    SpectatorObjs
    Qt::Core
    Qt::Concurrent)
