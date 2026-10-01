# File Name: CMakeLists.txt
# Author: Elio Decolli (eliodecolli@gmail.com)
# Last Modified: 20/09/2026
# Purpose: Configures the skyX CMake build and dependencies.

cmake_minimum_required(VERSION 4.3)
project(skyx)

# Use CMake's optimized flags by default without overriding a selected configuration.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
option(SKYX_ENABLE_SANITIZERS "Enable address/undefined sanitizers in Debug builds" ON)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl" CACHE PATH "OpenSSL installation root")
endif()
find_package(OpenSSL REQUIRED)

add_library(skyx SHARED
        main.cpp
        net/netServer.cpp
        skyx/skyTracker.cpp
        skyx/skyPeerTable.cpp
        skyx/fs/job.cpp
        skyx/persistence/db.cpp
        include/skySerialize.hpp
        skyx/serializer/skySerialize.cpp)

set(LIBUV_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(LIBUV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LIBUV_BUILD_BENCH OFF CACHE BOOL "" FORCE)

add_subdirectory(ext/libuv/libuv EXCLUDE_FROM_ALL)

find_package(Threads REQUIRED)
add_library(sqlite3 STATIC ext/sqlite3/sqlite3.c)
set_target_properties(sqlite3 PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(sqlite3 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/ext/sqlite3")
target_link_libraries(sqlite3 PRIVATE Threads::Threads ${CMAKE_DL_LIBS})

# Let upstream detect SIMD support and honor explicit/cached BLAKE3_SIMD_TYPE settings.
add_subdirectory(ext/blake3/c EXCLUDE_FROM_ALL)
message(STATUS "skyx BLAKE3 SIMD: ${BLAKE3_SIMD_TYPE}")
if(BLAKE3_SIMD_TYPE STREQUAL "none")
    message(STATUS "BLAKE3 SIMD is disabled; use -U BLAKE3_SIMD_TYPE to rerun autodetection if this cache setting is stale.")
endif()

target_include_directories(skyx
    PUBLIC
        include
)

target_link_libraries(skyx PRIVATE
        uv_a
        blake3
        OpenSSL::SSL
        OpenSSL::Crypto
        sqlite3)

target_compile_options(skyx PRIVATE -Wall -Wextra -Wpedantic)

# Debug symbols and optimization levels come from the selected CMake configuration.
if(SKYX_ENABLE_SANITIZERS)
    target_compile_options(skyx PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
        $<$<CONFIG:Debug>:-fno-sanitize-recover=undefined>
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
        $<$<CONFIG:Debug>:-fno-optimize-sibling-calls>)
    target_link_options(skyx PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
        $<$<CONFIG:Debug>:-fno-sanitize-recover=undefined>)
endif()
