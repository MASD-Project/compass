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

project(compass VERSION 0.0.1 LANGUAGES CXX
    DESCRIPTION "Data-oriented codegen, gates and analysis for MASD.")

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

find_package(Catch2 3 CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(expat CONFIG REQUIRED)
find_package(pcre2 CONFIG REQUIRED COMPONENTS 8BIT)
find_package(ZLIB REQUIRED)
find_package(unofficial-tree-sitter CONFIG REQUIRED)
find_package(unofficial-tree-sitter-cpp CONFIG REQUIRED)
find_package(unofficial-tree-sitter-sql CONFIG REQUIRED)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/code_hash.cmake)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/projects)
