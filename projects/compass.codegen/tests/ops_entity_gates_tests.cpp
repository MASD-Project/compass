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

}

TEST_CASE("the generator facet name defaults and takes an upper form",
    "[codegen][entity][gates]") {
    codegen::tests::entity_fixture f(R"j({})j");
    codegen::ops::derive_generator_facet_names(f.b, f.entity);
    CHECK(f.json("generator_facet_name") == R"("generators")");
    CHECK(f.json("generator_facet_name_upper") == R"("GENERATORS")");

    codegen::tests::entity_fixture g(R"j({"generator_facet_name": "generator"})j");
    codegen::ops::derive_generator_facet_names(g.b, g.entity);
    CHECK(g.json("generator_facet_name_upper") == R"("GENERATOR")");
}

TEST_CASE("uuid column flags fall back to the primary key",
    "[codegen][entity][gates]") {
    codegen::tests::entity_fixture f(R"j({
        "columns": [{"name": "code", "render_is_optional_uuid": true}],
        "primary_key": {"is_uuid": false}})j");
    codegen::ops::derive_uuid_column_flags(f.b, f.entity);
    CHECK(f.json("has_uuid_columns") == "false");
    CHECK(f.json("has_render_uuid_columns") == "true");

    codegen::tests::entity_fixture g(R"j({
        "natural_keys": [{"column": "party_id", "is_uuid": true}]})j");
    codegen::ops::derive_uuid_column_flags(g.b, g.entity);
    CHECK(g.json("has_uuid_columns") == "true");
}

TEST_CASE("render kind flags skip columns off the wire",
    "[codegen][entity][gates]") {
    codegen::tests::entity_fixture f(R"j({
        "columns": [{"name": "a", "render_is_timestamp": true, "no_wire": true},
            {"name": "b", "render_is_int": true, "sql_only": true}],
        "natural_keys": [{"column": "n", "is_int": true}]})j");
    codegen::ops::derive_render_kind_flags(f.b, f.entity);
    CHECK(f.json("has_render_datetime_columns") == "false");
    CHECK(f.json("has_render_string_columns") == "true");
}

/*
 * ORE Studio's trade: its enum columns' domain headers, sorted and unique.
 */
TEST_CASE("the mapper includes each enum column's domain header once",
    "[codegen][entity][gates]") {
    codegen::tests::entity_fixture f(R"j({"columns": [
        {"name": "origin", "is_enum": true,
         "cpp_type": "ores::trading::domain::origin"},
        {"name": "booking_nature", "is_enum": true, "render_is_enum": true,
         "cpp_type": "ores::trading::domain::booking_nature"},
        {"name": "again", "is_enum": true,
         "cpp_type": "ores::trading::domain::origin"},
        {"name": "hidden", "is_enum": true, "sql_only": true,
         "cpp_type": "ores::trading::domain::hidden"},
        {"name": "plain", "is_enum": true, "cpp_type": "std::string"}],
        "primary_key": {"columns": [{"is_timestamp": true}]}})j");
    codegen::ops::derive_mapper_include_gates(f.b, f.entity);
    CHECK(f.json("mapper_enum_includes") == R"j([
  {
    "path": "ores.trading.api/domain/booking_nature.hpp"
  },
  {
    "path": "ores.trading.api/domain/origin.hpp"
  }
])j");
    CHECK(f.json("mapper_uses_optional") == "false");
    CHECK(f.json("mapper_uses_string") == "false");
    CHECK(f.json("mapper_uses_datetime") == "true");
    CHECK(f.json("mapper_uses_string_view") == "true");
}

/*
 * ORE Studio's convention finds by a code under its party: the oracle's
 * has_uuid_include is the parent column itself.
 */
TEST_CASE("the uuid include gate keeps Python's or operand",
    "[codegen][entity][gates]") {
    codegen::tests::entity_fixture f(R"j({
        "service_find_by_code": {"parent_column": "party_id"},
        "primary_key": {"columns": [{"is_uuid": false}],
                        "party_sql_key": "party_id"}})j");
    codegen::ops::derive_uuid_include_gates(f.b, f.entity);
    CHECK(f.json("has_uuid_include") == R"("party_id")");
    CHECK(f.json("repository_uses_uuid_io") == "true");

    codegen::tests::entity_fixture g(R"j({
        "foreign_keys": [{"list_by_uuid": true}]})j");
    codegen::ops::derive_uuid_include_gates(g.b, g.entity);
    CHECK(g.json("has_uuid_include") == "true");
    CHECK(g.json("repository_uses_uuid_io") == "false");
}
