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

void derive(codegen::tests::entity_fixture& f) {
    codegen::ops::derive_natural_key_flags(f.b, f.entity);
    codegen::ops::derive_key_aggregates(f.b, f.entity);
}

}

/*
 * ORE Studio's calendar_event, trimmed; expected values are the oracle's.
 */
TEST_CASE("a composite natural key derives its flags and names",
    "[codegen][entity][keys]") {
    codegen::tests::entity_fixture f(R"j({
     "primary_key": {"column": "id", "type": "uuid"},
     "natural_keys": [
      {"column": "calendar_code", "type": "text", "cpp_type": "std::string",
       "generator_expr": "std::string(\"TARGET\")"},
      {"column": "event_date", "type": "date",
       "cpp_type": "std::chrono::year_month_day"},
      {"column": "diary_entry_type", "type": "text",
       "cpp_type": "std::string"}],
     "columns": [{"name": "description", "cpp_type": "std::string"}]})j");
    derive(f);

    const auto keys = items(f.tables, f.entity, "natural_keys");
    CHECK(f.json(keys[0], "render_label") == "\"Calendar Code\"");
    CHECK(f.json(keys[0], "is_date") == "false");
    CHECK(f.json(keys[0], "iter_var") == "\"e\"");
    CHECK(f.json(keys[1], "is_date") == "true");
    CHECK(f.json(keys[1], "is_timestamp") == "false");
    CHECK(f.json(keys[2], "last") == "false");
    CHECK(f.json(keys[0], "last") == "<absent>");

    CHECK(f.json("has_multiple_natural_keys") == "true");
    CHECK(f.json("has_text_natural_keys") == "true");
    CHECK(f.json("has_date_natural_keys") == "true");
    CHECK(f.json("has_date_or_timestamp_natural_keys") == "true");
    CHECK(f.json("natural_keys_composite_columns") ==
        "\"calendar_code, event_date, diary_entry_type\"");
    CHECK(f.json("natural_keys_composite_name") ==
        "\"calendar_code_event_date_diary_entry_type\"");
    CHECK(f.json("needs_counter") == "true");
    CHECK(f.json("has_unique_suffix_columns") == "false");
    CHECK(f.json("has_uuid_to_string_generator") == "false");
    CHECK(f.json("unique_keys") == "[]");
}

TEST_CASE("a uuid natural key with no plain columns keeps its last marker",
    "[codegen][entity][keys]") {
    codegen::tests::entity_fixture f(R"j({
     "cpp": {"iterator_var": "s"},
     "primary_key": {"column": "id", "type": "uuid",
       "generator_expr": "boost::uuids::to_string(x)"},
     "natural_keys": [{"column": "session_id", "type": "uuid",
       "nullable": true}]})j");
    derive(f);
    const auto key = items(f.tables, f.entity, "natural_keys").front();
    CHECK(f.json(key, "last") == "true");
    CHECK(f.json(key, "iter_var") == "\"s\"");
    CHECK(f.json(key, "is_optional_uuid") == "true");
    CHECK(f.json(key, "render_label") == "\"Session ID\"");
    CHECK(f.json("has_text_natural_keys") == "false");
    CHECK(f.json("needs_counter") == "false");
    CHECK(f.json("has_uuid_to_string_generator") == "true");
    CHECK(f.json("natural_keys_composite_columns") == "<absent>");
}

TEST_CASE("unique columns and generators naming idx need the counter",
    "[codegen][entity][keys]") {
    codegen::tests::entity_fixture f(R"j({
     "primary_key": {"column": "id", "type": "uuid"},
     "columns": [
      {"name": "email", "unique": true, "requires_unique_suffix": true},
      {"name": "seq", "generator_expr": "std::to_string(idx)"},
      {"name": "x_index", "generator_expr": "x_idx_y"}]})j");
    derive(f);
    CHECK(f.json("has_text_natural_keys") == "false");
    CHECK(f.json("unique_keys") == R"j([
  {
    "column": "email"
  }
])j");
    CHECK(f.json("has_unique_suffix_columns") == "true");
    CHECK(f.json("needs_counter") == "true");

    codegen::tests::entity_fixture idx(R"j({"natural_keys": [],
     "columns": [{"name": "seq", "generator_expr": "std::to_string(idx)"}]})j");
    derive(idx);
    CHECK(idx.json("has_text_natural_keys") == "true");
    CHECK(idx.json("has_multiple_natural_keys") == "false");
}
