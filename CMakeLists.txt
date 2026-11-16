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

find_package(Catch2 3 CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(unofficial-tree-sitter CONFIG REQUIRED)
find_package(unofficial-tree-sitter-cpp CONFIG REQUIRED)
find_package(unofficial-tree-sitter-sql CONFIG REQUIRED)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/projects)
