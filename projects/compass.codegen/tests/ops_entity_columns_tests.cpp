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

#include <string>
#include <utility>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "entity_fixture.hpp"

namespace {

using namespace compass;
using codegen::ops::values::items;

/*
 * Columns of ORE Studio models, as loaded. The expected values below are
 * the oracle's for the same columns: account (shared, default_party_id,
 * email), login_info (last_login), bond_instrument (notional),
 * seed_profile (bullets_json) and calendar_event's event_date key.
 */
constexpr std::string_view entity = R"j({
 "cpp": {"iterator_var": "a"},
 "columns": [
  {"name": "shared", "type": "boolean", "cpp_type": "bool",
   "nullable": false},
  {"name": "default_party_id", "type": "uuid",
   "cpp_type": "std::optional<boost::uuids::uuid>", "nullable": true},
  {"name": "email", "type": "text", "cpp_type": "std::string",
   "nullable": false, "unique": true,
   "generator_expr": "std::string(faker::internet::email())"},
  {"name": "last_login", "type": "timestamp with time zone",
   "cpp_type": "std::optional<std::chrono::system_clock::time_point>",
   "nullable": true},
  {"name": "notional", "type": "numeric(38, 12)",
   "cpp_type": "std::optional<ores::utility::decimal::decimal>",
   "nullable": true},
  {"name": "bullets_json", "type": "jsonb", "cpp_type": "std::string",
   "nullable": false, "generator_expr": "std::string(\"[]\")"},
  {"name": "payload", "type": "jsonb", "cpp_type": "std::string",
   "nullable": false, "no_wire": true}],
 "natural_keys": [{"column": "event_date", "type": "date",
   "cpp_type": "std::chrono::year_month_day"}]
})j";

using expectations = std::vector<std::pair<std::string_view, std::string>>;

void check(codegen::tests::entity_fixture& f, model::types::value_id col,
    const expectations& expected) {
    for (const auto& [key, value] : expected) {
        INFO(key);
        CHECK(f.json(col, key) == value);
    }
}

}

TEST_CASE("the column loop derives each layer's type flags",
    "[codegen][entity][columns]") {
    codegen::tests::entity_fixture f(entity);
    codegen::ops::derive_column_flags(f.b, f.entity);
    const auto columns = items(f.tables, f.entity, "columns");

    check(f, columns[0], {{"is_simple", "true"}, {"default_value", "\"false\""},
        {"render_is_bool", "true"}, {"render_label", "\"Shared\""},
        {"is_plain_string", "false"}, {"iter_var", "\"a\""},
        {"no_wire", "false"}, {"sql_only", "false"}});
    check(f, columns[1], {{"is_optional_uuid", "true"}, {"is_uuid", "false"},
        {"is_already_optional", "false"}, {"is_simple", "false"},
        {"render_is_optional_uuid", "true"},
        {"render_label", "\"Default Party ID\""},
        {"default_value", "<absent>"}});
    check(f, columns[2], {{"is_simple", "true"},
        {"is_plain_string", "false"}, {"requires_unique_suffix", "true"},
        {"render_is_string", "true"}});
    check(f, columns[3], {{"is_optional_timestamp", "true"},
        {"is_required_timestamp", "false"}, {"is_already_optional", "false"},
        {"render_is_optional_timestamp", "true"},
        {"render_is_timestamp", "false"}, {"is_nullable_string", "false"}});
    check(f, columns[4], {{"is_decimal", "true"},
        {"is_optional_decimal", "true"}, {"is_required_decimal", "false"},
        {"is_already_optional", "false"}, {"is_text_stored", "false"},
        {"render_is_optional_decimal", "true"},
        {"render_is_decimal", "false"},
        {"render_cpp_type",
            "\"std::optional<ores::utility::decimal::decimal>\""}});
    check(f, columns[5], {{"is_text_stored", "true"}, {"is_simple", "true"},
        {"generator_expr", R"j("std::string(\"[]\")")j"},
        {"render_label", "\"Bullets Json\""}});
    check(f, columns[6], {{"generator_expr", R"j("std::string(\"{}\")")j"},
        {"no_wire", "true"}});

    const auto key = items(f.tables, f.entity, "natural_keys").front();
    check(f, key, {{"is_date", "false"}, {"is_simple", "true"},
        {"is_plain_string", "false"}, {"is_text_stored", "false"},
        {"render_is_date", "true"}, {"iter_var", "\"a\""}});
}

TEST_CASE("a domain-grouped column takes its render type from its group",
    "[codegen][entity][columns]") {
    codegen::tests::entity_fixture f(R"j({
     "domain_group_fields": [{"name": "economics.trade_date",
       "cpp_type": " std::chrono::year_month_day "}],
     "columns": [{"name": "trade_date", "type": "date",
       "cpp_type": "std::string", "nullable": false}]})j");
    codegen::ops::derive_column_flags(f.b, f.entity);
    const auto col = items(f.tables, f.entity, "columns").front();
    check(f, col, {{"render_cpp_type", "\"std::chrono::year_month_day\""},
        {"is_date", "true"}, {"is_required_date", "true"},
        {"is_simple", "false"}, {"is_text_stored", "false"},
        {"iter_var", "\"e\""}});
}

TEST_CASE("the column aggregates read the derived flags",
    "[codegen][entity][columns]") {
    codegen::tests::entity_fixture f(entity);
    codegen::ops::derive_column_flags(f.b, f.entity);
    codegen::ops::derive_column_aggregates(f.b, f.entity);
    check(f, f.entity, {{"has_enum_columns", "false"},
        {"has_no_wire_columns", "true"}, {"wire_left_out", "1"},
        {"has_inet_columns", "false"}, {"has_base64_columns", "false"},
        {"has_plain_date_columns", "false"},
        {"has_decimal_columns", "true"}, {"has_time_columns", "false"}});
}

/*
 * A time of day is SQL time without time zone; its zone lives in another
 * column. The repository row holds it as "HH:MM:SS" text, so no generic
 * member branch takes it. A model property never sets the flags.
 */
TEST_CASE("a time column takes the time flags from its SQL type",
    "[codegen][entity][columns]") {
    codegen::tests::entity_fixture f(R"j({
     "columns": [
      {"name": "cut_time", "type": "time",
       "cpp_type": "ores::platform::time::time_of_day", "nullable": false},
      {"name": "latest_cut_time", "type": "time",
       "cpp_type": "std::optional<ores::platform::time::time_of_day>",
       "nullable": true},
      {"name": "label", "type": "text", "cpp_type": "std::string",
       "nullable": false, "is_time": true, "is_optional_time": true}]})j");
    codegen::ops::derive_column_flags(f.b, f.entity);
    codegen::ops::derive_column_aggregates(f.b, f.entity);
    const auto columns = items(f.tables, f.entity, "columns");

    check(f, columns[0], {{"is_time", "true"}, {"is_optional_time", "false"},
        {"is_text_stored", "true"}, {"is_simple", "false"},
        {"is_plain_string", "false"}, {"is_already_optional", "false"},
        {"is_date", "false"}});
    check(f, columns[1], {{"is_time", "false"}, {"is_optional_time", "true"},
        {"is_text_stored", "true"}, {"is_simple", "false"},
        {"is_already_optional", "false"}, {"is_nullable_string", "false"},
        {"is_nullable_numeric", "false"}});
    check(f, columns[2], {{"is_time", "false"},
        {"is_optional_time", "false"}, {"is_simple", "true"},
        {"is_text_stored", "false"}});
    check(f, f.entity, {{"has_time_columns", "true"},
        {"has_plain_date_columns", "false"}});
}
