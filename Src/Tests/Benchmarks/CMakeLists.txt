#
# Copyright (C) 2025 Glauco Pacheco <glauco@kourier.io>
# All rights reserved.
#
qt_add_executable(Benchmarks)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(Benchmarks PRIVATE
        ../../Core/Object.bench.cpp
        ../../Core/TcpSocket.bench.cpp
        ../../Core/Timer.bench.cpp
        ../../Core/TlsSocket.bench.cpp
        ../../Http/HttpRequestParser.bench.cpp
        ../../Http/HttpServer.bench.cpp)
    target_link_libraries(Benchmarks
        PRIVATE
        KourierServer
        KourierHttpServer)
endif()
find_package(Qt6 COMPONENTS Core Concurrent Network REQUIRED)
target_link_libraries(Benchmarks
    PRIVATE
    Spectator
    KourierCore
    TestResources
    Qt::Core
    Qt::Concurrent
    Qt::Network)
target_include_directories(Benchmarks PRIVATE ../Spectator ../..)
target_precompile_headers(Benchmarks PRIVATE
    <Spectator.h>
    <QtCore>
    <QtConcurrent>
    <QtNetwork>)
