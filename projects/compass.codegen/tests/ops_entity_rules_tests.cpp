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
#include "compass.codegen/ops/entity_steps.hpp"

#include <catch2/catch_test_macros.hpp>
#include "entity_fixture.hpp"

namespace {

using namespace compass;
using codegen::ops::values::items;
using codegen::ops::values::member;

}

/*
 * ORE Studio's grid_sample, a current-state hypertable; expected values
 * are the oracle's.
 */
TEST_CASE("a current-state table has no validity window",
    "[codegen][entity][rules]") {
    codegen::tests::entity_fixture f(R"j({
     "current_state": true, "has_as_of_lookup": true,
     "generate_touch_function": true,
     "indexes": [{"columns": "tenant_id, sampled_at desc",
       "current_only": true}],
     "foreign_keys": [{"column": "a", "list_by_as_of": true},
                      {"column": "b"}],
     "sql": {"hypertable": true, "no_audit_columns": true,
       "has_list_by_as_of": true, "bitemporal_soft_update": true}})j");
    codegen::ops::derive_table_rules(f.b, f.entity);
    CHECK(f.json("has_gist_exclusion") == "false");
    CHECK(f.json("has_audit_columns") == "false");
    CHECK(f.json("change_reason_code_declared") == "false");
    CHECK(f.json("has_as_of_lookup") == "false");
    CHECK(f.json("generate_touch_function") == "false");
    CHECK(f.json("temporal_filter") == R"("")");
    CHECK(f.json("list_temporal_filter") == R"("")");
    CHECK(f.json("version_filter") == R"("")");
    CHECK(f.json(items(f.tables, f.entity, "indexes")[0], "current_only") ==
        "false");
    const auto fks = items(f.tables, f.entity, "foreign_keys");
    CHECK(f.json(fks[0], "list_by_as_of") == "<absent>");
    CHECK(f.json(fks[0], "last") == "<absent>");
    CHECK(f.json(fks[1], "last") == "true");
    const auto sql = member(f.tables, f.entity, "sql");
    CHECK(f.json(sql, "has_list_by_as_of") == "<absent>");
    CHECK(f.json(sql, "bitemporal_soft_update") == "false");
}

/*
 * ORE Studio's book: a temporal table with an as_of read and a validated
 * column whose nullability comes from its declaration.
 */
TEST_CASE("a temporal table filters its open row and versions",
    "[codegen][entity][rules]") {
    codegen::tests::entity_fixture f(R"j({
     "current_state": false, "has_as_of_lookup": true,
     "columns": [{"name": "functional_currency"},
                 {"name": "parent_id", "nullable": true}],
     "insert_trigger": {"validations": [
       {"column": "functional_currency"}, {"column": "parent_id"},
       {"column": "change_reason_code"}]},
     "sql": {"text_code_validations": [{"column": "x"}, {"column": "y"}]}})j");
    codegen::ops::derive_table_rules(f.b, f.entity);
    CHECK(f.json("has_gist_exclusion") == "true");
    CHECK(f.json("has_audit_columns") == "true");
    CHECK(f.json("change_reason_code_declared") == "true");
    CHECK(f.json("temporal_filter") == R"(" && \"valid_to\"_c == max")");
    CHECK(f.json("list_temporal_filter") == R"("")");
    CHECK(f.json("version_filter") == R"(" && \"version\"_c == expected")");
    const auto v = items(f.tables,
        member(f.tables, f.entity, "insert_trigger"), "validations");
    CHECK(f.json(v[0], "nullable") == "false");
    CHECK(f.json(v[1], "nullable") == "true");
    CHECK(f.json(v[2], "nullable") == "false");
    const auto codes = items(f.tables, member(f.tables, f.entity, "sql"),
        "text_code_validations");
    CHECK(f.json(codes[1], "last") == "true");
}
