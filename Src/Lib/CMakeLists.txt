#
# Copyright (C) 2024 Glauco Pacheco <glauco@kourier.io>
# All rights reserved.
#
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    qt_add_library(Lib SHARED
        $<TARGET_OBJECTS:KourierCore>
        $<TARGET_OBJECTS:KourierServer>
        $<TARGET_OBJECTS:KourierHttpServer>)
    set_property(TARGET Lib PROPERTY OUTPUT_NAME Kourier)
    target_include_directories(Lib PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
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
