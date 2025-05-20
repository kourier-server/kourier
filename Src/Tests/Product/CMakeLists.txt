#
# Copyright (C) 2024 Glauco Pacheco <glauco@kourier.io>
# All rights reserved.
#
cmake_minimum_required(VERSION 3.19 FATAL_ERROR)
cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0083 NEW)

project(ProductTests)
if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "ProductTests can only be built on Linux with AVX2 support.")
endif()
set(CMAKE_INSTALL_SO_NO_EXE False)
enable_language(CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_POSITION_INDEPENDENT_CODE True)
# Disable RTTI
string(REGEX REPLACE "-frtti" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
find_package(Qt6 REQUIRED COMPONENTS Core)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
#
# Setting global compiler options
#
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_C_FLAGS "${CFLAGS} ${CMAKE_C_FLAGS} -pipe -ftls-model=initial-exec -fvisibility=hidden -fstack-protector-strong -fstack-check -march=x86-64-v3 -msse2 -msse3 -mssse3 -msse4.1 -msse4.2 -mavx -mavx2")
    set(CMAKE_CXX_FLAGS "${CXXFLAGS} ${CMAKE_CXX_FLAGS} -pipe -ftls-model=initial-exec -fno-rtti -fvisibility=hidden -fvisibility-inlines-hidden -fstack-protector-strong -fstack-check -march=x86-64-v3 -msse2 -msse3 -mssse3 -msse4.1 -msse4.2 -mavx -mavx2")
    add_link_options(-pipe -ftls-model=initial-exec)
    if (NOT (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
        add_link_options(LINKER:-z,noexecstack,-z,relro,-z,now)
        add_link_options(-Wl,--enable-new-dtags -Wl,--demangle)
    endif ()
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION False)
        add_compile_options(-fno-omit-frame-pointer)
        if (ENABLE_SANITIZERS)
            message("Building a sanitizer-intrumented build")
            add_compile_definitions(SANITIZED_BUILD)
            add_compile_options(-fsanitize=address,undefined,leak)
            add_link_options(-fsanitize=address,undefined,leak --param=max-vartrack-size=0)
        endif()
    else()
        message("Enabling IPO.")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION False)
        add_compile_options(-O2 -flto -pipe -fomit-frame-pointer)
        add_link_options(-O2 -flto)
    endif()
    set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O2")
    set(CMAKE_C_FLAGS_RELEASE "-DNDEBUG -O2")
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install-root")
endif()
add_subdirectory(EmitterLibrary)
add_subdirectory(ReceiverLibrary)
qt_add_executable(ProductTests)
target_sources(ProductTests PRIVATE
    Product.spec.cpp
    ../Resources/TlsTestCertificates.cpp
    ../Resources/TlsTestCertificates.h
    ../Spectator/Spectator.cpp
    ../Spectator/Spectator.h)
find_package(Qt6 COMPONENTS Core Concurrent Network REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Kourier REQUIRED)
target_link_libraries(ProductTests PUBLIC
    EmitterLib
    ReceiverLib
    Qt::Core
    Qt::Concurrent
    Qt::Network
    OpenSSL::SSL
    Kourier::Lib)
