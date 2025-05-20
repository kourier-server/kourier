#
# Copyright (C) 2025 Glauco Pacheco <glauco@kourier.io>
# All rights reserved.
#

qt_add_library(EmitterLib SHARED
    Emitter.cpp
    Emitter.h)
target_compile_definitions(EmitterLib PRIVATE KOURIER_EXPORT_EMITTER_LIBRARY)
find_package(Qt6 COMPONENTS Core Concurrent Network REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Kourier REQUIRED)
target_link_libraries(EmitterLib PUBLIC
    Qt::Core
    Qt::Concurrent
    Qt::Network
    OpenSSL::SSL
    Kourier::Lib)
