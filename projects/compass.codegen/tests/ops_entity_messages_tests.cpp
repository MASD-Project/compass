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

#include <chrono>
#include <set>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "entity_fixture.hpp"

namespace {

using namespace compass;
using codegen::ops::values::items;
using codegen::ops::values::member;
using strings = std::vector<std::string>;
using model::types::value_id;

/*
 * ores.compute.app as the oracle shows it, trimmed: declared key name, a
 * uuid storage key, versioned, with an app_version entity beside it.
 */
constexpr auto app = R"j({"component": "compute",
    "entity_singular": "app", "entity_plural": "apps",
    "entity_plural_short": "apps", "key_field": "name",
    "has_audit_columns": true,
    "primary_key": {"columns": [
        {"column": "id", "cpp_type": "boost::uuids::uuid"}]},
    "natural_keys": [{"column": "name", "cpp_type": "std::string"}],
    "columns": [{"name": "description", "cpp_type": "std::string"},
        {"name": "modified_by", "cpp_type": "std::string"}]})j";

const std::set<std::string, std::less<>> app_siblings{"app", "app_version"};

value_id message(codegen::tests::entity_fixture& f, std::string_view name) {
    for (const auto m : items(f.tables, f.entity, "messages")) {
        if (codegen::ops::values::str(f.tables, m, "name") == name)
            return m;
    }
    return value_id::none();
}

strings field_names(codegen::tests::entity_fixture& f, value_id m) {
    strings out;
    for (const auto field : items(f.tables, m, "fields"))
        out.emplace_back(codegen::ops::values::str(f.tables, field, "name"));
    return out;
}

}

TEST_CASE("an entity derives its records and operations in order",
    "[codegen][entity][messages]") {
    codegen::tests::entity_fixture f(app);
    codegen::ops::derive_messages(f.b, f.entity, {}, app_siblings);
    strings names;
    for (const auto m : items(f.tables, f.entity, "messages"))
        names.emplace_back(codegen::ops::values::str(f.tables, m, "name"));
    CHECK(names == strings{"app_key", "app_write", "app_change", "app_removal",
        "app_lookup", "apps_filter", "app_event", "apps_version_key",
        "apps_versions_filter", "list_apps_request", "list_apps_response",
        "get_app_request", "get_app_response", "get_many_apps_request",
        "get_many_apps_response", "put_app_request", "put_app_response",
        "put_many_apps_request", "put_many_apps_response",
        "delete_app_request", "delete_app_response",
        "delete_many_apps_request", "delete_many_apps_response",
        "list_apps_versions_request", "list_apps_versions_response",
        "get_apps_version_request", "get_apps_version_response"});

    CHECK(field_names(f, message(f, "app_key")) == strings{"name"});
    CHECK(field_names(f, message(f, "app_write")) ==
        strings{"id", "name", "description"});
    CHECK(field_names(f, message(f, "apps_filter")) == strings{"id_one_of"});

    const auto removal = items(f.tables, message(f, "app_removal"), "fields");
    CHECK(f.json(removal[1], "default") ==
        R"("ores::utility::domain::removal_precondition")");
    const auto lookup = items(f.tables, message(f, "app_lookup"), "fields");
    CHECK(f.json(lookup[1], "ts_type") == R"("App | null")");

    const auto get_version = message(f, "get_apps_version_request");
    CHECK(f.json(get_version, "subject") ==
        R"("compute.v1.apps_versions.get")");
    CHECK(f.json(get_version, "verb") == R"("get_version")");
    CHECK(f.json(get_version, "response_type") ==
        R"("get_apps_version_response")");
    CHECK(f.json(get_version, "requires_session") == R"("true")");
    CHECK(f.json(message(f, "app_key"), "requires_session") == "<absent>");
    CHECK(f.json(message(f, "list_apps_request"), "subject") ==
        R"("compute.v1.apps.list")");
}

TEST_CASE("the versions facet takes the singular when no sibling owns it",
    "[codegen][entity][messages]") {
    codegen::tests::entity_fixture f(app);
    codegen::ops::derive_messages(f.b, f.entity, {}, {});
    CHECK(!message(f, "app_version_key").is_none());
    CHECK(!message(f, "get_app_version_request").is_none());
    CHECK(message(f, "apps_version_key").is_none());
}

TEST_CASE("a read-only entity keeps its reads and declared messages",
    "[codegen][entity][messages]") {
    codegen::tests::entity_fixture f(R"j({"component": "iam",
        "entity_singular": "session", "entity_plural": "sessions",
        "read_only": true,
        "primary_key": {"columns": [{"column": "id", "is_uuid": true}]},
        "declared_messages": [{"name": "end_session_request",
            "subject": "iam.v1.ops.end_session", "fields": []},
          {"name": "put_session_request", "fields": []}]})j");
    codegen::ops::derive_messages(f.b, f.entity, {}, {});
    strings names;
    for (const auto m : items(f.tables, f.entity, "messages"))
        names.emplace_back(codegen::ops::values::str(f.tables, m, "name"));
    CHECK(names == strings{"session_key", "session_lookup", "sessions_filter",
        "session_event",
        "list_sessions_request", "list_sessions_response",
        "get_session_request", "get_session_response",
        "get_many_sessions_request", "get_many_sessions_response",
        "end_session_request"});
    const auto key = items(f.tables, message(f, "session_key"), "fields");
    CHECK(f.json(key[0], "cpp_type") == R"("boost::uuids::uuid")");
    const auto declared = items(f.tables, f.entity, "declared_messages");
    CHECK(f.json(declared[0], "requires_session") == R"("true")");
    CHECK(f.json(message(f, "end_session_request"), "requires_session") ==
        R"("true")");
}

/*
 * ores.marketdata.market_observation: a relation and a filterable
 * timestamp natural key give equals, one-of and range members.
 */
TEST_CASE("the filter record states one member per way a list narrows",
    "[codegen][entity][messages]") {
    codegen::tests::entity_fixture f(R"j({"component": "marketdata",
        "entity_singular": "market_observation",
        "entity_plural": "market_observations",
        "extra_list_requests": [{"filter_column": "series_id"}],
        "primary_key": {"columns": [
            {"column": "id", "cpp_type": "boost::uuids::uuid"}]},
        "natural_keys": [
            {"column": "series_id", "cpp_type": "boost::uuids::uuid"},
            {"column": "observation_datetime", "filterable": true,
             "cpp_type": "std::chrono::system_clock::time_point"}]})j");
    codegen::ops::derive_messages(f.b, f.entity, {}, {});
    CHECK(field_names(f, message(f, "market_observations_filter")) ==
        strings{"series_id", "observation_datetime", "id_one_of",
            "series_id_one_of", "observation_datetime_one_of",
            "observation_datetime_from", "observation_datetime_to"});
    CHECK(!message(f, "list_by_series_id_market_observations_request")
        .is_none());
}

TEST_CASE("every message and declared message repeats its name",
    "[codegen][entity][messages]") {
    codegen::tests::entity_fixture f(R"j({"component": "iam",
        "entity_singular": "tenant",
        "primary_key": {"columns": [{"column": "id"}]},
        "declared_messages": [{"name": "end_session_request",
            "subject": "iam.v1.ops.end_session", "fields": []}]})j");
    codegen::ops::derive_messages(f.b, f.entity, {}, {});
    codegen::ops::derive_message_struct_names(f.b, f.entity);
    CHECK(f.json(message(f, "tenant_key"), "struct_name") ==
        R"("tenant_key")");
    const auto declared = items(f.tables, f.entity, "declared_messages");
    CHECK(f.json(declared[0], "struct_name") == R"("end_session_request")");
    CHECK(f.json(message(f, "end_session_request"), "struct_name") ==
        R"("end_session_request")");
}

TEST_CASE("the filter record states its members, search and range",
    "[codegen][entity][messages]") {
    codegen::tests::entity_fixture f(R"j({"entity_singular": "result",
        "entity_plural": "results",
        "primary_key": {"columns": [{"column": "id"}]},
        "columns": [{"name": "name", "searchable": true},
            {"name": "code", "searchable": true},
            {"name": "as_of", "filterable": true,
             "cpp_type": "std::optional<std::chrono::year_month_day>"}]})j");
    codegen::ops::derive_filter_record(f.b, f.entity, {"m.org", true, true});
    const auto members = items(f.tables, member(f.tables, f.entity,
        "filter_members"));
    REQUIRE(members.size() == 5);
    CHECK(f.json(members[0], "is_nullable") == "true");
    CHECK(f.json(members[0], "read_type") ==
        R"("std::chrono::year_month_day")");
    CHECK(f.json(members[1], "is_key") == "true");
    CHECK(f.json(members[3], "member") == R"("as_of_from")");
    CHECK(f.json(members[3], "is_from") == "true");
    CHECK(f.json(members[4], "is_to") == "true");
    CHECK(f.json(members[4], "is_timestamp") == "false");
    CHECK(f.json("search_columns") == R"("{\"name\", \"code\"}")");
    CHECK(f.json("has_search") == "true");
    CHECK(f.json("has_range_filter") == "true");
    CHECK(f.json("has_filter_record") == "true");
    CHECK(f.json("filter_type") == R"("messaging::results_filter")");
    CHECK(f.json("payload_member") == R"("result_value")");

    codegen::ops::derive_filter_record(f.b, f.entity, {"m.org", true, false});
    CHECK(f.json("has_filter_record") == "false");
}

/*
 * ores.compute.result: the envelope's Result owns the name, so the
 * entity's own interface is imported as ResultEntity (oracle).
 */
TEST_CASE("an entity whose name a utility interface owns is aliased",
    "[codegen][entity][messages]") {
    codegen::tests::entity_fixture f(R"j({"component": "compute",
        "entity_singular": "result",
        "messages": [{"fields": [
            {"cpp_type": "ores::utility::domain::result", "ts_type": "Result"},
            {"cpp_type": "std::optional<ores::compute::domain::result>",
             "ts_type": "Result | null"}]}]})j");
    codegen::ops::derive_ts_alias(f.b, f.entity);
    CHECK(f.json("entity_ts_alias") == R"("ResultEntity")");
    const auto fields = items(f.tables,
        items(f.tables, f.entity, "messages")[0], "fields");
    CHECK(f.json(fields[0], "ts_type") == R"("Result")");
    CHECK(f.json(fields[1], "ts_type") == R"("ResultEntity | null")");

    codegen::tests::entity_fixture free(R"j({"component": "compute",
        "entity_singular": "app", "messages": [{"fields": [
            {"cpp_type": "ores::utility::domain::result"}]}]})j");
    codegen::ops::derive_ts_alias(free.b, free.entity);
    CHECK(free.json("entity_ts_alias") == "<absent>");
}

TEST_CASE("write fields state the conversions their members need",
    "[codegen][entity][messages]") {
    codegen::tests::entity_fixture f(R"j({
        "primary_key": {"columns": [{"column": "id"}]},
        "columns": [
            {"name": "kind", "is_enum": true, "group_prefix": "economics.",
             "cpp_type": "std::string"},
            {"name": "trade_date", "group_prefix": "economics.",
             "cpp_type": "std::string"},
            {"name": "started_at", "cpp_type": "std::string"}],
        "domain_group_fields": [
            {"name": "economics.kind", "cpp_type": "trade_kind",
             "default_value": "trade_kind::spot"},
            {"name": "economics.trade_date",
             "cpp_type": "std::chrono::year_month_day"}]})j");
    codegen::ops::derive_write_fields(f.b, f.entity, {});
    const auto fields = items(f.tables, member(f.tables, f.entity,
        "write_fields"));
    REQUIRE(fields.size() == 3);
    CHECK(f.json(fields[1], "domain_member") == R"("economics.kind")");
    CHECK(f.json(fields[1], "enum_from_string") ==
        R"("trade_kind_from_string")");
    CHECK(f.json(fields[1], "enum_default") == R"("trade_kind::spot")");
    CHECK(f.json(fields[2], "date_from_string") == "true");
    CHECK(f.json(fields[0], "date_from_string") == "<absent>");
    const auto preserved = items(f.tables, member(f.tables, f.entity,
        "preserved_fields"));
    REQUIRE(preserved.size() == 1);
    CHECK(f.json(preserved[0], "member") == R"("started_at")");
    CHECK(f.json("has_preserved_fields") == "true");
}
