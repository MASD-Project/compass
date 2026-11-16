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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "compass.codegen/ops/values.hpp"
#include "entity_text.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::model_tables;
using compass::model::types::value_id;
using compass::model::types::value_kind;

namespace {

using cpp_types = std::unordered_map<std::string, std::string>;
using optional_names = std::vector<std::optional<std::string>>;

value_id presentation_of(const model_tables& t, value_id entity) {
    const auto p = values::member(t, entity, "presentation");
    return values::is(t, p, value_kind::object) ? p : value_id::none();
}

/*
 * dict.get(key) of a string member: None when absent or not a string.
 */
std::optional<std::string> optional_str(const model_tables& t,
    value_id object, std::string_view key) {
    const auto v = values::member(t, object, key);
    if (!values::is(t, v, value_kind::string))
        return std::nullopt;
    return std::string(values::str(t, v));
}

/*
 * dict.get(key, fallback) of a string member.
 */
std::string str_or(const model_tables& t, value_id object,
    std::string_view key, std::string_view fallback) {
    const auto v = values::member(t, object, key);
    return v.is_none() ? std::string(fallback)
                       : std::string(values::str(t, v));
}

void set_default(builder& b, value_id object, std::string_view key,
    std::string_view value) {
    if (values::member(b.tables(), object, key).is_none())
        b.set(object, key, value);
}

void set_default(builder& b, value_id object, std::string_view key,
    const char* value) {
    set_default(b, object, key, std::string_view(value));
}

void set_default(builder& b, value_id object, std::string_view key,
    bool value) {
    if (values::member(b.tables(), object, key).is_none())
        b.set(object, key, value);
}

std::string replace_all(std::string s, std::string_view from,
    std::string_view to) {
    for (auto at = s.find(from); at != std::string::npos;
         at = s.find(from, at + to.size()))
        s.replace(at, from.size(), to);
    return s;
}

/*
 * The domain columns' cpp_type by name, the last column of a name winning
 * as in the Python's dict comprehension.
 */
cpp_types column_cpp_types(const model_tables& t, value_id entity) {
    cpp_types out;
    for (const auto c : values::items(t, entity, "columns"))
        out[std::string(values::str(t, c, "name"))] =
            std::string(values::str(t, c, "cpp_type"));
    return out;
}

std::string cpp_type_of(const cpp_types& types, std::string_view name) {
    const auto i = types.find(std::string(name));
    return i == types.end() ? std::string{} : i->second;
}

/*
 * _mark_last_item on the columns hidden by default: those marked
 * hidden_by_default and Description, the first column of each enum_name.
 */
void mark_last_hidden_column(builder& b, value_id columns) {
    const auto& t = b.tables();
    optional_names seen;
    value_id last = value_id::none();
    for (const auto c : values::items(t, columns)) {
        auto name = optional_str(t, c, "enum_name");
        if (std::ranges::find(seen, name) != seen.end())
            continue;
        if (values::truthy(t, c, "hidden_by_default") ||
            name == "Description") {
            last = c;
            seen.push_back(std::move(name));
        }
    }
    if (values::is(t, last, value_kind::object))
        b.set(last, "last", true);
}

optional_names icon_column_names(const model_tables& t,
    value_id presentation) {
    optional_names out;
    for (const auto ic : values::items(t, presentation, "icon_columns"))
        out.push_back(optional_str(t, ic, "column"));
    if (values::truthy(t, presentation, "flag_icon_column"))
        out.push_back(optional_str(t, presentation, "flag_icon_column"));
    return out;
}

/*
 * An is_<kind> flag that becomes is_optional_<kind> over an optional
 * domain type.
 */
void flip_to_optional(builder& b, value_id column, std::string_view kind,
    bool optional) {
    const auto flag = "is_" + std::string(kind);
    if (!optional || !values::truthy(b.tables(), column, flag))
        return;
    b.set(column, "is_optional_" + std::string(kind), true);
    b.set(column, flag, false);
}

std::string_view default_column_style(const model_tables& t, value_id column,
    const optional_names& icon_names) {
    if (values::truthy(t, column, "is_badge") ||
        values::truthy(t, column, "self_colour"))
        return "cs::badge_centered";
    if (std::ranges::find(icon_names, optional_str(t, column, "enum_name")) !=
        icon_names.end())
        return "cs::icon_text_left";
    if (values::truthy(t, column, "is_int") ||
        values::truthy(t, column, "is_optional_int"))
        return "cs::mono_center";
    return "cs::text_left";
}

bool any_truthy(const model_tables& t, value_id list, std::string_view key) {
    return std::ranges::any_of(values::items(t, list),
        [&](value_id v) { return values::truthy(t, v, key); });
}

/*
 * The detail form's member types: the domain columns, then the natural
 * keys and the primary key for names the columns lack.
 */
cpp_types detail_cpp_types(const model_tables& t, value_id entity) {
    auto out = column_cpp_types(t, entity);
    for (const auto nk : values::items(t, entity, "natural_keys")) {
        if (values::truthy(t, nk, "column"))
            out.emplace(std::string(values::str(t, nk, "column")),
                std::string(values::str(t, nk, "cpp_type")));
    }
    const auto pk = values::member(t, entity, "primary_key");
    if (values::truthy(t, pk) && values::truthy(t, pk, "column"))
        out.emplace(std::string(values::str(t, pk, "column")),
            std::string(values::str(t, pk, "cpp_type")));
    return out;
}

void promote_badge_combo(builder& b, value_id field) {
    set_default(b, field, "combo_widget_class",
        "ores::presentation::OreBadgeComboBox");
    set_default(b, field, "combo_widget_extends", "QComboBox");
    set_default(b, field, "combo_widget_header",
        "ores.presentation/OreBadgeComboBox.hpp");
}

void derive_flagged_combo(builder& b, value_id field) {
    set_default(b, field, "combo_allow_blank", false);
    const auto source = str_or(b.tables(), field, "flag_source", "currency");
    b.set(field, "flag_source_pascal", text::snake_to_pascal(source));
    b.set(field, "flag_icon_size_expr", source == "currency_pair"
        ? "currency_pair_icon_size()" : "single_flag_icon_size()");
    if (source != "currency")
        return;
    set_default(b, field, "combo_helper", "setup_currency_combo");
    set_default(b, field, "combo_widget_class",
        "ores::presentation::OreCurrencyComboBox");
    set_default(b, field, "combo_widget_extends", "QComboBox");
    set_default(b, field, "combo_widget_header",
        "ores.presentation/OreCurrencyComboBox.hpp");
}

/*
 * is_self_referencing_combo: combo_domain_type and its equality with the
 * presentation's domain_class, Python's `and` keeping a falsy operand.
 */
void derive_self_referencing(builder& b, value_id field,
    value_id presentation) {
    const auto& t = b.tables();
    const auto type = values::member(t, field, "combo_domain_type");
    if (type.is_none()) {
        b.set_null(field, "is_self_referencing_combo");
        return;
    }
    if (!values::truthy(t, type)) {
        b.set_copy(field, "is_self_referencing_combo", type);
        return;
    }
    const auto domain_class = values::member(t, presentation, "domain_class");
    b.set(field, "is_self_referencing_combo",
        values::is(t, type, value_kind::string) &&
        values::is(t, domain_class, value_kind::string) &&
        values::str(t, type) == values::str(t, domain_class));
}

void derive_widget_flags(builder& b, value_id field, std::string_view type,
    std::string_view name) {
    const auto& t = b.tables();
    const bool static_combo = type == "static_combo";
    const bool dynamic_combo = type == "dynamic_combo";
    b.set(field, "is_line_edit", type == "line_edit");
    b.set(field, "is_text_edit",
        type == "text_edit" || type == "plain_text_edit");
    b.set(field, "is_static_combo", static_combo);
    b.set(field, "is_dynamic_combo", dynamic_combo);
    if ((static_combo || dynamic_combo) &&
        values::truthy(t, field, "badge_key"))
        promote_badge_combo(b, field);
    if (dynamic_combo && values::truthy(t, field, "combo_fetch_fn")) {
        const auto pascal = text::snake_to_pascal(name);
        set_default(b, field, "combo_setter_pascal", pascal);
        set_default(b, field, "combo_items_member",
            text::lower(pascal.substr(0, 1)) + pascal.substr(1));
    }
    b.set(field, "is_flagged_combo", type == "flagged_combo");
    if (type == "flagged_combo")
        derive_flagged_combo(b, field);
    b.set(field, "is_check_box", type == "check_box");
    b.set(field, "is_spin_box", type == "spin_box");
    b.set(field, "is_colour", type == "colour");
    if (type == "colour")
        set_default(b, field, "placeholder", "#rrggbb");
}

void derive_type_flags(builder& b, value_id field, std::string_view type,
    const std::string& cpp) {
    const auto& t = b.tables();
    const bool line = type == "line_edit";
    const bool spin_box = type == "spin_box";
    const bool text_or_combo = line || type == "text_edit" ||
        type == "plain_text_edit" || type == "flagged_combo" ||
        type == "static_combo" || type == "dynamic_combo";
    b.set(field, "is_optional_string",
        cpp.starts_with("std::optional<std::string>") && text_or_combo);
    const bool optional_uuid =
        cpp.find("std::optional<boost::uuids::uuid>") != std::string::npos;
    b.set(field, "is_optional_uuid", optional_uuid);
    b.set(field, "is_uuid",
        cpp.find("boost::uuids::uuid") != std::string::npos && !optional_uuid);
    const bool nullable_int =
        spin_box && cpp.starts_with("std::optional<int>");
    b.set(field, "is_tristate",
        type == "check_box" && cpp.starts_with("std::optional<bool>"));
    b.set(field, "is_nullable_int", nullable_int);
    b.set(field, "is_double", line && (cpp == "double" || cpp == "float"));
    b.set(field, "is_optional_double",
        line && cpp == "std::optional<double>");
    b.set(field, "is_date", line && cpp == "std::chrono::year_month_day");
    if (!spin_box)
        return;
    if (values::member(t, field, "spin_min").is_none())
        b.set_integer(field, "spin_min", nullable_int ? -1 : 0);
    if (values::member(t, field, "spin_max").is_none())
        b.set_integer(field, "spin_max", 9999);
}

/*
 * The position markers, the key and lock flags and the widget names.
 */
void derive_row_flags(builder& b, value_id field, std::string_view name,
    std::size_t index, std::size_t count) {
    const auto& t = b.tables();
    b.set(field, "_is_first", index == 0);
    b.set(field, "_is_last", index + 1 == count);
    b.set_integer(field, "_row_index", static_cast<std::int64_t>(index));
    if (!values::truthy(t, field, "is_key"))
        b.set(field, "is_key", false);
    if (!values::truthy(t, field, "is_required"))
        b.set(field, "is_required", false);
    const bool immutable = values::truthy(t, field, "immutable");
    b.set(field, "is_immutable", immutable);
    if (values::truthy(t, field, "is_key"))
        b.set_copy(field, "is_locked_after_create",
            values::member(t, field, "is_key"));
    else
        b.set(field, "is_locked_after_create", immutable);

    const auto widget = str_or(t, field, "widget", std::string(name) + "Edit");
    b.set(field, "value_widget",
        replace_all(replace_all(widget, "Edit", "Value"), "Combo", "Value"));
    const auto pascal = text::snake_to_pascal(name);
    b.set(field, "label_widget", "label" + pascal);
    b.set(field, "field_pascal", pascal);
}

/*
 * The default detail form: the key row, then name and description rows
 * when the entity has those members.
 */
void default_detail_fields(builder& b, value_id entity,
    value_id presentation) {
    const auto& t = b.tables();
    const auto key = str_or(t, presentation, "key_field", "code");
    std::vector<std::string> names;
    for (const auto c : values::items(t, entity, "columns"))
        names.emplace_back(values::str(t, c, "name"));
    for (const auto nk : values::items(t, entity, "natural_keys")) {
        if (values::truthy(t, nk, "column"))
            names.emplace_back(values::str(t, nk, "column"));
    }
    const auto has = [&](std::string_view n) {
        return std::ranges::find(names, n) != names.end();
    };
    const bool key_is_name = key == "name";
    const auto key_words = replace_all(key, "_", " ");
    const auto words = str_or(t, entity, "entity_singular_words", "item");

    const auto fields = b.list(presentation, "detail_fields");
    const auto k = b.append_object(fields);
    b.set(k, "field", key);
    b.set(k, "label", text::title(key_words));
    b.set(k, "widget", key_is_name ? "nameEdit" : "codeEdit");
    b.set(k, "type", "line_edit");
    b.set(k, "is_key", true);
    b.set(k, "is_required", true);
    b.set(k, "placeholder", "Enter " + words + " " + key_words);
    if (!key_is_name && has("name")) {
        const auto n = b.append_object(fields);
        b.set(n, "field", "name");
        b.set(n, "label", "Name");
        b.set(n, "widget", "nameEdit");
        b.set(n, "type", "line_edit");
        b.set(n, "is_required", true);
        b.set(n, "placeholder", "Enter display name");
    }
    if (has("description")) {
        const auto d = b.append_object(fields);
        b.set(d, "field", "description");
        b.set(d, "label", "Description");
        b.set(d, "widget", "descriptionEdit");
        b.set(d, "type", "text_edit");
        b.set(d, "placeholder", "Enter a description");
    }
}

}

void derive_presentation_columns(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto p = presentation_of(t, entity);
    if (p.is_none())
        return;
    const std::string singular(values::str(t, entity, "entity_singular"));
    const std::string component(values::str(t, entity, "component"));
    if (values::member(t, p, "domain_class").is_none() && !singular.empty() &&
        !component.empty())
        b.set(p, "domain_class", component + "::domain::" + singular);

    const auto columns = values::member(t, p, "columns");
    if (columns.is_none())
        return;
    mark_last_item(b, columns);
    mark_last_hidden_column(b, columns);
    const auto types = column_cpp_types(t, entity);
    const auto icon_names = icon_column_names(t, p);
    const auto list = values::items(t, columns);
    for (std::size_t i = 0; i < list.size(); ++i) {
        const auto c = list[i];
        const bool optional = cpp_type_of(types, values::str(t, c, "field"))
            .starts_with("std::optional<");
        flip_to_optional(b, c, "string", optional);
        flip_to_optional(b, c, "int", optional);
        flip_to_optional(b, c, "double", optional);
        if (values::member(t, c, "column_index").is_none())
            b.set_integer(c, "column_index", static_cast<std::int64_t>(i));
        if (values::member(t, c, "column_style").is_none())
            b.set(c, "column_style", default_column_style(t, c, icon_names));
    }

    const auto combo_badges = values::member(t, p, "has_combo_badge_source");
    if (any_truthy(t, columns, "is_badge"))
        b.set(p, "has_badge_columns", true);
    else if (!combo_badges.is_none())
        b.set_copy(p, "has_badge_columns", combo_badges);
    else
        b.set(p, "has_badge_columns", false);
    b.set(p, "has_self_colour_columns", any_truthy(t, columns, "self_colour"));
}

void derive_presentation_detail_defaults(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto p = presentation_of(t, entity);
    if (p.is_none())
        return;
    b.set(p, "has_parent_relationship",
        values::truthy(t, p, "has_explorer_api") &&
        values::truthy(t, p, "parent_entity_singular"));
    set_default(b, p, "item_var", "item");
    if (values::member(t, p, "detail_fields").is_none())
        default_detail_fields(b, entity, p);
}

void derive_detail_fields(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto p = presentation_of(t, entity);
    if (p.is_none())
        return;
    const auto types = detail_cpp_types(t, entity);
    const auto fields = values::items(t, p, "detail_fields");
    for (std::size_t i = 0; i < fields.size(); ++i) {
        const auto f = fields[i];
        const std::string type(values::str(t, f, "type"));
        const std::string name(values::str(t, f, "field"));
        derive_widget_flags(b, f, type, name);
        derive_type_flags(b, f, type, cpp_type_of(types, name));
        derive_self_referencing(b, f, p);
        derive_row_flags(b, f, name, i, fields.size());
    }
}

void derive_presentation_flags(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto p = presentation_of(t, entity);
    if (p.is_none())
        return;
    set_default(b, p, "has_pagination", false);
    set_default(b, p, "has_readonly_paginated_list", false);
    set_default(b, p, "has_parent_scoped_list", false);
    b.set(p, "has_combo_badge_source", std::ranges::any_of(
        values::items(t, p, "detail_fields"), [&](value_id f) {
            const auto type = values::str(t, f, "type");
            return (type == "static_combo" || type == "dynamic_combo") &&
                values::truthy(t, f, "badge_key");
        }));
    if (values::truthy(t, p, "has_uuid_primary_key")) {
        set_default(b, p, "delete_request_id_field", "ids");
        return;
    }
    const std::string pk_column(values::str(t,
        values::member(t, entity, "primary_key"), "column"));
    if (!pk_column.empty())
        set_default(b, p, "delete_request_id_field", pk_column + "s");
}

void project_import_preview_columns(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto p = presentation_of(t, entity);
    if (values::member(t, p, "import_preview_columns").is_none())
        return;
    const auto columns = values::items(t, p, "columns");
    const auto preview = b.list(p, "import_preview_columns");
    for (const auto c : columns) {
        if (values::truthy(t, c, "import_preview"))
            b.append_copy(preview, c);
    }
}

}
