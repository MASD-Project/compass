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
set(name "compass.cli")
set(lib_target_name ${name}.lib)
set(tests_target_name ${name}.tests)

add_executable(${tests_target_name}
    app_application_tests.cpp
    app_codegen_tests.cpp
    app_commit_tests.cpp
    app_exceptions_tests.cpp
    app_pipeline_tests.cpp
    app_ratchet_tests.cpp
    config_arguments_tests.cpp
    config_precedence_tests.cpp
    config_repository_file_tests.cpp
    ops_ratchet_tests.cpp)

target_link_libraries(${tests_target_name}
    PRIVATE
        ${lib_target_name}
        Catch2::Catch2WithMain)

add_test(NAME ${tests_target_name}
    COMMAND ${tests_target_name}
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

add_test(NAME ${name}.end_to_end
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/end_to_end.sh
        $<TARGET_FILE:${name}.exe>)
