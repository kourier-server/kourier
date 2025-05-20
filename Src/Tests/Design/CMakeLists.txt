#
# Copyright (C) 2024 Glauco Pacheco <glauco@kourier.io>
# All rights reserved.
#
qt_add_executable(DesignTests)
target_sources(DesignTests PRIVATE
    ../../Core/HostAddressFetcher.spec.cpp
    ../../Core/IOChannel.spec.cpp
    ../../Core/Object.spec.cpp
    ../../Core/RingBuffer.spec.cpp
    ../../Core/TlsContext.spec.cpp)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(DesignTests PRIVATE
        ../../Core/EpollEventSource.spec.cpp
        ../../Core/EpollObjectDeleter.spec.cpp
        ../../Core/EpollReadyEventSourceRegistrar.spec.cpp
        ../../Core/TcpSocket.spec.cpp
        ../../Core/Timer.spec.cpp
        ../../Core/TlsSocket.spec.cpp
        ../../Core/UnixSignalListener.spec.cpp
        ../../Http/HttpBrokerPrivate.spec.cpp
        ../../Http/HttpChunkMetadataParser.spec.cpp
        ../../Http/HttpConnectionHandler.spec.cpp
        ../../Http/HttpRequestParser.spec.cpp
        ../../Http/HttpRequestRouter.spec.cpp
        ../../Http/HttpServerOptions.spec.cpp
        ../../Http/HttpServer.spec.cpp
        ../../Server/AsyncServerWorker.spec.cpp
        ../../Server/ConnectionHandlerRepository.spec.cpp
        ../../Server/QTcpServerBasedConnectionListener.spec.cpp
        ../../Server/Server.spec.cpp
        ../../Server/ServerWorker.spec.cpp)
    target_link_libraries(DesignTests
        PRIVATE
        KourierServer
        KourierHttpServer)
endif()
find_package(Qt6 COMPONENTS Core Concurrent Network REQUIRED)
target_link_libraries(DesignTests
    PRIVATE
    Spectator
    KourierCore
    TestResources
    Qt::Core
    Qt::Concurrent
    Qt::Network)
target_include_directories(DesignTests PRIVATE ../Spectator ../..)
target_precompile_headers(DesignTests PRIVATE
    <Spectator.h>
    <QtCore>
    <QtConcurrent>
    <QtNetwork>)
add_subdirectory(UnixSignalListenerTestApp)
add_dependencies(DesignTests UnixSignalListenerTestApp)
