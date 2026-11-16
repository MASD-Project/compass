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
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <unordered_map>
#include "compass.codegen/ops/values.hpp"
#include "entity_text.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::model_tables;
using compass::model::types::value_id;

namespace {

/*
 * The member type a domain-grouped column is reached through, by column
 * name: the field group's declaration, which can differ from the column's
 * own cpp_type.
 */
using domain_types = std::unordered_map<std::string, std::string>;

domain_types domain_types_of(const model_tables& t, value_id entity) {
    domain_types out;
    for (const auto f : values::items(t, entity, "domain_group_fields")) {
        const auto name = values::str(t, f, "name");
        const auto dot = name.find('.');
        out[std::string(name.substr(dot + 1))] =
            std::string(values::strip(values::str(t, f, "cpp_type")));
    }
    return out;
}

/*
 * The column loop's derived values, before they are written: the type and
 * representation flags of each layer, the render flags of the domain
 * member's type, and the defaults the loop fills in.
 */
struct column_flags {
    bool is_image_id = false;
    bool is_int = false;
    bool is_date = false;
    bool is_optional_date = false;
    bool is_required_date = false;
    bool is_decimal = false;
    bool is_optional_decimal = false;
    bool is_required_decimal = false;
    bool is_already_optional = false;
    bool is_uuid = false;
    bool is_optional_uuid = false;
    bool is_optional_timestamp = false;
    bool is_required_timestamp = false;
    bool is_required_inet = false;
    bool is_base64 = false;
    bool is_enum = false;
    bool is_value_type = false;
    bool is_nullable_string = false;
    bool is_nullable_numeric = false;
    bool is_simple = false;
    bool is_text_stored = false;
    bool is_plain_string = false;
    bool requires_unique_suffix = false;
    std::string render_cpp_type;
    bool render_is_timestamp = false;
    bool render_is_optional_timestamp = false;
    bool render_is_enum = false;
    std::string render_label;
    std::optional<std::string> default_value;
    std::optional<std::string> generator_expr;
    bool sql_only = false;
    bool no_wire = false;
};

/*
 * The render_* flags that test the domain member's type for equality, in
 * the order the Python sets them around the two timestamp flags.
 */
struct render_type {
    std::string_view flag;
    std::string_view cpp_type;
};

constexpr render_type render_types_before_timestamp[] = {
    {"render_is_string", "std::string"},
    {"render_is_optional_string", "std::optional<std::string>"},
    {"render_is_bool", "bool"},
    {"render_is_optional_bool", "std::optional<bool>"},
    {"render_is_int", "int"},
    {"render_is_optional_int", "std::optional<int>"},
    {"render_is_double", "double"},
    {"render_is_optional_double", "std::optional<double>"},
    {"render_is_uuid", "boost::uuids::uuid"},
    {"render_is_optional_uuid", "std::optional<boost::uuids::uuid>"},
};

constexpr render_type render_types_after_timestamp[] = {
    {"render_is_date", "std::chrono::year_month_day"},
    {"render_is_optional_date", "std::optional<std::chrono::year_month_day>"},
    {"render_is_decimal", "ores::utility::decimal::decimal"},
    {"render_is_optional_decimal",
        "std::optional<ores::utility::decimal::decimal>"},
};

bool is_one_of(std::string_view s, std::initializer_list<std::string_view> l) {
    return std::ranges::find(l, s) != l.end();
}

column_flags derive(const model_tables& t, value_id col, bool plain,
    const domain_types& domain_type_of, bool entity_has_image_id) {
    column_flags f;
    const auto name_value = values::member(t, col, "name");
    const auto name = values::str(t, name_value);
    const auto type = values::str(t, col, "type");
    const auto sql_type = text::lower(values::strip(
        values::py_str(t, values::member(t, col, "type"))));
    const std::string cpp_type(values::str(t, col, "cpp_type"));
    const bool nullable = values::truthy(t, col, "nullable");

    f.is_image_id = name == "image_id" && entity_has_image_id;
    f.is_int = type == "integer" || cpp_type == "int";
    const bool is_uuid_type = type == "uuid" ||
        cpp_type.contains("boost::uuids::uuid");
    const bool is_timestamp_type = is_one_of(type,
        {"timestamp", "timestamptz", "timestamp with time zone"});
    const bool is_enum_type = values::truthy(t, col, "is_enum");
    const bool is_inet_type = cpp_type.contains("boost::asio::ip::address");

    std::string domain_type(values::strip(cpp_type));
    if (values::is(t, name_value, compass::model::types::value_kind::string)) {
        if (const auto i = domain_type_of.find(std::string(name));
            i != domain_type_of.end())
            domain_type = i->second;
    }
    const bool is_date_domain = is_one_of(domain_type,
        {"std::chrono::year_month_day",
            "std::optional<std::chrono::year_month_day>"});
    f.is_date = plain && type == "date" && is_date_domain;
    f.is_optional_date = f.is_date && nullable;
    f.is_required_date = f.is_date && !nullable;

    const bool is_numeric_sql_type = sql_type.starts_with("numeric");
    const bool is_decimal_domain = is_one_of(domain_type,
        {"ores::utility::decimal::decimal",
            "std::optional<ores::utility::decimal::decimal>"});
    f.is_decimal = plain && is_numeric_sql_type && is_decimal_domain;
    f.is_optional_decimal = f.is_decimal && nullable;
    f.is_required_decimal = f.is_decimal && !nullable;

    const bool already_optional = cpp_type.starts_with("std::optional<") &&
        !is_uuid_type && !is_timestamp_type && !f.is_date && !f.is_decimal;
    f.is_already_optional = already_optional;
    f.is_uuid = is_uuid_type && !nullable;
    f.is_optional_uuid = is_uuid_type && nullable;
    f.is_optional_timestamp = is_timestamp_type && nullable;
    f.is_required_timestamp = is_timestamp_type && !nullable;
    f.is_required_inet = is_inet_type && !nullable;
    f.is_base64 = values::is_true(t, col, "base64") && !nullable;
    f.is_enum = is_enum_type && !nullable;
    f.is_value_type = values::truthy(t, col, "is_value_type") && !nullable &&
        !is_enum_type && !is_uuid_type && !is_timestamp_type &&
        !already_optional;
    const bool plain_nullable = nullable && !is_uuid_type &&
        !is_timestamp_type && !is_enum_type && !already_optional &&
        !f.is_decimal;
    f.is_nullable_string = plain_nullable && !f.is_date &&
        cpp_type == "std::string";
    f.is_nullable_numeric = plain_nullable && is_one_of(cpp_type,
        {"int", "std::int64_t", "std::uint64_t", "double", "float", "bool"});
    f.is_simple = !nullable && !is_uuid_type && !is_timestamp_type &&
        !is_inet_type && !is_enum_type && !already_optional && !f.is_date &&
        !f.is_decimal && !f.is_base64 && !f.is_value_type;
    f.is_text_stored = is_one_of(sql_type, {"jsonb", "inet"}) ||
        (sql_type == "date" && !is_date_domain) ||
        (is_numeric_sql_type && !is_decimal_domain);

    const bool has_generator_expr = values::truthy(t, col, "generator_expr");
    f.is_plain_string = f.is_simple && cpp_type == "std::string" &&
        !has_generator_expr;
    f.requires_unique_suffix = values::is_true(t, col, "unique") &&
        cpp_type == "std::string" &&
        !values::truthy(t, col, "no_generator_suffix");

    f.render_is_timestamp = domain_type.contains("time_point") &&
        !domain_type.starts_with("std::optional<");
    f.render_is_optional_timestamp = domain_type.starts_with("std::optional<") &&
        domain_type.contains("time_point");
    f.render_cpp_type = std::move(domain_type);
    f.render_is_enum = is_enum_type;
    f.render_label = text::render_label(name);

    if (!values::truthy(t, col, "default_value") && f.is_simple) {
        if (cpp_type == "bool")
            f.default_value = "false";
        else if (cpp_type == "int")
            f.default_value = "0";
    }
    if (!has_generator_expr && is_one_of(text::lower(values::py_str(t,
            values::member(t, col, "type"))), {"json", "jsonb"}))
        f.generator_expr = "std::string(\"{}\")";
    f.sql_only = values::truthy(t, col, "sql_only");
    f.no_wire = values::truthy(t, col, "no_wire");
    return f;
}

void write(builder& b, value_id col, const column_flags& f,
    value_id iter_var) {
    b.set(col, "is_image_id", f.is_image_id);
    b.set(col, "is_int", f.is_int);
    b.set(col, "is_date", f.is_date);
    b.set(col, "is_optional_date", f.is_optional_date);
    b.set(col, "is_required_date", f.is_required_date);
    b.set(col, "is_decimal", f.is_decimal);
    b.set(col, "is_optional_decimal", f.is_optional_decimal);
    b.set(col, "is_required_decimal", f.is_required_decimal);
    b.set(col, "is_already_optional", f.is_already_optional);
    b.set(col, "is_uuid", f.is_uuid);
    b.set(col, "is_optional_uuid", f.is_optional_uuid);
    b.set(col, "is_optional_timestamp", f.is_optional_timestamp);
    b.set(col, "is_required_timestamp", f.is_required_timestamp);
    b.set(col, "is_required_inet", f.is_required_inet);
    b.set(col, "is_base64", f.is_base64);
    b.set(col, "is_enum", f.is_enum);
    b.set(col, "is_value_type", f.is_value_type);
    b.set(col, "is_nullable_string", f.is_nullable_string);
    b.set(col, "is_nullable_numeric", f.is_nullable_numeric);
    b.set(col, "is_simple", f.is_simple);
    b.set(col, "is_text_stored", f.is_text_stored);
    b.set(col, "is_plain_string", f.is_plain_string);
    b.set(col, "requires_unique_suffix", f.requires_unique_suffix);
    b.set(col, "render_cpp_type", f.render_cpp_type);
    for (const auto& r : render_types_before_timestamp)
        b.set(col, r.flag, f.render_cpp_type == r.cpp_type);
    b.set(col, "render_is_timestamp", f.render_is_timestamp);
    b.set(col, "render_is_optional_timestamp", f.render_is_optional_timestamp);
    for (const auto& r : render_types_after_timestamp)
        b.set(col, r.flag, f.render_cpp_type == r.cpp_type);
    b.set(col, "render_is_enum", f.render_is_enum);
    b.set(col, "render_label", f.render_label);
    if (f.default_value)
        b.set(col, "default_value", *f.default_value);
    if (iter_var.is_none())
        b.set(col, "iter_var", "e");
    else
        b.set_copy(col, "iter_var", iter_var);
    if (f.generator_expr)
        b.set(col, "generator_expr", *f.generator_expr);
    b.set(col, "sql_only", f.sql_only);
    if (!f.sql_only)
        b.set(col, "no_wire", f.no_wire);
}

bool any_column(const model_tables& t, value_id entity, std::string_view flag) {
    return std::ranges::any_of(values::items(t, entity, "columns"),
        [&](value_id c) { return values::truthy(t, c, flag); });
}

}

void derive_column_flags(builder& b, value_id entity) {
    const auto& t = b.tables();
    if (values::member(t, entity, "columns").is_none())
        return;
    const auto domain_type_of = domain_types_of(t, entity);
    const bool has_image_id = values::truthy(t, entity, "has_image_id");
    const auto iter_var = values::member(t,
        values::member(t, entity, "cpp"), "iterator_var");
    const auto apply = [&](value_id col, bool plain) {
        write(b, col, derive(b.tables(), col, plain, domain_type_of,
            has_image_id), iter_var);
    };
    for (const auto c : values::items(t, entity, "columns"))
        apply(c, true);
    for (const auto c : values::items(t, entity, "natural_keys"))
        apply(c, false);
    for (const auto c : values::items(t,
             values::member(t, entity, "primary_key"), "columns"))
        apply(c, false);
}

void derive_column_aggregates(builder& b, value_id entity) {
    const auto& t = b.tables();
    if (values::member(t, entity, "columns").is_none())
        return;
    b.set(entity, "has_enum_columns", any_column(t, entity, "is_enum"));
    b.set(entity, "has_no_wire_columns", any_column(t, entity, "no_wire"));
    std::int64_t wire_left_out = 0;
    for (const auto c : values::items(t, entity, "columns")) {
        if (values::truthy(t, c, "no_wire") &&
            !values::truthy(t, c, "sql_only") &&
            !values::truthy(t, c, "is_identity_group_column"))
            ++wire_left_out;
    }
    b.set_integer(entity, "wire_left_out", wire_left_out);
    b.set(entity, "has_inet_columns", any_column(t, entity,
        "is_required_inet"));
    b.set(entity, "has_base64_columns", any_column(t, entity, "is_base64"));
    b.set(entity, "has_plain_date_columns", any_column(t, entity, "is_date"));
    b.set(entity, "has_decimal_columns", any_column(t, entity, "is_decimal"));
}

}
