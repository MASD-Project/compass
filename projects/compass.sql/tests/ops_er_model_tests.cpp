/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Marco Craveiro <marco.craveiro@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#include "compass.sql/ops/er_model.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace compass::sql;

}

TEST_CASE("a foreign key a script states belongs to the one table that "
    "script defines first with the column", "[ops][er_model]") {
    const std::array<ops::er_script, 3> scripts{{
        {"create/a_create.sql",
         "create table if not exists ores_parents_tbl (\n"
         "    id uuid not null\n"
         ");\n"
         "create table if not exists ores_owners_tbl (\n"
         "    parent_id uuid not null\n"
         ");\n"},
        {"create/b_create.sql",
         "create table if not exists ores_owners_tbl (\n"
         "    parent_id uuid not null\n"
         ");\n"
         "create table if not exists ores_children_tbl (\n"
         "    parent_id uuid not null\n"
         ");\n"
         "-- Validate parent_id (soft FK to ores_parents_tbl)\n"},
        {"create/c_create.sql",
         "create table if not exists ores_others_tbl (\n"
         "    parent_id uuid not null\n"
         ");\n"
         "create table if not exists ores_more_tbl (\n"
         "    parent_id uuid not null\n"
         ");\n"
         "-- Validate parent_id (soft FK to ores_parents_tbl)\n"}}};
    const auto m = ops::er_model_of(scripts, {});
    REQUIRE(m.relationships.size() == 1);
    CHECK(m.relationships[0].from_table == "ores_parents_tbl");
    CHECK(m.relationships[0].to_table == "ores_children_tbl");
    REQUIRE(m.tables.size() == 5);
    CHECK(m.tables[1].name == "ores_owners_tbl");
    CHECK(m.tables[1].path == "create/b_create.sql");
}
