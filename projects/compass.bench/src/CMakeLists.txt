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
set(name "compass.bench")
set(lib_target_name ${name}.lib)
set(exe_target_name ${name}.exe)

add_library(${lib_target_name}
    io/compile_commands.cpp
    io/report.cpp
    ops/include_graph.cpp
    ops/include_resolve.cpp
    ops/include_scan.cpp
    ops/listing.cpp
    ops/partition.cpp
    ops/read_stage.cpp
    ops/search_paths.cpp)

set_target_properties(${lib_target_name} PROPERTIES OUTPUT_NAME ${name})

target_include_directories(${lib_target_name} PUBLIC
    ${CMAKE_SOURCE_DIR}/projects/compass.bench/include)

target_link_libraries(${lib_target_name}
    PUBLIC
        compass.platform.lib
        Threads::Threads)

add_executable(${exe_target_name}
    main.cpp)

set_target_properties(${exe_target_name} PROPERTIES OUTPUT_NAME compass-bench)

target_link_libraries(${exe_target_name}
    PRIVATE
        ${lib_target_name})
