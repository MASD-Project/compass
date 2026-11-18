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
set(name "compass.platform")
set(lib_target_name ${name}.lib)
set(tests_target_name ${name}.tests)

add_executable(${tests_target_name}
    filesystem_file_status_tests.cpp
    filesystem_mapped_file_tests.cpp
    filesystem_new_file_tests.cpp
    process_environment_tests.cpp
    process_executable_tests.cpp
    process_liveness_tests.cpp
    process_run_program_tests.cpp
    text_decoding_tests.cpp
    text_regex_tests.cpp
    text_xml_tests.cpp)

target_link_libraries(${tests_target_name}
    PRIVATE
        ${lib_target_name}
        compass.testing.lib
        Catch2::Catch2WithMain)

compass_add_catch2_tests(${name} ${tests_target_name})
