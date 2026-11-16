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

/*
 * ores.refdata.business_unit as the oracle shows it, trimmed: a legacy
 * finder by party and name, a declared key that is not the storage key,
 * and a party natural key that scopes it.
 */
constexpr auto business_unit = R"j({
    "presentation": {"key_field": "unit_code"},
    "service_find_by_uuid": true,
    "service_find_by_code": {"column": "unit_name", "columns": ["unit_name"],
        "parent_column": "party_id"},
    "primary_key": {"columns": [{"column": "id", "is_uuid": true,
        "cpp_type": "boost::uuids::uuid"}]},
    "natural_keys": [{"column": "party_id", "cpp_type": "boost::uuids::uuid"}],
    "columns": [{"name": "unit_code", "cpp_type": "std::string"},
        {"name": "unit_name", "cpp_type": "std::string"}]})j";

}

TEST_CASE("a legacy finder and the declared key each get a finder",
    "[codegen][entity][finders]") {
    codegen::tests::entity_fixture f(business_unit);
    codegen::ops::derive_key_finders(f.b, f.entity);
    codegen::ops::derive_declared_key(f.b, f.entity);
    const auto finders = items(f.tables, member(f.tables, f.entity,
        "key_finders"));
    REQUIRE(finders.size() == 2);
    CHECK(f.json(finders[0], "suffix") == R"("code")");
    CHECK(f.json(finders[0], "columns") == "<absent>");
    CHECK(f.json(finders[0], "params") ==
        R"("const std::string& party_id, const std::string& unit_name")");
    CHECK(f.json(finders[0], "where_and") ==
        R"("\"party_id\"_c == party_id && \"unit_name\"_c == unit_name")");
    CHECK(f.json(finders[0], "log_fields") ==
        R"("party_id << \"/\" << unit_name")");
    CHECK(f.json(finders[0], "key_label") == R"("party_id and unit_name")");
    CHECK(f.json(finders[0], "key_noun") == R"("pair")");
    CHECK(f.json(finders[0], "service_params") ==
        R"("const boost::uuids::uuid& party_id, const std::string& unit_name")");
    CHECK(f.json(finders[0], "service_call_args") ==
        R"("boost::uuids::to_string(party_id), unit_name")");
    CHECK(f.json(finders[0], "is_declared_key_finder") == "false");
    CHECK(f.json(finders[1], "column") == R"("unit_code")");
    CHECK(f.json(finders[1], "suffix") == R"("unit_code")");
    CHECK(f.json(finders[1], "is_declared_key_finder") == "true");

    const auto legacy = member(f.tables, f.entity, "service_find_by_code");
    CHECK(f.json(legacy, "key_label") == R"("party_id and unit_name")");
    const auto resolvers = items(f.tables, member(f.tables, f.entity,
        "key_resolvers"));
    REQUIRE(resolvers.size() == 1);
    CHECK(f.json(resolvers[0], "suffix") == R"("unit_code")");

    CHECK(f.json("declared_key") == R"("unit_code")");
    CHECK(f.json("key_is_primary") == "false");
    CHECK(f.json("history_by_row_id") == "true");
    CHECK(f.json("declared_key_text") == R"("key.unit_code")");
    CHECK(f.json("declared_row_text") == R"("row.unit_code")");
    CHECK(f.json("declared_key_has_batch_read") == "true");
    CHECK(f.json("declared_key_is_uuid") == "false");
}

TEST_CASE("a model stating its own finder keeps it, and its batch form",
    "[codegen][entity][finders]") {
    codegen::tests::entity_fixture f(R"j({"key_field": "username",
        "implementations": {"A": [
            "    std::vector<domain::account> read_latest_by_username(const context& ctx, const std::string& username);",
            "    std::vector<domain::account> read_latest_by_username(\n        const context & ctx,  const std::vector< std::string > & names);"]},
        "primary_key": {"columns": [{"column": "id"}]},
        "columns": [{"name": "username"}]})j");
    codegen::ops::derive_key_finders(f.b, f.entity);
    codegen::ops::derive_declared_key(f.b, f.entity);
    CHECK(f.json("key_finders") == "[]");
    CHECK(f.json("declared_key_has_batch_read") == "true");
}

TEST_CASE("a declared key converts to text by its column's type",
    "[codegen][entity][finders]") {
    codegen::tests::entity_fixture f(R"j({"key_field": "number",
        "primary_key": {"columns": [{"column": "id"}]},
        "columns": [{"name": "number", "cpp_type": "std::uint32_t",
            "group_prefix": "identity."}]})j");
    codegen::ops::derive_key_finders(f.b, f.entity);
    codegen::ops::derive_declared_key(f.b, f.entity);
    CHECK(f.json("declared_key_text") == R"x("std::to_string(key.number)")x");
    CHECK(f.json("declared_row_text") ==
        R"x("std::to_string(row.identity.number)")x");
    CHECK(f.json("declared_key_group_prefix") == R"("identity.")");
    CHECK(f.json("declared_key_has_batch_read") == "true");
    CHECK(f.json("history_by_row_id") == "false");
}

TEST_CASE("an entity keyed by its declared key has no finder",
    "[codegen][entity][finders]") {
    codegen::tests::entity_fixture f(R"j({"key_field": "code",
        "primary_key": {"columns": [{"column": "code"}]}})j");
    codegen::ops::derive_key_finders(f.b, f.entity);
    codegen::ops::derive_declared_key(f.b, f.entity);
    CHECK(f.json("key_finders") == "[]");
    CHECK(f.json("key_resolvers") == "[]");
    CHECK(f.json("key_is_primary") == "true");
    CHECK(f.json("declared_key_has_batch_read") == "false");
}
