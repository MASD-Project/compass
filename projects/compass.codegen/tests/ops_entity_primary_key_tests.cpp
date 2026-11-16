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
 * ORE Studio's session: a uuid and a timestamp make its key; expected
 * values are the oracle's.
 */
TEST_CASE("a compound key with a timestamp column converts each column",
    "[codegen][entity][primary_key]") {
    codegen::tests::entity_fixture f(R"j({
     "entity_singular": "session",
     "presentation": {"key_field": "id"},
     "primary_key": {"column": "id", "type": "uuid",
       "columns": [
         {"column": "id", "type": "uuid", "cpp_type": "boost::uuids::uuid"},
         {"column": "start_time", "type": "timestamp with time zone",
          "cpp_type": "std::chrono::system_clock::time_point"}]}})j");
    codegen::ops::derive_primary_key(f.b, f.entity);
    codegen::ops::project_primary_key_extra_columns(f.b, f.entity);
    const auto pk = member(f.tables, f.entity, "primary_key");
    const auto columns = items(f.tables, pk, "columns");
    CHECK(f.json(columns[0], "uuid_check_fn") ==
        R"j("ores_utility_nil_uuid_fn()")j");
    CHECK(f.json(columns[0], "render_label") == R"j("ID")j");
    CHECK(f.json(columns[1], "is_timestamp") == "true");
    CHECK(f.json(columns[1], "last") == "true");
    CHECK(f.json(columns[0], "last") == "<absent>");
    CHECK(f.json(columns[1], "key_text") ==
        R"j("ores::platform::time::datetime::to_db_string(key.start_time)")j");
    CHECK(f.json(columns[0], "value_text") == "<absent>");
    CHECK(f.json(columns[1], "value_text") ==
        R"j("ores::platform::time::datetime::to_db_string(v.start_time)")j");
    CHECK(f.json("has_timestamp_key_columns") == "true");
    CHECK(f.json("has_date_or_timestamp_natural_keys") == "true");
    CHECK(f.json(pk, "is_compound") == "true");
    CHECK(f.json(pk, "cpp_type") == R"j("boost::uuids::uuid")j");
    CHECK(f.json(pk, "gist_column_clause") ==
        R"j("id WITH =,\n        start_time WITH =,")j");
    CHECK(f.json(pk, "index_suffix") == R"j("id_start_time")j");
    CHECK(f.json(pk, "typed_params") ==
        R"j("const std::string& id, const std::string& start_time")j");
    CHECK(f.json(pk, "version_key_args") ==
        R"j("boost::uuids::to_string(request.key.session.id), )j"
        R"j(ores::platform::time::datetime::to_db_string()j"
        R"j(request.key.session.start_time)")j");
    CHECK(f.json(pk, "log_fields") ==
        R"j("\"id: \" << id << \" start_time: \" << start_time")j");
    CHECK(f.json(pk, "batch_size_mismatch_check") ==
        R"j("start_times.size() != ids.size()")j");
    CHECK(f.json(pk, "batch_requested_insert") ==
        R"j("requested.emplace(ids[i], start_times[i]);")j");
    CHECK(f.json(pk, "value_text") == R"j("boost::uuids::to_string(v.id)")j");
    CHECK(f.json(pk, "notify_declarations") == R"j("changed_id uuid;")j");
    CHECK(f.json(pk, "notify_key_object") ==
        R"j("jsonb_build_object('id', changed_id)")j");
    CHECK(f.json(pk, "key_equals_v") == R"j("decoded->key.id == v.id")j");
    const auto copies = items(f.tables, pk, "key_columns");
    REQUIRE(copies.size() == 2);
    CHECK(f.json(copies[1], "value_text") == "<absent>");
    CHECK(f.json(copies[1], "last") == "true");
    const auto extra = items(f.tables, pk, "extra_columns");
    REQUIRE(extra.size() == 1);
    CHECK(f.json(extra[0], "column") == R"j("start_time")j");
}

/*
 * ORE Studio's average_ois_convention: a :sql_key: party widens the
 * storage key, not the key the API takes.
 */
TEST_CASE("a sql_key party widens the storage key",
    "[codegen][entity][primary_key]") {
    codegen::tests::entity_fixture f(R"j({
     "entity_singular": "average_ois_convention",
     "columns": [{"name": "party_id", "type": "uuid", "sql_key": true}],
     "primary_key": {"column": "id", "type": "text",
       "columns": [{"column": "id", "type": "text"}]}})j");
    codegen::ops::derive_primary_key(f.b, f.entity);
    const auto pk = member(f.tables, f.entity, "primary_key");
    CHECK(f.json(pk, "column_list") == R"j("id, party_id")j");
    CHECK(f.json(pk, "party_sql_key") == "true");
    CHECK(f.json(pk, "party_member") == R"j("party_id")j");
    CHECK(f.json(pk, "where_new") ==
        R"j("id = NEW.id and party_id = NEW.party_id")j");
    CHECK(f.json(pk, "index_suffix") == R"j("id")j");
    CHECK(f.json(pk, "is_single_uuid") == "false");
    CHECK(f.json(pk, "cpp_type") == R"j("std::string")j");
    CHECK(f.json(pk, "notify_declarations") == R"j("changed_id text;")j");
    CHECK(f.json("has_timestamp_key_columns") == "false");
    CHECK(f.json("has_date_or_timestamp_natural_keys") == "<absent>");
}
