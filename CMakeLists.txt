#
# Copyright (C) 2024 Glauco Pacheco <glauco@kourier.io>
# All rights reserved.
#

cmake_minimum_required(VERSION 3.19 FATAL_ERROR)
cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0083 NEW)

project(Kourier)
set(CMAKE_INSTALL_SO_NO_EXE False)
enable_language(CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_POSITION_INDEPENDENT_CODE True)
# Disable RTTI
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    string(REGEX REPLACE "/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")
else ()
    string(REGEX REPLACE "-frtti" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif ()
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
            #[[
            I use a sanitized OpenSSL/Qt build.

            Sanitized Qt is compiled with the following options:
            export CC=clang-18
            export CXX=clang++-18
            export ASAN_OPTIONS=halt_on_error=0
            configure -prefix ***prefix*** -release -force-debug-info -static -linker lld -no-gui -no-widgets -no-dbus --
            -DCMAKE_CXX_STANDARD=20
            -DCMAKE_C_FLAGS="-fsanitize=address,undefined,leak"
            -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined,leak"
            -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address,undefined,leak --param=max-vartrack-size=0"
            -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined,leak --param=max-vartrack-size=0"
            cmake --build . --parallel 12
            cmake --install .

            Sanitized OpenSSL is build with the following options:
            export CC=clang-18
            export CXX=clang++-18
            export ASAN_OPTIONS=halt_on_error=0
            Configure --prefix=***prefix*** --release -no-shared no-tests no-ssl3 no-comp -fsanitize=address,undefined,leak -g3
            make -j12
            make install
            ]]
            string(REGEX REPLACE "-fno-rtti" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -frtti")
            add_compile_definitions(SANITIZED_BUILD)
            add_compile_options(-fsanitize=address,undefined,leak)
            add_link_options(-fsanitize=address,undefined,leak --param=max-vartrack-size=0)
        endif()
    else()
        if (ANDROID OR IOS)
            message("We're on mobile: disabling IPO.")
            set(CMAKE_INTERPROCEDURAL_OPTIMIZATION False)
            add_compile_options(-O2 -pipe -fomit-frame-pointer)
            add_link_options(-O2)
        else()
            message("Enabling IPO.")
            set(CMAKE_INTERPROCEDURAL_OPTIMIZATION False)
            add_compile_options(-O2 -flto -pipe -fomit-frame-pointer)
            add_link_options(-O2 -flto)
        endif()
    endif()
    set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O2")
    set(CMAKE_C_FLAGS_RELEASE "-DNDEBUG -O2")
endif()
if (MSVC)
    message("Compiling with Microsoft C/C++ Compiler")
    if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        message("Enabling link-time optimization on MSVC.")
        add_compile_options(/GL)
        add_link_options(/LTCG)
    endif()
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install-root")
endif()

if (WIN32)
    if (RELEASE_WITH_DEBUG_CPP_RUNTIME_DLL)
        set(CMAKE_RELEASE_POSTFIX d CACHE INTERNAL "")
        set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDebugDLL CACHE INTERNAL "")
        set(CMAKE_MAP_IMPORTED_CONFIG_RELEASE "Debug;" CACHE INTERNAL "")
    endif ()
endif ()

add_subdirectory(Src)
