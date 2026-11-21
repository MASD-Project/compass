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
set(name "compass.codegen")
set(lib_target_name ${name}.lib)
set(tests_target_name ${name}.tests)

add_executable(${tests_target_name}
    codecs_codegen_tests.cpp
    codecs_component_scaffold_tests.cpp
    ops_facts_tests.cpp
    codecs_component_files_tests.cpp
    codecs_pending_models_tests.cpp
    ops_compile_tests.cpp
    ops_context_tests.cpp
    ops_entity_codecs_tests.cpp
    ops_entity_columns_tests.cpp
    ops_entity_economics_tests.cpp
    ops_entity_finders_tests.cpp
    ops_entity_gates_tests.cpp
    ops_entity_groups_tests.cpp
    ops_entity_keys_tests.cpp
    ops_entity_markers_tests.cpp
    ops_entity_messages_tests.cpp
    ops_entity_names_tests.cpp
    ops_entity_operations_tests.cpp
    ops_entity_order_tests.cpp
    ops_entity_presentation_tests.cpp
    ops_entity_primary_key_tests.cpp
    ops_entity_repository_tests.cpp
    ops_entity_rules_tests.cpp
    ops_entity_seeds_tests.cpp
    ops_entity_service_tests.cpp
    ops_entity_sql_names_tests.cpp
    ops_entity_sql_table_tests.cpp
    ops_entity_storage_tests.cpp
    ops_entity_supply_tests.cpp
    ops_entity_surfaces_tests.cpp
    ops_entity_typescript_tests.cpp
    ops_model_contexts_tests.cpp
    ops_output_tests.cpp
    ops_physical_space_tests.cpp
    ops_render_tests.cpp
    ops_spec_tests.cpp
    ops_template_context_tests.cpp)

target_compile_definitions(${tests_target_name} PRIVATE
    COMPASS_CODEGEN_SPEC_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/mustache_spec"
    COMPASS_CODEGEN_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

target_link_libraries(${tests_target_name}
    PRIVATE
        ${lib_target_name}
        compass.cpp.lib
        compass.ingest.lib
        compass.testing.lib
        Catch2::Catch2WithMain)

compass_add_catch2_tests(${name} ${tests_target_name})
