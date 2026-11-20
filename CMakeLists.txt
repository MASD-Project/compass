# -*- mode: cmake; cmake-tab-width: 4; indent-tabs-mode: nil -*-
#
# Copyright (C) 2026 Marco Craveiro <marco.craveiro@gmail.com>
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 3 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
# Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

# vcpkg reads the overlays when project() runs the manifest install, so this
# must come first.
set(VCPKG_OVERLAY_PORTS "${CMAKE_CURRENT_SOURCE_DIR}/build/cmake/overlays/common")

# CMake 4.4 enforces the warnings.uninitialized preset inside project()'s own
# compiler detection and vcpkg's toolchain file, neither of which this
# repository owns, as ORE Studio found. Silence it only around project(), so our
# own CMake code still reports genuine uninitialized variables.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.4")
    cmake_diagnostic(PUSH)
    cmake_diagnostic(SET CMD_UNINITIALIZED IGNORE)
endif()

project(compass VERSION 0.0.2 LANGUAGES CXX
    DESCRIPTION "Data-oriented codegen, gates and analysis for MASD.")

if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.4")
    cmake_diagnostic(POP)
endif()

enable_testing()

set(COMPASS_PUBLISH_DIRECTORY ${CMAKE_BINARY_DIR}/publish)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${COMPASS_PUBLISH_DIRECTORY}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${COMPASS_PUBLISH_DIRECTORY}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${COMPASS_PUBLISH_DIRECTORY}/bin)

if(UNIX)
    set(CMAKE_INSTALL_PREFIX "/opt/compass/${CMAKE_PROJECT_VERSION}")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/compass/${CMAKE_PROJECT_VERSION}")
endif()

option(WITH_PROFILING "Generate profiling information for code coverage." OFF)
option(WITH_MEMCHECK "Configure the tests for a run under valgrind." OFF)
option(COMPASS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors." OFF)

#
# Compiler cache. CI always uses sccache. A local build uses the tool named by
# COMPASS_COMPILER_CACHE in .env, and no cache when .env does not name one, so
# a compiler cache installed for some other reason is never picked up.
#
set(COMPASS_COMPILER_CACHE "")
if(DEFINED ENV{CI})
    set(COMPASS_COMPILER_CACHE "sccache")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/.env")
    set_property(DIRECTORY APPEND PROPERTY
        CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/.env")
    file(STRINGS "${CMAKE_SOURCE_DIR}/.env" compass_env_lines
        REGEX "^COMPASS_COMPILER_CACHE=")
    foreach(line ${compass_env_lines})
        string(REGEX REPLACE "^COMPASS_COMPILER_CACHE=" ""
            COMPASS_COMPILER_CACHE "${line}")
    endforeach()
endif()

if(COMPASS_COMPILER_CACHE)
    find_program(CCACHE_PROGRAM "${COMPASS_COMPILER_CACHE}")
    if(CCACHE_PROGRAM)
        message(STATUS "Found ${COMPASS_COMPILER_CACHE} (${CCACHE_PROGRAM})...")
        if(WIN32)
            # A shell script is not a Win32 executable, so Windows calls the
            # tool directly and loses the cross-worktree SCCACHE_BASEDIR.
            set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE STRING "" FORCE)
        else()
            # The wrapper sets SCCACHE_BASEDIR to this worktree's root, so every
            # worktree of one commit shares a cache entry per translation unit.
            set(compiler_cache_wrapper
                "${CMAKE_SOURCE_DIR}/build/scripts/compiler_cache_wrapper.sh")
            if(NOT EXISTS "${compiler_cache_wrapper}")
                message(FATAL_ERROR
                    "compiler cache wrapper not found: ${compiler_cache_wrapper}")
            endif()
            set(CMAKE_CXX_COMPILER_LAUNCHER "${compiler_cache_wrapper}" CACHE STRING "" FORCE)
        endif()
    else()
        message(STATUS "${COMPASS_COMPILER_CACHE} NOT found.")
    endif()
else()
    message(STATUS "No compiler cache configured.")
endif()

# The package configurations vcpkg installs, and CMake's own Find modules, read
# variables before setting them in the same way, so the same silence covers
# the find_package calls.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.4")
    cmake_diagnostic(PUSH)
    cmake_diagnostic(SET CMD_UNINITIALIZED IGNORE)
endif()
find_package(Catch2 3 CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(expat CONFIG REQUIRED)
find_package(pcre2 CONFIG REQUIRED COMPONENTS 8BIT)
find_package(ZLIB REQUIRED)
find_package(unofficial-tree-sitter CONFIG REQUIRED)
find_package(unofficial-tree-sitter-cpp CONFIG REQUIRED)
find_package(unofficial-tree-sitter-sql CONFIG REQUIRED)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.4")
    cmake_diagnostic(POP)
endif()

macro(get_win_hex outvar)
    string(REGEX MATCH "^([0-9]+)\\.([0-9]+)" ${outvar} ${CMAKE_SYSTEM_VERSION})
    math(EXPR ${outvar} "(${CMAKE_MATCH_1} << 8) + ${CMAKE_MATCH_2}" OUTPUT_FORMAT HEXADECIMAL)
endmacro()

if(WIN32)
    get_win_hex(winver)
    add_compile_definitions(_WIN32_WINNT=${winver})
    # Prevent Windows.h from including the legacy WinSock.h header, which
    # conflicts with WinSock2.h pulled in by Boost.ASIO (socket_types.hpp).
    add_compile_definitions(WIN32_LEAN_AND_MEAN)
    # Keep Windows.h from defining min and max as function-like macros. A unity
    # unit that includes Windows.h makes std::numeric_limits<T>::max() expand
    # as a macro call with no arguments.
    add_compile_definitions(NOMINMAX)
    if(MSVC)
        # Allow object files with more than 65535 addressable sections.
        # Required by large translation units (e.g. registrar.cpp with many
        # template instantiations from Boost headers).
        add_compile_options(/bigobj)
    endif()
endif()

if(APPLE)
    # Apple's linker warns when a static library appears twice on one link
    # line. CMake repeats static libraries on purpose so their dependencies
    # resolve in any order. The flag is passed only when the linker accepts it,
    # because linkers before Xcode 15 do not know it.
    include(CheckLinkerFlag)
    check_linker_flag(CXX "LINKER:-no_warn_duplicate_libraries"
        compass_ld_no_warn_duplicates)
    if(compass_ld_no_warn_duplicates)
        add_link_options("LINKER:-no_warn_duplicate_libraries")
    endif()
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/code_hash.cmake)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/projects)
