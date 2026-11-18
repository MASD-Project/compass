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

add_library(${lib_target_name}
    codecs/catalogue.cpp
    codecs/convention_kind_terms.cpp
    codecs/conventions_coverage.cpp
    codecs/conventions_dataset.cpp
    codecs/curve_column_guards.cpp
    codecs/curve_sections_wiring.cpp
    codecs/entity_binary_types.cpp
    codecs/entity_writes.cpp
    codecs/er_diagram.cpp
    codecs/generated_column_refs.cpp
    codecs/marketdata_identity_columns.cpp
    codecs/model_rules.cpp
    codecs/open_reads.cpp
    codecs/ore_output_catalogue.cpp
    codecs/ore_renderings_tables.cpp
    codecs/protocol_twins.cpp
    codecs/registry_gates.cpp
    codecs/route_inventory.cpp
    codecs/script_library_seed.cpp
    codecs/subject_conformance.cpp
    codecs/template_sections.cpp
    codecs/value_sets.cpp
    ops/csv.cpp
    ops/curve_wiring.cpp
    ops/json_value.cpp
    ops/md5.cpp
    ops/ore_schema.cpp
    ops/projection_columns.cpp
    ops/python_numbers.cpp
    ops/python_literals.cpp
    ops/subjects.cpp
    ops/value_sets.cpp
    ops/words.cpp)

set_target_properties(${lib_target_name} PROPERTIES OUTPUT_NAME ${name})

target_include_directories(${lib_target_name} PUBLIC
    ${CMAKE_SOURCE_DIR}/projects/compass.crosscheck/include)

target_link_libraries(${lib_target_name}
    PUBLIC
        compass.codegen.lib
        compass.sql.lib
        compass.cpp.lib
        compass.codec.lib
        compass.platform.lib)

compass_code_hash(${lib_target_name} codecs/code_hash.cpp
    compass::crosscheck::codecs compass.crosscheck/codecs/catalogue.hpp)
