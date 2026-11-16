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
using codegen::ops::values::member;

/*
 * ORE Studio's party_id_scheme, trimmed to what the projection reads.
 * Expected values are the oracle's: ores.codegen's context for the model.
 */
constexpr std::string_view party_id_scheme = R"j({
 "product": "ores", "schema": "public", "component": "refdata",
 "entity_singular": "party_id_scheme", "entity_plural": "party_id_schemes",
 "description": "Reference data table defining valid party identifier scheme types.\nExamples: 'LEI', 'BIC', 'MIC', 'DUNS'.\n\nParty ID schemes are managed by the system tenant. The optional\ncoding_scheme_code field cross-references the DQ coding scheme table.",
 "coding_scheme": "none", "has_tenant_id": true, "image_id": false,
 "primary_key": {"column": "code", "columns": [{"column": "code",
   "cpp_type": "std::string", "type": "text"}],
   "cpp_type": "std::string", "type": "text"},
 "natural_keys": [{"column": "name", "cpp_type": "std::string",
   "type": "text"}],
 "columns": [
  {"cpp_type": "std::string", "name": "description", "nullable": false,
   "type": "text"},
  {"cpp_type": "std::string", "name": "coding_scheme_code", "nullable": true,
   "type": "text"},
  {"cpp_type": "int", "default": 0, "name": "display_order",
   "nullable": false, "type": "integer"},
  {"cpp_type": "std::optional<int>", "name": "max_cardinality",
   "nullable": true, "type": "integer"}],
 "validation_fn": {"order_by": "display_order", "tenant_scope": "system"},
 "insert_trigger": {"validations": [
  {"column": "change_reason_code", "nullable": false,
   "validation_function": "ores_dq_validate_change_reason_fn"},
  {"column": "coding_scheme_code", "nullable": true,
   "validation_function": "ores_refdata_validate_party_id_scheme_coding_scheme_fn"}]},
 "check_constraints": [{"expression": "\"max_cardinality\" is null or \"max_cardinality\" > 0"}]
})j";

}

TEST_CASE("the SQL table flags reach the entity's shared lists and flags",
    "[codegen][entity][sql_table]") {
    codegen::tests::entity_fixture f(party_id_scheme);
    codegen::ops::derive_sql_table_flags(f.b, f.entity);

    CHECK(f.json("validation_fn") == R"j({
  "order_by": "display_order",
  "scope_both": false,
  "scope_system": true,
  "scope_tenant": false,
  "tenant_scope": "system"
})j");
    CHECK(f.json("check_constraints") == R"j([
  {
    "expression": "\"max_cardinality\" is null or \"max_cardinality\" > 0",
    "last": true
  }
])j");
    const auto validations = member(f.tables,
        member(f.tables, f.entity, "insert_trigger"), "validations");
    const auto items = codegen::ops::values::items(f.tables, validations);
    CHECK(f.json(items.front(), "last") == "<absent>");
    CHECK(f.json(items.back(), "last") == "true");
    CHECK(f.json("has_coding_scheme") == "false");
    CHECK(f.json("has_nullable_coding_scheme") == "false");
    CHECK(f.json("has_any_coding_scheme") == "false");
    CHECK(f.json("has_image_id") == "false");
}

TEST_CASE("a nullable coding scheme sets the nullable and any flags",
    "[codegen][entity][sql_table]") {
    codegen::tests::entity_fixture f(R"j({"coding_scheme": "nullable"})j");
    codegen::ops::derive_sql_table_flags(f.b, f.entity);
    CHECK(f.json("has_coding_scheme") == "false");
    CHECK(f.json("has_nullable_coding_scheme") == "true");
    CHECK(f.json("has_any_coding_scheme") == "true");
}

/*
 * The table shares the primary key's columns list with the entity, so a
 * flag a later step sets on a key column shows in the table, while one set
 * on the key itself does not.
 */
TEST_CASE("the SQL table projects the entity as the oracle's table",
    "[codegen][entity][sql_table]") {
    codegen::tests::entity_fixture f(party_id_scheme);
    codegen::ops::derive_sql_table_flags(f.b, f.entity);
    const auto pk = member(f.tables, f.entity, "primary_key");
    f.b.set(pk, "group_prefix", "");
    f.b.set(codegen::ops::values::items(f.tables, pk, "columns").front(),
        "group_prefix", "");

    const auto context = f.b.detached_object();
    codegen::ops::project_sql_table(f.b, context, f.loaded, f.entity);
    CHECK(f.json(context, "table") == R"j({
  "check_constraints": [
    {
      "expression": "\"max_cardinality\" is null or \"max_cardinality\" > 0",
      "last": true
    }
  ],
  "coding_scheme": "none",
  "columns": [
    {
      "name": "name",
      "type": "text"
    },
    {
      "name": "description",
      "nullable": false,
      "type": "text"
    },
    {
      "name": "coding_scheme_code",
      "nullable": true,
      "type": "text"
    },
    {
      "default": "0",
      "name": "display_order",
      "nullable": false,
      "type": "integer"
    },
    {
      "last": true,
      "name": "max_cardinality",
      "nullable": true,
      "type": "integer"
    }
  ],
  "component": "refdata",
  "description": "Reference data table defining valid party identifier scheme types.\nExamples: 'LEI', 'BIC', 'MIC', 'DUNS'.\n\nParty ID schemes are managed by the system tenant. The optional\ncoding_scheme_code field cross-references the DQ coding scheme table.",
  "description_comment": "-- Reference data table defining valid party identifier scheme types.\n-- Examples: 'LEI', 'BIC', 'MIC', 'DUNS'.\n--\n-- Party ID schemes are managed by the system tenant. The optional\n-- coding_scheme_code field cross-references the DQ coding scheme table.",
  "entity_plural": "party_id_schemes",
  "entity_singular": "party_id_scheme",
  "has_any_coding_scheme": false,
  "has_check_constraints": true,
  "has_coding_scheme": false,
  "has_image_id": false,
  "has_nullable_coding_scheme": false,
  "has_tenant_id": true,
  "image_id": false,
  "indexes": [],
  "insert_trigger": {
    "validations": [
      {
        "column": "change_reason_code",
        "nullable": false,
        "validation_function": "ores_dq_validate_change_reason_fn"
      },
      {
        "column": "coding_scheme_code",
        "last": true,
        "nullable": true,
        "validation_function": "ores_refdata_validate_party_id_scheme_coding_scheme_fn"
      }
    ]
  },
  "primary_key": {
    "column": "code",
    "columns": [
      {
        "column": "code",
        "cpp_type": "std::string",
        "group_prefix": "",
        "type": "text"
      }
    ],
    "cpp_type": "std::string",
    "is_text": true,
    "type": "text"
  },
  "product": "ores",
  "schema": "public",
  "sql_check_constraints": "    check (\"max_cardinality\" is null or \"max_cardinality\" > 0)",
  "validation_fn": {
    "order_by": "display_order",
    "scope_both": false,
    "scope_system": true,
    "scope_tenant": false,
    "tenant_scope": "system"
  }
})j");
}
