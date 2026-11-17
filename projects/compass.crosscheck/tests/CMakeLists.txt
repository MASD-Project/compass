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
set(name "compass.crosscheck")
set(lib_target_name ${name}.lib)
set(tests_target_name ${name}.tests)

add_executable(${tests_target_name}
    codecs_convention_kind_terms_tests.cpp
    codecs_conventions_coverage_tests.cpp
    codecs_conventions_dataset_tests.cpp
    codecs_curve_wiring_tests.cpp
    codecs_entity_writes_tests.cpp
    codecs_er_diagram_tests.cpp
    codecs_generated_reads_tests.cpp
    codecs_marketdata_identity_columns_tests.cpp
    codecs_ore_output_catalogue_tests.cpp
    codecs_model_columns_tests.cpp
    codecs_ore_renderings_tables_tests.cpp
    codecs_paste_blocks_tests.cpp
    codecs_protocol_twins_tests.cpp
    codecs_registry_gates_tests.cpp
    codecs_route_inventory_tests.cpp
    codecs_script_library_seed_tests.cpp
    codecs_subject_conformance_tests.cpp)

target_link_libraries(${tests_target_name}
    PRIVATE
        ${lib_target_name}
        compass.store.lib
        Catch2::Catch2WithMain)

add_test(NAME ${tests_target_name}
    COMMAND ${tests_target_name}
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
