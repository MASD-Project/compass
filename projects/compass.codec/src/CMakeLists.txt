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
set(name "compass.codec")
set(lib_target_name ${name}.lib)

add_library(${lib_target_name}
    codecs/catalogue.cpp
    codecs/document_index.cpp
    codecs/required_sections.cpp
    codecs/tangle.cpp
    codecs/tangle_check.cpp
    io/action_cache.cpp
    io/dependency_file.cpp
    io/format_cache.cpp
    io/output_sink.cpp
    ops/action_cache.cpp
    ops/dependencies.cpp
    ops/exceptions.cpp
    ops/format_cache.cpp
    ops/outputs.cpp
    ops/registry.cpp
    ops/run.cpp
    ops/tangle.cpp
    ops/unified_diff.cpp)

set_target_properties(${lib_target_name} PROPERTIES OUTPUT_NAME ${name})

target_include_directories(${lib_target_name} PUBLIC
    ${CMAKE_SOURCE_DIR}/projects/compass.codec/include)

target_link_libraries(${lib_target_name}
    PUBLIC
        compass.org.lib
        compass.store.lib
        Threads::Threads
    PRIVATE
        compass.platform.lib)
