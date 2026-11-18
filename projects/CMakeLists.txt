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
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(flags "")
    if(WITH_PROFILING)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            set(flags "${flags} -fprofile-abs-path")
        endif()
        set(flags "${flags} -fprofile-arcs")
        set(flags "${flags} -ftest-coverage")
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flags}")
    message(STATUS "C++ flags: ${CMAKE_CXX_FLAGS}")
endif()

add_compile_options(
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wall>
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wextra>
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Werror>)

# Components are added by name, in dependency order. compass.legacy and
# modeling are not C++ components and are never added here.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.platform)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.store)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.org)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.ingest)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.model)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.codegen)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.codec)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.cpp)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.sql)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.crosscheck)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.docs)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.cli)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.bench)
