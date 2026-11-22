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
set(name "compass.docs")
set(lib_target_name ${name}.lib)

add_library(${lib_target_name}
    codecs/backlog_indexes.cpp
    codecs/catalogue.cpp
    codecs/component_docs.cpp
    codecs/heading_numbers.cpp
    codecs/knowledge_titles.cpp
    codecs/lint.cpp
    codecs/org_links.cpp
    codecs/physical_inventory.cpp
    codecs/prototypes.cpp
    codecs/recipe_inventories.cpp
    codecs/recipe_libraries.cpp
    codecs/rule_identifiers.cpp
    codecs/skill_levels.cpp
    codecs/skill_tables.cpp
    codecs/trade_type_coverage.cpp
    codecs/tree_gates.cpp
    ops/link_graph.cpp
    ops/org_text.cpp
    ops/regions.cpp
    ops/skills.cpp)

set_target_properties(${lib_target_name} PROPERTIES OUTPUT_NAME ${name})

target_include_directories(${lib_target_name} PUBLIC
    ${CMAKE_SOURCE_DIR}/projects/compass.docs/include)

target_link_libraries(${lib_target_name}
    PUBLIC
        compass.cpp.lib
        compass.codec.lib
        compass.platform.lib)

compass_code_hash(${lib_target_name} codecs/code_hash.cpp
    compass::docs::codecs compass.docs/codecs/catalogue.hpp)
