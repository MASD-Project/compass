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
set(name "compass.sql")
set(lib_target_name ${name}.lib)
set(tests_target_name ${name}.tests)

add_executable(${tests_target_name}
    codecs_curve_parameter_paths_tests.cpp
    codecs_handler_permissions_tests.cpp
    codecs_populate_refs_tests.cpp
    codecs_rls_coverage_tests.cpp
    codecs_rls_widenings_tests.cpp
    codecs_sql_hygiene_tests.cpp
    codecs_sql_reachability_tests.cpp
    ops_er_model_tests.cpp
    ops_includes_tests.cpp
    ops_sql_model_tests.cpp)

target_link_libraries(${tests_target_name}
    PRIVATE
        ${lib_target_name}
        compass.store.lib
        Catch2::Catch2WithMain)

add_test(NAME ${tests_target_name}
    COMMAND ${tests_target_name} ${COMPASS_TEST_FILTER}
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
