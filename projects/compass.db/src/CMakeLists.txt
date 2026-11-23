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
set(name "compass.db")
set(lib_target_name ${name}.lib)

add_library(${lib_target_name}
    io/connection.cpp
    io/corpus.cpp
    io/document_copy.cpp
    io/heading_copy.cpp
    io/link_copy.cpp
    io/placement_copy.cpp
    io/scenario_copy.cpp
    io/schema.cpp
    io/section_copy.cpp
    io/step_copy.cpp
    ops/copy_text.cpp
    ops/extract.cpp
    ops/migrations.cpp
    ops/run_outcome.cpp
    query/campaign.cpp
    query/search.cpp
    query/testing.cpp)

set_target_properties(${lib_target_name} PROPERTIES OUTPUT_NAME ${name})

target_include_directories(${lib_target_name} PUBLIC
    ${CMAKE_SOURCE_DIR}/projects/compass.db/include)

target_link_libraries(${lib_target_name}
    PUBLIC
        compass.org.lib
        compass.store.lib
    PRIVATE
        compass.html.lib
        compass.platform.lib
        PostgreSQL::PostgreSQL)
