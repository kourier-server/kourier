# Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

qt_add_library(SpectatorObjs OBJECT
    Generator.cpp
    Generator.h
    GlobalScopeData.cpp
    GlobalScopeData.h
    MacroHelpers.h
    NoDestroy.h
    Scenario.cpp
    Scenario.h
    ScenarioFilter.h
    ScenarioPathRunResults.cpp
    ScenarioPathRunResults.h
    ScenarioRepository.cpp
    ScenarioRepository.h
    ScenarioRunner.cpp
    ScenarioRunner.h
    ScenarioRunResults.cpp
    ScenarioRunResults.h
    ScenariosRunner.cpp
    ScenariosRunner.h
    ScenariosRunnerPrivate.cpp
    ScenariosRunnerPrivate.h
    Section.cpp
    Section.h
    SectionGuard.cpp
    SectionGuard.h
    Settings.cpp
    Settings.h
    Spectator.cpp
    Spectator.h
    SpectatorException.h
    SpectatorGlobals.h)
target_compile_definitions(SpectatorObjs PUBLIC
    SPECTATOR_LIBRARY
    CONFIGURATION_VERSION_STRING=${CONFIGURATION_VERSION_STRING})
find_package(Qt6 COMPONENTS Core Concurrent REQUIRED)
target_link_libraries(SpectatorObjs
    PRIVATE
    Qt::Core
    Qt::Concurrent)
qt_add_library(Spectator SHARED $<TARGET_OBJECTS:SpectatorObjs>)
target_include_directories(Spectator PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
add_library(Kourier::Spectator ALIAS Spectator)
target_link_libraries(Spectator
    PRIVATE
    Qt::Core
    Qt::Concurrent
    INTERFACE
    Qt::Core
    Qt::Concurrent)
include(GNUInstallDirs)
    install(TARGETS Spectator
        EXPORT Spectator
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(FILES
        SpectatorGlobals.h
        Generator.h
        MacroHelpers.h
        Scenario.h
        ScenariosRunner.h
        Section.h
        SectionGuard.h
        Spectator.h
        DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}/Private)
    install(FILES
        ${CMAKE_CURRENT_LIST_DIR}/Spectator_install
        DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}
        RENAME
        Spectator)
    install(EXPORT Spectator
        FILE SpectatorConfig.cmake
        NAMESPACE Kourier::
        DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake)
    include(CMakePackageConfigHelpers)
    configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/SpectatorConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)
    include(CMakeFindDependencyMacro)
    find_dependency(Qt6 COMPONENTS Core Concurrent REQUIRED)
    set(version 1.0.0)
    set_property(TARGET Spectator PROPERTY VERSION ${version})
    set_property(TARGET Spectator PROPERTY SOVERSION 1)
    set_property(TARGET Spectator PROPERTY INTERFACE_Lib_MAJOR_VERSION 1)
    set_property(TARGET Spectator APPEND PROPERTY COMPATIBLE_INTERFACE_STRING Lib_MAJOR_VERSION)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/SpectatorConfigVersion.cmake"
        VERSION "${version}"
        COMPATIBILITY AnyNewerVersion)
    export(EXPORT Spectator
        FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/SpectatorConfig.cmake"
        NAMESPACE Kourier::)
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/SpectatorConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/SpectatorConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)
