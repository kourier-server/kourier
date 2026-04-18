# Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.19 FATAL_ERROR)
project(ProductTests)
if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "ProductTests can only be built on Linux with AVX2 support.")
endif()
enable_language(CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CheckPIESupported)
check_pie_supported()
set (CMAKE_POSITION_INDEPENDENT_CODE True)
include(CheckIPOSupported)
check_ipo_supported(RESULT HAS_IPO)
if (HAS_IPO AND (NOT (CMAKE_BUILD_TYPE STREQUAL "Debug")))
    message("Enabling IPO.")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION True)
else()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION False)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
# Set compiler options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O2")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELEASE} ${CMAKE_CXX_FLAGS_DEBUG}")
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pipe")
    add_link_options(-pipe)
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" AND (NOT (CMAKE_BUILD_TYPE STREQUAL "Debug")))
        message("Enabling -ftls-model=initial-exec -march=x86-64-v3 -mavx2.")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftls-model=initial-exec -march=x86-64-v3 -mavx2")
        add_link_options(-ftls-model=initial-exec)
    endif()
endif()
add_subdirectory(EmitterLibrary)
add_subdirectory(ReceiverLibrary)
qt_add_executable(ProductTests)
target_sources(ProductTests PRIVATE
    main.cpp
    Product.spec.cpp
    ../Resources/TestHostNamesFetcher.cpp
    ../Resources/TestHostNamesFetcher.h
    ../Resources/TlsTestCertificates.cpp
    ../Resources/TlsTestCertificates.h)
find_package(Qt6 COMPONENTS Core Concurrent Network REQUIRED)
find_package(OpenSSL REQUIRED)
if (DEFINED Spectator_DIR OR DEFINED ENV{Spectator_DIR})
  find_package(Spectator)
else ()
  include(FetchContent)
  FetchContent_Declare(
    Spectator
    GIT_REPOSITORY https://github.com/GlaucoPacheco/Spectator.git
    GIT_TAG        8535939ab3ad1ffb6f055c7b8a92ca917395a2af)
  FetchContent_MakeAvailable(Spectator)
endif ()
if (DEFINED Kourier_DIR OR DEFINED ENV{Kourier_DIR})
  find_package(Kourier)
else ()
  include(FetchContent)
  FetchContent_Declare(
    Spectator
    GIT_REPOSITORY https://github.com/kourier-server/kourier.git
    GIT_TAG        8535939ab3ad1ffb6f055c7b8a92ca917395a2af)
  FetchContent_MakeAvailable(Kourier)
endif ()
target_link_libraries(ProductTests PUBLIC
    EmitterLib
    ReceiverLib
    Kourier::Spectator
    Qt::Core
    Qt::Concurrent
    Qt::Network
    OpenSSL::SSL
    Kourier::Lib)
