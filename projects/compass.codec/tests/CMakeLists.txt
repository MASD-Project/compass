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
set(name "compass.codec")
set(lib_target_name ${name}.lib)
set(tests_target_name ${name}.tests)

add_executable(${tests_target_name}
    codecs_document_index_tests.cpp
    codecs_required_sections_tests.cpp
    codecs_tangle_tests.cpp
    io_dependency_file_tests.cpp
    io_format_cache_tests.cpp
    io_output_sink_tests.cpp
    io_unmarked_file_tests.cpp
    ops_action_cache_tests.cpp
    ops_dependencies_tests.cpp
    ops_exceptions_tests.cpp
    ops_file_findings_tests.cpp
    ops_file_inputs_tests.cpp
    ops_format_cache_tests.cpp
    ops_outputs_tests.cpp
    ops_registry_tests.cpp
    ops_rule_table_tests.cpp
    ops_run_tests.cpp
    ops_scan_tests.cpp
    ops_staged_tests.cpp
    ops_unified_diff_tests.cpp
    ops_unit_index_tests.cpp)

# The parser and the registry builder are needed to make the corpus a test
# runs over; the library itself does not depend on compass.ingest.
target_link_libraries(${tests_target_name}
    PRIVATE
        ${lib_target_name}
        compass.ingest.lib
        Catch2::Catch2WithMain)

add_test(NAME ${tests_target_name}
    COMMAND ${tests_target_name}
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
