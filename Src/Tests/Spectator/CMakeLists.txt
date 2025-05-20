#
# Copyright (C) 2024 Glauco Pacheco <glauco@kourier.io>
# All rights reserved.
#
qt_add_library(Spectator STATIC
    Spectator.cpp
    Spectator.h)

find_package(Qt6 COMPONENTS Core Concurrent REQUIRED)

target_link_libraries(Spectator
    PRIVATE
    Qt::Core
    Qt::Concurrent)
