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
set(tests_target_name ${name}.tests)

add_executable(${tests_target_name}
    codecs_backlog_indexes_tests.cpp
    codecs_component_docs_tests.cpp
    codecs_heading_numbers_tests.cpp
    codecs_lint_graph_tests.cpp
    codecs_lint_tests.cpp
    codecs_org_links_tests.cpp
    codecs_physical_inventory_tests.cpp
    codecs_recipe_inventories_tests.cpp
    codecs_recipe_libraries_tests.cpp
    codecs_refusals_tests.cpp
    codecs_skill_levels_tests.cpp
    codecs_skill_tables_tests.cpp
    codecs_trade_type_coverage_tests.cpp
    codecs_tree_gates_tests.cpp)

target_link_libraries(${tests_target_name}
    PRIVATE
        ${lib_target_name}
        compass.ingest.lib
        compass.store.lib
        Catch2::Catch2WithMain)

compass_add_catch2_tests(${name} ${tests_target_name})
