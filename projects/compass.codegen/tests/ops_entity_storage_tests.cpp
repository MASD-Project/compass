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

void derive(codegen::tests::entity_fixture& f) {
    codegen::ops::derive_party_scope(f.b, f.entity);
    codegen::ops::derive_storage_shape(f.b, f.entity);
}

}

/*
 * ORE Studio's grid_sample, a current-state hypertable; expected values
 * are the oracle's.
 */
TEST_CASE("a current-state hypertable keeps tenant_id out of its key",
    "[codegen][entity][storage]") {
    codegen::tests::entity_fixture f(R"j({
     "entity_plural": "grid_samples", "has_tenant_id": true,
     "columns": [{"name": "party_id",
       "cpp_type": "std::optional<boost::uuids::uuid>"}],
     "sql": {"current_state": true, "hypertable": true,
       "newest_by": "sampled_at", "no_audit_columns": true}})j");
    derive(f);
    CHECK(f.json("index_name_prefix") == R"("grid_samples")");
    CHECK(f.json("has_party_column") == "true");
    CHECK(f.json("party_column_optional") == "true");
    CHECK(f.json("party_scoped_writes") == "true");
    CHECK(f.json("current_state") == "true");
    CHECK(f.json("has_newest_read") == "true");
    CHECK(f.json("has_tenant_in_pk") == "false");
    CHECK(f.json("validates_tenant_on_insert") == "true");
    CHECK(f.json("fixed_columns") == "[]");
    const auto sql = codegen::ops::values::member(f.tables, f.entity, "sql");
    CHECK(f.json(sql, "bitemporal_soft_update") == "false");
    CHECK(f.json(sql, "rls_hot_path") == "false");
}

/*
 * ORE Studio's book: a fixed column, and a party taken from a parent that
 * is the party, so the test's session is not party-scoped.
 */
TEST_CASE("a temporal entity keys on its tenant and lists fixed columns",
    "[codegen][entity][storage]") {
    codegen::tests::entity_fixture f(R"j({
     "entity_plural": "books", "has_tenant_id": true,
     "columns": [{"name": "party_id", "cpp_type": "boost::uuids::uuid"},
                 {"name": "sandbox_id", "fixed": true}],
     "foreign_keys": [{"column": "party_id", "parent_is_party": true}],
     "sql": {"system_scope": false, "nullable_tenant_id": false,
       "rls_policy_style": "hot_path"}})j");
    derive(f);
    CHECK(f.json("party_column_optional") == "false");
    CHECK(f.json("party_scoped_writes") == "false");
    CHECK(f.json("has_as_of_lookup") == "true");
    CHECK(f.json("has_tenant_in_pk") == "true");
    CHECK(f.json("immutable_tenant_key") == "false");
    CHECK(f.json("fixed_columns") == R"j([
  {
    "column": "sandbox_id"
  }
])j");
    const auto sql = codegen::ops::values::member(f.tables, f.entity, "sql");
    CHECK(f.json(sql, "rls_hot_path") == "true");
}

TEST_CASE("an as_of lookup the model turns off stays off",
    "[codegen][entity][storage]") {
    codegen::tests::entity_fixture f(R"j({
     "has_as_of_lookup": "No", "sql": {"index_prefix": "bk"}})j");
    derive(f);
    CHECK(f.json("has_as_of_lookup") == "false");
    CHECK(f.json("index_name_prefix") == R"("bk")");
}
