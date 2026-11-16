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

add_library(${lib_target_name}
    ops/compile.cpp
    ops/context.cpp
    ops/entity_codecs.cpp
    ops/entity_columns.cpp
    ops/entity_context.cpp
    ops/entity_economics.cpp
    ops/entity_finders.cpp
    ops/entity_gates.cpp
    ops/entity_groups.cpp
    ops/entity_keys.cpp
    ops/entity_markers.cpp
    ops/entity_messages.cpp
    ops/entity_names.cpp
    ops/entity_operations.cpp
    ops/entity_order.cpp
    ops/entity_presentation.cpp
    ops/entity_primary_key.cpp
    ops/entity_registry.cpp
    ops/entity_repository.cpp
    ops/entity_rules.cpp
    ops/entity_seeds.cpp
    ops/entity_service.cpp
    ops/entity_sql_names.cpp
    ops/entity_sql_table.cpp
    ops/entity_storage.cpp
    ops/entity_supply.cpp
    ops/entity_surfaces.cpp
    ops/entity_typescript.cpp
    ops/entity_wire.cpp
    ops/manifest.cpp
    ops/output.cpp
    ops/physical_space.cpp
    ops/render.cpp)

set_target_properties(${lib_target_name} PROPERTIES OUTPUT_NAME ${name})

target_include_directories(${lib_target_name} PUBLIC
    ${CMAKE_SOURCE_DIR}/projects/compass.codegen/include)

target_link_libraries(${lib_target_name}
    PUBLIC
        compass.model.lib)
