#
# Copyright (C) 2024 Glauco Pacheco <glaucopacheco@gmail.com>
# All rights reserved.
#

cmake_minimum_required(VERSION 3.19 FATAL_ERROR)

project(Kourier)
enable_language(CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CheckPIESupported)
check_pie_supported()
set (CMAKE_POSITION_INDEPENDENT_CODE True)
find_package(Qt6 REQUIRED COMPONENTS Core)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
#
# Setting global compiler options
#
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_C_FLAGS "${CFLAGS} ${CMAKE_C_FLAGS} -pipe -ftls-model=initial-exec -D_FORTIFY_SOURCE=3 -D_GLIBCXX_ASSERTIONS -ftrivial-auto-var-init=zero -fcf-protection=full -fstack-clash-protection -fstack-protector-strong -fstack-check -march=x86-64-v3 -msse2 -msse3 -mssse3 -msse4.1 -msse4.2 -mavx -mavx2")
    set(CMAKE_CXX_FLAGS "${CXXFLAGS} ${CMAKE_CXX_FLAGS} -pipe -ftls-model=initial-exec -D_FORTIFY_SOURCE=3 -D_GLIBCXX_ASSERTIONS -ftrivial-auto-var-init=zero -fcf-protection=full -fstack-clash-protection -fstack-protector-strong -fstack-check -march=x86-64-v3 -msse2 -msse3 -mssse3 -msse4.1 -msse4.2 -mavx -mavx2")
    add_link_options(-pipe -ftls-model=initial-exec)
    add_link_options(LINKER:-z,noexecstack,-z,relro,-z,now)
    add_link_options(-Wl,--enable-new-dtags -Wl,--demangle)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION False)
        if (ENABLE_SANITIZERS)
            message("Building a sanitizer-intrumented build")
            add_compile_definitions(SANITIZED_BUILD)
            add_compile_options(-fsanitize=address,undefined,leak)
            add_link_options(-fsanitize=address,undefined,leak --param=max-vartrack-size=0)
        endif()
    else()
        message("Enabling IPO.")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION True)
    endif()
    set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O2")
    set(CMAKE_C_FLAGS_RELEASE "-DNDEBUG -O2")
endif()

add_subdirectory(Src)
