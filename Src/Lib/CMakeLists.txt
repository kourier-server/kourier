#
# Copyright (C) 2024 Glauco Pacheco <glauco@kourier.io>
# All rights reserved.
#
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    qt_add_library(Lib SHARED
        ../Core/AsyncQObject.h
        ../Core/EpollEventNotifier.cpp
        ../Core/EpollEventNotifier.h
        ../Core/EpollEventSource.h
        ../Core/EpollObjectDeleter.cpp
        ../Core/EpollObjectDeleter.h
        ../Core/EpollReadyEventSourceRegistrar.cpp
        ../Core/EpollReadyEventSourceRegistrar.h
        ../Core/EpollTimerRegistrar.cpp
        ../Core/EpollTimerRegistrar.h
        ../Core/HostAddressFetcher.cpp
        ../Core/HostAddressFetcher.h
        ../Core/IOChannel.cpp
        ../Core/IOChannel.h
        ../Core/MetaTypeSystem.cpp
        ../Core/MetaTypeSystem.h
        ../Core/NoDestroy.h
        ../Core/Object.cpp
        ../Core/Object.h
        ../Core/SDK.h
        ../Core/RingBuffer.cpp
        ../Core/RingBuffer.h
        ../Core/RingBufferBIO.cpp
        ../Core/RingBufferBIO.h
        ../Core/RuntimeError.cpp
        ../Core/RuntimeError.h
        ../Core/SimdIterator.cpp
        ../Core/SimdIterator.h
        ../Core/TcpSocket.cpp
        ../Core/TcpSocket.h
        ../Core/TcpSocketDataSink.cpp
        ../Core/TcpSocketDataSink.h
        ../Core/TcpSocketDataSource.cpp
        ../Core/TcpSocketDataSource.h
        ../Core/TcpSocketPrivate_epoll.cpp
        ../Core/TcpSocketPrivate_epoll.h
        ../Core/Timer.h
        ../Core/Timer_epoll.cpp
        ../Core/TimerPrivate_epoll.cpp
        ../Core/TimerPrivate_epoll.h
        ../Core/TlsConfiguration.cpp
        ../Core/TlsConfiguration.h
        ../Core/TlsContext.cpp
        ../Core/TlsContext.h
        ../Core/TlsError.cpp
        ../Core/TlsError.h
        ../Core/TlsSocket.cpp
        ../Core/TlsSocket.h
        ../Core/TlsSocketDataSink.cpp
        ../Core/TlsSocketDataSink.h
        ../Core/TlsSocketDataSource.cpp
        ../Core/TlsSocketDataSource.h
        ../Core/TlsSocketPrivate_epoll.cpp
        ../Core/TlsSocketPrivate_epoll.h
        ../Core/UnixSignalListener.cpp
        ../Core/UnixSignalListener.h
        ../Core/UnixUtils.cpp
        ../Core/UnixUtils.h
        ../Http/ErrorHandler.h
        ../Http/HttpBroker.cpp
        ../Http/HttpBroker.h
        ../Http/HttpBrokerPrivate.cpp
        ../Http/HttpBrokerPrivate.h
        ../Http/HttpChunkMetadataParser.cpp
        ../Http/HttpChunkMetadataParser.h
        ../Http/HttpConnectionHandler.cpp
        ../Http/HttpConnectionHandler.h
        ../Http/HttpConnectionHandlerFactory.cpp
        ../Http/HttpConnectionHandlerFactory.h
        ../Http/HttpFieldBlock.cpp
        ../Http/HttpFieldBlock.h
        ../Http/HttpRequest.cpp
        ../Http/HttpRequest.h
        ../Http/HttpRequestBody.h
        ../Http/HttpRequestLimits.h
        ../Http/HttpRequestLine.h
        ../Http/HttpRequestParser.cpp
        ../Http/HttpRequestParser.h
        ../Http/HttpRequestPrivate.h
        ../Http/HttpRequestRouter.cpp
        ../Http/HttpRequestRouter.h
        ../Http/HttpServer.cpp
        ../Http/HttpServer.h
        ../Http/HttpServerOptions.cpp
        ../Http/HttpServerOptions.h
        ../Http/HttpServerPrivate.cpp
        ../Http/HttpServerPrivate.h
        ../Http/HttpServerWorker.h
        ../Http/HttpServerWorkerFactory.cpp
        ../Http/HttpServerWorkerFactory.h
        ../Server/AsyncServerWorker.h
        ../Server/ConnectionHandler.cpp
        ../Server/ConnectionHandler.h
        ../Server/ConnectionHandlerFactory.h
        ../Server/ConnectionHandlerRepository.cpp
        ../Server/ConnectionHandlerRepository.h
        ../Server/ConnectionListener.cpp
        ../Server/ConnectionListener.h
        ../Server/ExecutionState.h
        ../Server/QTcpServerBasedConnectionListener.cpp
        ../Server/QTcpServerBasedConnectionListener.h
        ../Server/QTcpServerBasedConnectionListenerPrivate.cpp
        ../Server/QTcpServerBasedConnectionListenerPrivate.h
        ../Server/Server.cpp
        ../Server/Server.h
        ../Server/ServerError.h
        ../Server/ServerWorker.cpp
        ../Server/ServerWorker.h
        ../Server/ServerWorkerFactory.h
    )
    set_property(TARGET Lib PROPERTY OUTPUT_NAME Kourier)
    target_compile_definitions(Lib PRIVATE KOURIER_LIBRARY)
    find_package(Qt6 COMPONENTS Core Concurrent Network REQUIRED)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(Lib PUBLIC
        Qt::Core
        Qt::Concurrent
        Qt::Network
        OpenSSL::SSL)
    target_precompile_headers(Lib PRIVATE
        <QtCore>
        <QtConcurrent>
        <QtNetwork>)
    include(GNUInstallDirs)
    target_include_directories(Lib PUBLIC "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
    install(TARGETS Lib
        EXPORT Kourier
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(FILES
        ../Core/SDK.h
        ../Core/TlsConfiguration.h
        ../Core/TcpSocket.h
        ../Core/IOChannel.h
        ../Core/RingBuffer.h
        ../Core/Object.h
        ../Core/MetaTypeSystem.h
        ../Core/NoDestroy.h
        ../Core/EpollEventNotifier.h
        ../Core/TlsSocket.h
        ../Core/Timer.h
        ../Core/UnixSignalListener.h
        DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}/00-Private/Core)
    install(FILES
        ../Http/ErrorHandler.h
        ../Http/HttpServer.h
        ../Http/HttpRequest.h
        ../Http/HttpBroker.h
        DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}/00-Private/Http)
    install(FILES
        Kourier
        DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR})
    install(EXPORT Kourier
        FILE KourierConfig.cmake
        NAMESPACE Kourier::
        DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake)
    include(CMakePackageConfigHelpers)
    configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/KourierConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)
    include(CMakeFindDependencyMacro)
    find_dependency(Qt6 COMPONENTS Core Concurrent Network REQUIRED)
    find_dependency(OpenSSL REQUIRED)
    set(version 1.0.0)
    set_property(TARGET Lib PROPERTY VERSION ${version})
    set_property(TARGET Lib PROPERTY SOVERSION 1)
    set_property(TARGET Lib PROPERTY INTERFACE_Lib_MAJOR_VERSION 1)
    set_property(TARGET Lib APPEND PROPERTY COMPATIBLE_INTERFACE_STRING Lib_MAJOR_VERSION)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/KourierConfigVersion.cmake"
        VERSION "${version}"
        COMPATIBILITY AnyNewerVersion)
    export(EXPORT Kourier
        FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/KourierConfig.cmake"
        NAMESPACE Kourier::)
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/KourierConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/KourierConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)
endif()
