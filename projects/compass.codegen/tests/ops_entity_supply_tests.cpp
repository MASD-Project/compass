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

void derive(codegen::tests::entity_fixture& f) {
    codegen::ops::derive_supply(f.b, f.entity);
    codegen::ops::derive_sample_tokens(f.b, f.entity);
}

}

/*
 * ORE Studio's notification_delivery, trimmed to four of its columns;
 * expected values are the oracle's for those columns.
 */
TEST_CASE("a uuid key is minted and every column is read in order",
    "[codegen][entity][supply]") {
    codegen::tests::entity_fixture f(R"j({
     "primary_key": {"column": "id", "type": "uuid",
       "cpp_type": "boost::uuids::uuid", "group_prefix": "",
       "columns": [{"column": "id", "type": "uuid",
         "cpp_type": "boost::uuids::uuid", "group_prefix": ""}]},
     "columns": [
      {"name": "notification_id", "type": "uuid",
       "cpp_type": "boost::uuids::uuid", "group_prefix": ""},
      {"name": "channel_code", "type": "text", "cpp_type": "std::string",
       "group_prefix": ""},
      {"name": "outcome", "type": "text", "cpp_type": "domain::delivery_outcome",
       "is_enum": true, "default_value": "domain::delivery_outcome::pending",
       "group_prefix": ""},
      {"name": "failure_reason", "type": "text",
       "cpp_type": "std::optional<std::string>", "group_prefix": ""}]})j");
    derive(f);

    const auto pk = member(f.tables, f.entity, "primary_key");
    for (const auto key : {pk, items(f.tables, pk, "columns").front()}) {
        CHECK(f.json(key, "is_minted") == "true");
        CHECK(f.json(key, "is_user_supplied") == "false");
        CHECK(f.json(key, "member_access") == "\"id\"");
        CHECK(f.json(key, "positional_index") == "<absent>");
    }

    const auto columns = items(f.tables, f.entity, "columns");
    CHECK(f.json(columns[0], "is_user_supplied") == "true");
    CHECK(f.json(columns[0], "member_access") == "\"notification_id\"");
    CHECK(f.json(columns[0], "positional_index") == "0");
    CHECK(f.json(columns[0], "sample_token") ==
        "\"3f2504e0-4f89-11d3-9a0c-0305e82c3301\"");
    CHECK(f.json(columns[1], "sample_token") == "\"sample\"");
    CHECK(f.json(columns[2], "enum_short_name") == "\"delivery_outcome\"");
    CHECK(f.json(columns[2], "sample_token") == "\"pending\"");
    CHECK(f.json(columns[2], "is_bool") == "false");
    CHECK(f.json(columns[3], "positional_index") == "3");
    CHECK(f.json(columns[3], "sample_token") == "\"sample\"");

    CHECK(f.json("user_supplied_count") == "4");
    CHECK(f.json("add_positional_count") == "6");
    CHECK(f.json("has_bool_columns") == "false");
    CHECK(f.json("has_session_party_columns") == "false");
    CHECK(f.json("malformed_token_column") == R"j({
  "error": "Invalid value for notification_id",
  "kind": "value",
  "name": "notification_id",
  "positional_index": 0,
  "token": "not-a-value"
})j");
}

/*
 * ORE Studio's calendar_exception; expected values are the oracle's.
 */
TEST_CASE("a bool column is the first a malformed token can fail",
    "[codegen][entity][supply]") {
    codegen::tests::entity_fixture f(R"j({
     "primary_key": {"column": "id", "type": "uuid", "group_prefix": "",
       "columns": [{"column": "id", "type": "uuid", "group_prefix": ""}]},
     "columns": [
      {"name": "is_business_day", "cpp_type": "bool", "group_prefix": ""},
      {"name": "description", "cpp_type": "std::optional<std::string>",
       "group_prefix": ""}]})j");
    derive(f);

    const auto columns = items(f.tables, f.entity, "columns");
    CHECK(f.json(columns[0], "is_bool") == "true");
    CHECK(f.json(columns[0], "sample_token") == "\"false\"");
    CHECK(f.json(columns[1], "positional_index") == "1");
    CHECK(f.json("has_bool_columns") == "true");
    CHECK(f.json("user_supplied_count") == "2");
    CHECK(f.json("add_positional_count") == "4");
    CHECK(f.json("malformed_token_column") == R"j({
  "error": "is_business_day must be 'true' or 'false'.",
  "kind": "bool",
  "name": "is_business_day",
  "positional_index": 0,
  "token": "maybe"
})j");
}

/*
 * ORE Studio's diff_entry has no primary key; the oracle counts the empty
 * key Python's .get('primary_key', {}) gives it as a user-supplied member.
 */
TEST_CASE("an entity without a key counts an empty user-supplied key",
    "[codegen][entity][supply]") {
    codegen::tests::entity_fixture f(R"j({"columns": [
      {"name": "field_name", "cpp_type": "std::string"},
      {"name": "old_value", "cpp_type": "std::string"}]})j");
    derive(f);

    CHECK(f.json("primary_key") == "<absent>");
    CHECK(f.json("user_supplied_count") == "3");
    CHECK(f.json("add_positional_count") == "5");
    CHECK(f.json(items(f.tables, f.entity, "columns").front(),
        "positional_index") == "1");
    CHECK(f.json("malformed_token_column") == R"j({
  "error": "Invalid value for ",
  "kind": "value",
  "name": "",
  "positional_index": 0,
  "token": "not-a-value"
})j");
}

TEST_CASE("an entity whose columns all convert has no malformed column",
    "[codegen][entity][supply]") {
    codegen::tests::entity_fixture f(R"j({
     "primary_key": {"column": "code", "type": "text",
       "cpp_type": "std::string", "columns": [{"column": "code",
       "type": "text", "cpp_type": "std::string"}]},
     "columns": [{"name": "name", "cpp_type": "std::string"}]})j");
    derive(f);
    CHECK(f.json("malformed_token_column") == "null");
    CHECK(f.json(items(f.tables, member(f.tables, f.entity, "primary_key"),
        "columns").front(), "positional_index") == "0");
}
