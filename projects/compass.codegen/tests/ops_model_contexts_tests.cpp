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
#include "compass.codegen/ops/model_contexts.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.codegen/ops/values.hpp"
#include "compass.model/io/json.hpp"
#include "compass.model/ops/builder.hpp"

namespace {

using namespace compass;
using compass::model::types::value_id;
using strings = std::vector<std::string>;

/**
 * @brief A model read from JSON and the context one of the model contexts
 * builds from it, with an empty library.
 */
struct context_fixture {
    model::types::model_tables tables;
    model::ops::builder b{tables};
    value_id model;
    value_id library;
    value_id context = value_id::none();

    explicit context_fixture(std::string_view json) {
        model = model::io::read_json(b, json);
        library = b.detached_object();
    }

    value_id at(std::string_view key) {
        return codegen::ops::values::member(tables, context, key);
    }

    /**
     * @brief @p json as write_json writes it, without its line end.
     */
    std::string canon(std::string_view json) {
        std::string out;
        model::io::write_json(out, tables, model::io::read_json(b, json));
        out.pop_back();
        return out;
    }

    std::string json(value_id object, std::string_view key) {
        const auto v = codegen::ops::values::member(tables, object, key);
        if (v.is_none())
            return "<absent>";
        std::string out;
        model::io::write_json(out, tables, v);
        out.pop_back();
        return out;
    }
};

const codegen::types::entity_site trading_site{
    "projects/ores.trading/modeling/"
    "ores.trading.bond_fixed_leg_data_field_group.org",
    true, true};

codegen::types::entity_registry trading_registry() {
    codegen::types::entity_registry r;
    r.directories["projects/ores.trading/modeling"].group_names =
        {"bond_float_data", "bond_fixed_leg_data"};
    return r;
}

}

/*
 * ores.trading.bond_fixed_leg_data_field_group (oracle: the field_group
 * of its cpp_field_group.hpp context).
 */
TEST_CASE("a field group gains its include names, codec headers and "
    "TypeScript names", "[codegen][model_contexts][field_group]") {
    context_fixture f(R"j({"field_group": {"component": "trading",
        "subcomponent": "api", "entity_singular": "bond_fixed_leg_data",
        "description": "A fixed leg.\n\nRates  ",
        "fields": [{"name": "rates",
            "cpp_type": "std::vector<bond_float_data>"}]}})j");
    f.context = codegen::ops::field_group_context(f.b, f.library, f.model,
        "ores.trading.bond_fixed_leg_data_field_group", trading_registry(),
        trading_site);
    const auto group = f.at("field_group");
    CHECK(f.json(group, "component_include") == "\"trading.api\"");
    CHECK(f.json(group, "component_dir") == "\"ores.trading/api\"");
    CHECK(f.json(group, "component_include_upper") == "\"TRADING_API\"");
    CHECK(f.json(group, "entity_singular_upper") ==
        "\"BOND_FIXED_LEG_DATA\"");
    CHECK(f.json(group, "entity_pascal") == "\"BondFixedLegData\"");
    CHECK(f.json(group, "has_generated_wire_codec") == "true");
    CHECK(f.json(group, "wire_left_out") == "0");
    CHECK(f.json(group, "wire_codec_includes") ==
        f.canon("[\"ores.trading.api/domain/bond_float_data.hpp\"]"));
    CHECK(f.json(group, "ts_imports") == f.canon("[{\"module\": "
        "\"./bond_float_data.js\", \"pascal\": \"BondFloatData\"}]"));
    CHECK(f.json(group, "description_lines") ==
        f.canon("[\" * A fixed leg.\", \" *\", \" * Rates\"]"));
    const auto field = codegen::ops::values::items(f.tables, group,
        "fields").front();
    CHECK(f.json(field, "ts_type") == "\"BondFloatData[]\"");
    CHECK(f.json(field, "last") == "true");
    CHECK(f.json(codegen::ops::values::member(f.tables,
        f.at("ores.trading.bond_fixed_leg_data_field_group"), "field_group"),
        "entity_pascal") == "\"BondFixedLegData\"");
}

namespace {

const codegen::types::entity_site refdata_site{
    "projects/ores.refdata/modeling/ores.refdata.party_country_junction.org",
    false, true};

strings message_names(context_fixture& f, value_id owner) {
    strings out;
    for (const auto m : codegen::ops::values::items(f.tables, owner,
             "messages"))
        out.emplace_back(codegen::ops::values::str(f.tables, m, "name"));
    return out;
}

}

/*
 * ores.refdata.party_country_junction, which states :history: and
 * :events: (oracle: its cpp_domain_type_class.hpp context).
 */
TEST_CASE("a junction that announces its links gains its events and history",
    "[codegen][model_contexts][junction]") {
    context_fixture f(R"j({"junction": {"name": "party_countries",
        "name_singular": "party_country", "component": "refdata",
        "product": "ores", "history": true, "events": true,
        "wire_write_enabled": true,
        "sql": {"tablename": "ores_refdata_party_countries_tbl"},
        "left": {"column": "party_id", "type": "uuid", "list_by": true,
            "cpp_type": "boost::uuids::uuid", "column_title": "Party",
            "query_needs_str": true},
        "right": {"column": "country_alpha2_code", "type": "text",
            "cpp_type": "std::string", "column_title": "Country"},
        "repository": {"name_singular_words": "party country"}}})j");
    const auto [context, surface] = codegen::ops::junction_context(f.b,
        f.library, f.model, "ores.refdata.party_country_junction",
        codegen::types::entity_registry{}, refdata_site);
    f.context = context;
    const auto j = f.at("junction");
    CHECK(f.json(j, "events_channel") == "\"ores_refdata_party_countries\"");
    CHECK(f.json(j, "event_key_columns") == f.canon(R"([
        {"column": "party_id", "last": false},
        {"column": "country_alpha2_code", "last": true}])"));
    CHECK(f.json(j, "has_party_column") == "true");
    CHECK(f.json(j, "event_prefix") ==
        "\"refdata.v1.events.party_countries\"");
    CHECK(f.json(j, "history_entity_type") ==
        "\"ores.refdata.party_country\"");
    CHECK(f.json(j, "history_read_permission") ==
        "\"refdata::party_countries:read\"");
    const auto parent = codegen::ops::values::member(f.tables, j,
        "history_parent");
    CHECK(f.json(parent, "history_value") ==
        "\"boost::uuids::to_string(v.link.party_id)\"");
    CHECK(f.json(parent, "ts_type") == "\"string\"");
    CHECK(f.json(j, "service_uses_cstdint") == "true");
    CHECK(f.json(j, "name_singular_words") == "\"party country\"");
    CHECK(f.json(j, "tenant_where") == "\"\\\"tenant_id\\\"_c == tid\"");
    CHECK(message_names(f, j) == strings{"party_country_key",
        "party_country_write", "party_country_change",
        "party_country_removal", "party_country_lookup",
        "party_countries_filter", "party_country_event",
        "list_party_countries_request", "list_party_countries_response",
        "get_party_country_request", "get_party_country_response",
        "get_many_party_countries_request",
        "get_many_party_countries_response", "put_party_country_request",
        "put_party_country_response", "put_many_party_countries_request",
        "put_many_party_countries_response", "delete_party_country_request",
        "delete_party_country_response",
        "delete_many_party_countries_request",
        "delete_many_party_countries_response",
        "list_by_party_id_party_countries_request",
        "list_by_party_id_party_countries_response"});
    CHECK(f.json(surface, "entity_plural") == "\"party_countries\"");
    CHECK(f.json(surface, "subcomponent") == "\"api\"");
}

/*
 * ores.refdata.calendar_date: no wire writes, no events, a date side
 * (oracle: its cpp_domain_type_class.hpp context).
 */
TEST_CASE("a junction with no wire writes derives its reads only",
    "[codegen][model_contexts][junction]") {
    context_fixture f(R"j({"junction": {"name": "calendar_dates",
        "name_singular": "calendar_date", "component": "refdata",
        "product": "ores", "history": true, "wire_write_enabled": false,
        "left": {"column": "calendar_code", "type": "text", "list_by": true,
            "cpp_type": "std::string"},
        "right": {"column": "date", "type": "date", "query_needs_str": true,
            "cpp_type": "std::chrono::year_month_day"},
        "columns": [{"name": "is_business_day", "type": "boolean",
            "cpp_type": "bool", "nullable": false}]}})j");
    f.context = codegen::ops::junction_context(f.b, f.library, f.model,
        "ores.refdata.calendar_date", codegen::types::entity_registry{},
        refdata_site).context;
    const auto j = f.at("junction");
    CHECK(message_names(f, j) == strings{"calendar_date_key",
        "calendar_date_lookup", "calendar_dates_filter",
        "list_calendar_dates_request", "list_calendar_dates_response",
        "get_calendar_date_request", "get_calendar_date_response",
        "get_many_calendar_dates_request",
        "get_many_calendar_dates_response",
        "list_by_calendar_code_calendar_dates_request",
        "list_by_calendar_code_calendar_dates_response"});
    CHECK(f.json(j, "events_channel") == "<absent>");
    CHECK(f.json(j, "has_date_left_or_right_or_column") == "true");
    CHECK(f.json(j, "repository_uses_chrono") == "true");
    CHECK(f.json(j, "mapper_uses_datetime") == "true");
    CHECK(f.json(j, "service_uses_cstdint") == "false");
    const auto column = codegen::ops::values::items(f.tables, j,
        "columns").front();
    CHECK(f.json(column, "history_title") == "\"Is Business Day\"");
    CHECK(f.json(column, "history_value") == "\"(v.link.is_business_day ? "
        "std::string(\\\"true\\\") : std::string(\\\"false\\\"))\"");
    CHECK(f.json(column, "is_optional_uuid") == "false");
}

/*
 * The surface shape is taken before the repository section is copied
 * onto the junction, so it keeps the plural words the junction stated
 * (oracle: image_tag's shell recipe context, "tags" under domain_entity
 * and "image tags" under junction).
 */
TEST_CASE("a junction's surface shape keeps the names it stated before "
    "its repository section", "[codegen][model_contexts][junction]") {
    context_fixture f(R"j({"junction": {"name": "image_tags",
        "name_singular": "image_tag", "component": "assets",
        "entity_plural_words": "tags",
        "left": {"column": "image_id", "type": "uuid", "list_by": true},
        "right": {"column": "tag_id", "type": "uuid"},
        "repository": {"entity_plural_words": "image tags"}}})j");
    const auto [context, surface] = codegen::ops::junction_context(f.b,
        f.library, f.model, "ores.assets.image_tag",
        codegen::types::entity_registry{}, refdata_site);
    f.context = context;
    CHECK(f.json(surface, "entity_plural_words") == "\"tags\"");
    CHECK(f.json(f.at("junction"), "entity_plural_words") ==
        "\"image tags\"");
}

/*
 * ores.analytics' configuration_document operation, a part-qualified
 * model that states its wire codec (oracle: its cpp_protocol.hpp
 * context).
 */
TEST_CASE("an operation gains its component names and codec facts",
    "[codegen][model_contexts][operation]") {
    context_fixture f(R"j({"operation": {"component": "analytics",
        "subcomponent": "api", "entity_singular": "configuration_document",
        "messages": [{"name": "pricing_engines_document", "fields": [
            {"name": "configs",
             "cpp_type": "std::vector<pricing_model_config>"}]}]}})j");
    codegen::types::entity_registry registry;
    registry.directories["projects/ores.analytics/modeling"].member_names =
        {"pricing_model_config"};
    f.context = codegen::ops::operation_context(f.b, f.library, f.model,
        "ores.analytics.configuration_document", registry,
        {"projects/ores.analytics/modeling/"
         "ores.analytics.configuration_document.org", true, true});
    const auto op = f.at("operation");
    CHECK(f.json(op, "component_include_upper") == "\"ANALYTICS_API\"");
    CHECK(f.json(op, "component_core_upper") == "\"ANALYTICS_CORE\"");
    CHECK(f.json(op, "component_service") == "\"analytics.service\"");
    CHECK(f.json(op, "entity_singular_upper") ==
        "\"CONFIGURATION_DOCUMENT\"");
    CHECK(f.json(op, "has_generated_wire_codec") == "true");
    CHECK(f.json(op, "message_codec_includes") ==
        f.canon(R"(["ores.analytics.api/domain/pricing_model_config_codec.hpp"])"));
    const auto m = codegen::ops::values::items(f.tables, op,
        "messages").front();
    CHECK(f.json(m, "struct_name") == "\"pricing_engines_document\"");
}
