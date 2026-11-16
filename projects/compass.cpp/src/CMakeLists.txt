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
set(name "compass.cpp")
set(lib_target_name ${name}.lib)

add_library(${lib_target_name}
    codecs/catalogue.cpp
    codecs/enum_attributes.cpp
    codecs/exported_types.cpp
    codecs/forward_declarable_includes.cpp
    codecs/portable_stop.cpp
    codecs/registrars.cpp
    codecs/route_auth.cpp
    codecs/shell.cpp
    codecs/test_case_reachability.cpp
    codecs/unique_type_names.cpp
    codecs/unity_collisions.cpp
    ops/enum_attributes.cpp
    ops/exported_types.cpp
    ops/paths.cpp
    ops/report.cpp
    ops/test_case_reachability.cpp
    ops/text.cpp)

set_target_properties(${lib_target_name} PROPERTIES OUTPUT_NAME ${name})

target_include_directories(${lib_target_name} PUBLIC
    ${CMAKE_SOURCE_DIR}/projects/compass.cpp/include)

target_link_libraries(${lib_target_name}
    PUBLIC
        compass.codec.lib
        compass.platform.lib)
