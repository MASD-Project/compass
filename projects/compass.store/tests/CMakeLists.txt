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
set(name "compass.store")
set(lib_target_name ${name}.lib)
set(tests_target_name ${name}.tests)

add_executable(${tests_target_name}
    io_collection_tests.cpp
    io_git_objects_tests.cpp
    io_id_resolver_tests.cpp
    io_location_tests.cpp
    io_object_file_tests.cpp
    io_pack_store_tests.cpp
    io_worktree_index_tests.cpp
    ops_blob_hash_tests.cpp
    ops_git_index_tests.cpp
    ops_git_object_tests.cpp
    ops_git_settings_tests.cpp
    ops_pack_tests.cpp)

# These tests fork, which Windows cannot.
if(NOT WIN32)
    target_sources(${tests_target_name} PRIVATE
        io_pack_store_process_tests.cpp)
endif()

target_link_libraries(${tests_target_name}
    PRIVATE
        ${lib_target_name}
        compass.testing.lib
        Catch2::Catch2WithMain)

add_test(NAME ${tests_target_name}
    COMMAND ${tests_target_name} ${COMPASS_TEST_FILTER}
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
