#
# Copyright (C) 2024 Glauco Pacheco <glauco@kourier.io>
# All rights reserved.
#
qt_add_executable(UnixSignalListenerTestApp)
target_sources(UnixSignalListenerTestApp PRIVATE main.cpp)
add_dependencies(UnixSignalListenerTestApp KourierCore)
find_package(Qt6 COMPONENTS Core Concurrent Network REQUIRED)
target_link_libraries(UnixSignalListenerTestApp PRIVATE
    KourierCore
    Qt::Core
    Qt::Concurrent
    Qt::Network)
