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
set(tests_target_name ${name}.tests)

add_executable(${tests_target_name}
    codecs_bff_list_requests_tests.cpp
    codecs_core_link_edges_tests.cpp
    codecs_exported_types_tests.cpp
    codecs_forward_declarable_includes_tests.cpp
    codecs_forbid_tests.cpp
    ops_format_check_tests.cpp
    codecs_registrars_tests.cpp
    codecs_route_auth_tests.cpp
    codecs_shell_tests.cpp
    codecs_stream_coverage_tests.cpp
    codecs_test_case_reachability_tests.cpp
    codecs_translation_unit_budget_tests.cpp
    codecs_unique_type_names_tests.cpp
    codecs_unity_collisions_tests.cpp
    codecs_xml_family_census_tests.cpp
    ops_lexer_tests.cpp
    ops_paths_tests.cpp
    ops_type_widths_tests.cpp)

target_link_libraries(${tests_target_name}
    PRIVATE
        ${lib_target_name}
        compass.ingest.lib
        compass.store.lib
        compass.testing.lib
        Catch2::Catch2WithMain)

compass_add_catch2_tests(${name} ${tests_target_name})
