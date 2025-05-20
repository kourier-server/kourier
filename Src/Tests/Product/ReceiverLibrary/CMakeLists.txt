#
# Copyright (C) 2025 Glauco Pacheco <glauco@kourier.io>
# All rights reserved.
#

qt_add_library(ReceiverLib SHARED
    Receiver.cpp
    Receiver.h)
target_compile_definitions(ReceiverLib PRIVATE KOURIER_EXPORT_RECEIVER_LIBRARY)
find_package(Qt6 COMPONENTS Core Concurrent Network REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Kourier REQUIRED)
target_link_libraries(ReceiverLib PUBLIC
    Qt::Core
    Qt::Concurrent
    Qt::Network
    OpenSSL::SSL
    Kourier::Lib)
