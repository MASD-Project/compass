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

model::types::value_id presentation(codegen::tests::entity_fixture& f) {
    return member(f.tables, f.entity, "presentation");
}

void derive_all(codegen::tests::entity_fixture& f) {
    codegen::ops::derive_presentation_columns(f.b, f.entity);
    codegen::ops::derive_presentation_detail_defaults(f.b, f.entity);
    codegen::ops::derive_detail_fields(f.b, f.entity);
    codegen::ops::derive_presentation_flags(f.b, f.entity);
    codegen::ops::project_import_preview_columns(f.b, f.entity);
}

}

/*
 * ORE Studio's calendar_event hides Description and Source, both optional
 * strings in the domain; the oracle marks Source, the last hidden column,
 * and ModifiedBy, the last column.
 */
TEST_CASE("list columns take optional flags, styles and last markers",
    "[codegen][entity][presentation]") {
    codegen::tests::entity_fixture f(R"j({
        "component": "refdata", "entity_singular": "calendar_event",
        "columns": [{"name": "name", "cpp_type": "std::string"},
            {"name": "description", "cpp_type": "std::optional<std::string>"},
            {"name": "source", "cpp_type": "std::optional<std::string>"}],
        "presentation": {"columns": [
            {"enum_name": "Name", "field": "name", "is_string": true},
            {"enum_name": "Description", "field": "description",
             "hidden_by_default": true, "is_string": true,
             "import_preview": true},
            {"enum_name": "Source", "field": "source",
             "hidden_by_default": true, "is_string": true},
            {"enum_name": "Version", "field": "version", "is_int": true},
            {"enum_name": "ModifiedBy", "field": "modified_by",
             "is_string": true}],
            "import_preview_columns": [{"enum_name": "Description"}]}})j");
    derive_all(f);
    const auto p = presentation(f);
    const auto columns = items(f.tables, member(f.tables, p, "columns"));
    CHECK(f.json(p, "domain_class") == R"("refdata::domain::calendar_event")");
    CHECK(f.json(columns[1], "is_optional_string") == "true");
    CHECK(f.json(columns[1], "is_string") == "false");
    CHECK(f.json(columns[1], "last") == "<absent>");
    CHECK(f.json(columns[2], "last") == "true");
    CHECK(f.json(columns[4], "last") == "true");
    CHECK(f.json(columns[0], "is_optional_string") == "<absent>");
    CHECK(f.json(columns[3], "column_style") == R"("cs::mono_center")");
    CHECK(f.json(columns[4], "column_style") == R"("cs::text_left")");
    CHECK(f.json(columns[3], "column_index") == "3");
    CHECK(f.json(p, "has_badge_columns") == "false");
    CHECK(f.json(p, "has_self_colour_columns") == "false");
    const auto preview = items(f.tables,
        member(f.tables, p, "import_preview_columns"));
    REQUIRE(preview.size() == 1);
    CHECK(f.json(preview[0], "is_optional_string") == "true");
    CHECK(f.json(preview[0], "column_index") == "1");
}

/*
 * ORE Studio's currency_pair: icon columns render icon_text_left, the
 * badge column badge_centered.
 */
TEST_CASE("icon and badge columns take their styles",
    "[codegen][entity][presentation]") {
    codegen::tests::entity_fixture f(R"j({"presentation": {
        "icon_columns": [{"column": "PairCode"}, {"column": "BaseCurrency"}],
        "columns": [
            {"enum_name": "PairCode", "field": "pair_code", "is_string": true},
            {"enum_name": "BaseCurrency", "field": "base_currency",
             "is_string": true},
            {"enum_name": "Classification", "field": "classification",
             "is_badge": true, "is_string": true},
            {"enum_name": "Version", "field": "version", "is_int": true,
             "column_style": "cs::text_left"}]}})j");
    derive_all(f);
    const auto p = presentation(f);
    const auto columns = items(f.tables, member(f.tables, p, "columns"));
    CHECK(f.json(columns[0], "column_style") == R"("cs::icon_text_left")");
    CHECK(f.json(columns[1], "column_style") == R"("cs::icon_text_left")");
    CHECK(f.json(columns[2], "column_style") == R"("cs::badge_centered")");
    CHECK(f.json(columns[3], "column_style") == R"("cs::text_left")");
    CHECK(f.json(p, "has_badge_columns") == "true");
}

/*
 * ORE Studio's currency_group states no detail fields; its name is a
 * natural key, not a column, and still gets its row.
 */
TEST_CASE("an entity without detail fields gets the lookup form",
    "[codegen][entity][presentation]") {
    codegen::tests::entity_fixture f(R"j({
        "entity_singular_words": "currency group",
        "primary_key": {"column": "code", "cpp_type": "std::string"},
        "columns": [{"name": "description", "cpp_type": "std::string"},
            {"name": "display_order", "cpp_type": "int"}],
        "natural_keys": [{"column": "name", "cpp_type": "std::string"}],
        "presentation": {}})j");
    derive_all(f);
    const auto p = presentation(f);
    const auto fields = items(f.tables, member(f.tables, p, "detail_fields"));
    REQUIRE(fields.size() == 3);
    CHECK(f.json(fields[0], "placeholder") ==
        R"("Enter currency group code")");
    CHECK(f.json(fields[0], "label") == R"("Code")");
    CHECK(f.json(fields[0], "is_locked_after_create") == "true");
    CHECK(f.json(fields[0], "value_widget") == R"("codeValue")");
    CHECK(f.json(fields[0], "_is_first") == "true");
    CHECK(f.json(fields[1], "placeholder") == R"("Enter display name")");
    CHECK(f.json(fields[1], "is_key") == "false");
    CHECK(f.json(fields[1], "is_self_referencing_combo") == "null");
    CHECK(f.json(fields[2], "is_text_edit") == "true");
    CHECK(f.json(fields[2], "is_required") == "false");
    CHECK(f.json(fields[2], "_is_last") == "true");
    CHECK(f.json(fields[2], "_row_index") == "2");
    CHECK(f.json(fields[2], "label_widget") == R"("labelDescription")");
    CHECK(f.json(p, "delete_request_id_field") == R"("codes")");
    CHECK(f.json(p, "item_var") == R"("item")");
    CHECK(f.json(p, "has_pagination") == "false");
    CHECK(f.json(p, "has_combo_badge_source") == "false");
    CHECK(f.json(p, "has_parent_relationship") == "false");
    CHECK(f.json(p, "has_badge_columns") == "<absent>");
}

/*
 * ORE Studio's party: parent_party_id fetches parties, so the combo refers
 * to the entity itself; the model's own combo_setter_pascal stays.
 */
TEST_CASE("a combo over the entity's own type is self-referencing",
    "[codegen][entity][presentation]") {
    codegen::tests::entity_fixture f(R"j({
        "component": "refdata", "entity_singular": "party",
        "primary_key": {"column": "id", "cpp_type": "boost::uuids::uuid"},
        "columns": [{"name": "parent_party_id",
            "cpp_type": "std::optional<boost::uuids::uuid>"}],
        "presentation": {"has_uuid_primary_key": true, "detail_fields": [
            {"field": "id", "type": "line_edit", "is_key": true},
            {"field": "parent_party_id", "type": "dynamic_combo",
             "widget": "parentPartyCombo", "is_required": true,
             "combo_domain_type": "refdata::domain::party",
             "combo_fetch_fn": "fetch_parties",
             "combo_setter_pascal": "ParentPartyCombo"}]}})j");
    derive_all(f);
    const auto p = presentation(f);
    const auto fields = items(f.tables, member(f.tables, p, "detail_fields"));
    CHECK(f.json(fields[0], "is_uuid") == "true");
    CHECK(f.json(fields[1], "is_self_referencing_combo") == "true");
    CHECK(f.json(fields[1], "is_optional_uuid") == "true");
    CHECK(f.json(fields[1], "is_uuid") == "false");
    CHECK(f.json(fields[1], "combo_setter_pascal") ==
        R"("ParentPartyCombo")");
    CHECK(f.json(fields[1], "combo_items_member") == R"("parentPartyId")");
    CHECK(f.json(fields[1], "value_widget") == R"("parentPartyValue")");
    CHECK(f.json(fields[1], "field_pascal") == R"("ParentPartyId")");
    CHECK(f.json(p, "delete_request_id_field") == R"("ids")");
}

/*
 * ORE Studio's currency_pair: currency flagged combos reuse the shared
 * helper; the classification badge combo becomes a badge combo box.
 */
TEST_CASE("flagged and badge combos take their widget classes",
    "[codegen][entity][presentation]") {
    codegen::tests::entity_fixture f(R"j({"presentation": {"detail_fields": [
        {"field": "base_currency", "type": "flagged_combo", "immutable": true,
         "widget": "baseCurrencyCombo",
         "flag_source": "currency"},
        {"field": "classification", "type": "static_combo",
         "badge_key": "currency_pair_classification"}]}})j");
    derive_all(f);
    const auto p = presentation(f);
    const auto fields = items(f.tables, member(f.tables, p, "detail_fields"));
    CHECK(f.json(fields[0], "combo_helper") == R"("setup_currency_combo")");
    CHECK(f.json(fields[0], "combo_widget_class") ==
        R"("ores::presentation::OreCurrencyComboBox")");
    CHECK(f.json(fields[0], "flag_icon_size_expr") ==
        R"j("single_flag_icon_size()")j");
    CHECK(f.json(fields[0], "flag_source_pascal") == R"("Currency")");
    CHECK(f.json(fields[0], "combo_allow_blank") == "false");
    CHECK(f.json(fields[0], "is_locked_after_create") == "true");
    CHECK(f.json(fields[0], "value_widget") == R"("baseCurrencyValue")");
    CHECK(f.json(fields[1], "combo_widget_class") ==
        R"("ores::presentation::OreBadgeComboBox")");
    CHECK(f.json(fields[1], "combo_helper") == "<absent>");
    CHECK(f.json(p, "has_combo_badge_source") == "true");
}
