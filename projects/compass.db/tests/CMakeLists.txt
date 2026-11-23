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
set(name "compass.db")
set(lib_target_name ${name}.lib)
set(tests_target_name ${name}.tests)

add_executable(${tests_target_name}
    io_corpus_tests.cpp
    io_schema_tests.cpp
    ops_campaign_page_tests.cpp
    ops_copy_text_tests.cpp
    ops_extract_tests.cpp
    ops_migrations_tests.cpp
    query_campaign_tests.cpp
    query_search_tests.cpp
    query_testing_tests.cpp)

target_link_libraries(${tests_target_name}
    PRIVATE
        ${lib_target_name}
        compass.html.lib
        compass.ingest.lib
        compass.testing.lib
        Catch2::Catch2WithMain)

# The catalog test reads the generated schema from the checkout, as no
# program runs it against a database.
target_compile_definitions(${tests_target_name}
    PRIVATE
        COMPASS_DB_SCHEMA_SQL="${CMAKE_SOURCE_DIR}/projects/compass.db/sql/schema.sql")

compass_add_catch2_tests(${name} ${tests_target_name})
