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
#include <cctype>
#include <initializer_list>
#include <set>
#include <string>
#include <vector>
#include "compass.codegen/ops/values.hpp"
#include "entity_text.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::model_tables;
using compass::model::types::value_id;

namespace {

using keys = std::initializer_list<std::string_view>;

bool any_of_keys(const model_tables& t, value_id object, keys ks) {
    return std::ranges::any_of(ks,
        [&](std::string_view k) { return values::truthy(t, object, k); });
}

/*
 * The columns the wire carries: neither sql_only nor no_wire.
 */
std::vector<value_id> wire_columns(const model_tables& t, value_id entity) {
    std::vector<value_id> out;
    for (const auto c : values::items(t, entity, "columns")) {
        if (!values::truthy(t, c, "sql_only") &&
            !values::truthy(t, c, "no_wire"))
            out.push_back(c);
    }
    return out;
}

/*
 * Python's `a or b`: @p flag when true, else the member @p key of
 * @p object as it is, else false.
 */
void set_or_member(builder& b, value_id entity, std::string_view target,
    bool flag, value_id object, std::string_view key) {
    const auto v = values::member(b.tables(), object, key);
    if (!flag && !v.is_none())
        b.set_copy(entity, target, v);
    else
        b.set(entity, target, flag);
}

bool is_word(std::string_view s) {
    return !s.empty() && std::ranges::all_of(s, [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
    });
}

/*
 * The domain header of a type ores::<c>::domain::<name>: ores.<c>.api/
 * domain/<name>.hpp; empty for any other type.
 */
std::string enum_header(std::string_view type) {
    constexpr std::string_view prefix = "ores::";
    constexpr std::string_view middle = "::domain::";
    if (!type.starts_with(prefix))
        return {};
    const auto rest = type.substr(prefix.size());
    const auto at = rest.find(middle);
    if (at == std::string_view::npos)
        return {};
    const auto component = rest.substr(0, at);
    const auto name = rest.substr(at + middle.size());
    if (!is_word(component) || !is_word(name))
        return {};
    return "ores." + std::string(component) + ".api/domain/" +
        std::string(name) + ".hpp";
}

bool uses_optional(const model_tables& t, value_id c) {
    const auto is = [&](std::string_view k) { return values::truthy(t, c, k); };
    return is("is_optional_uuid") || is("is_optional_timestamp") ||
        (is("is_date") && is("is_optional_date")) ||
        (is("is_date") && is("is_required_date") &&
            is("render_is_optional_date")) ||
        (is("is_decimal") && is("is_optional_decimal")) ||
        (!is("render_is_enum") && is("is_nullable_string") &&
            !is("render_is_optional_string")) ||
        is("is_nullable_numeric");
}

bool uses_datetime(const model_tables& t, value_id c) {
    const auto is = [&](std::string_view k) { return values::truthy(t, c, k); };
    return is("is_date") || is("is_required_timestamp") ||
        (is("is_optional_timestamp") &&
            (is("render_is_optional_timestamp") || is("render_is_timestamp")));
}

}

void derive_generator_facet_names(builder& b, value_id entity) {
    if (values::member(b.tables(), entity, "generator_facet_name").is_none())
        b.set(entity, "generator_facet_name", "generators");
    b.set(entity, "generator_facet_name_upper", text::upper(
        values::str(b.tables(), entity, "generator_facet_name")));
}

void derive_uuid_column_flags(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto columns = values::items(t, entity, "columns");
    const bool uuid_keys = std::ranges::any_of(values::items(t, entity, "natural_keys"),
        [&](value_id k) { return values::truthy(t, k, "is_uuid"); });
    const bool uuid_columns = std::ranges::any_of(columns, [&](value_id c) {
        return any_of_keys(t, c, {"is_uuid", "is_optional_uuid"});
    });
    const bool render_uuid_columns = std::ranges::any_of(columns, [&](value_id c) {
        return any_of_keys(t, c, {"render_is_uuid", "render_is_optional_uuid"});
    });
    const auto pk = values::member(t, entity, "primary_key");
    set_or_member(b, entity, "has_uuid_columns", uuid_columns || uuid_keys,
        pk, "is_uuid");
    set_or_member(b, entity, "has_render_uuid_columns",
        render_uuid_columns || uuid_keys, pk, "is_uuid");
}

void derive_render_kind_flags(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto columns = wire_columns(t, entity);
    const auto natural_keys = values::items(t, entity, "natural_keys");
    const auto is = [&](value_id v, std::string_view k) {
        return values::truthy(t, v, k);
    };
    b.set(entity, "has_render_datetime_columns",
        values::truthy(t, entity, "has_audit_columns") ||
        std::ranges::any_of(natural_keys, [&](value_id k) {
            return is(k, "is_timestamp") && !is(k, "is_uuid") &&
                !is(k, "is_date");
        }) ||
        std::ranges::any_of(columns, [&](value_id c) {
            return any_of_keys(t, c, {"render_is_timestamp",
                "render_is_optional_timestamp", "render_is_date",
                "render_is_optional_date"});
        }));
    b.set(entity, "has_render_string_columns",
        std::ranges::any_of(columns, [&](value_id c) {
            return any_of_keys(t, c, {"render_is_optional_uuid",
                "render_is_optional_timestamp", "render_is_optional_date",
                "render_is_optional_decimal", "render_is_optional_string",
                "render_is_optional_bool", "render_is_optional_int",
                "render_is_optional_double", "render_is_int",
                "render_is_double"});
        }) ||
        std::ranges::any_of(natural_keys, [&](value_id k) {
            return (is(k, "is_uuid") && is(k, "render_is_optional_uuid")) ||
                (!is(k, "is_uuid") && !is(k, "is_date") &&
                    !is(k, "is_timestamp") && is(k, "is_int"));
        }));
}

void derive_mapper_include_gates(builder& b, value_id entity) {
    const auto& t = b.tables();
    std::vector<value_id> columns;
    for (const auto c : values::items(t, entity, "columns")) {
        if (!values::truthy(t, c, "sql_only"))
            columns.push_back(c);
    }
    const auto pk_columns = values::items(t,
        values::member(t, entity, "primary_key"), "columns");
    const auto natural_keys = values::items(t, entity, "natural_keys");
    const auto is = [&](value_id v, std::string_view k) {
        return values::truthy(t, v, k);
    };

    std::set<std::string> headers;
    bool enum_wrapped = false;
    for (const auto c : columns) {
        if (is(c, "is_enum"))
            headers.insert(enum_header(values::str(t, c, "cpp_type")));
    }
    for (const auto c : columns) {
        if (is(c, "render_is_enum") && !is(c, "is_enum")) {
            enum_wrapped = true;
            headers.insert(enum_header(values::str(t, c, "render_cpp_type")));
        }
    }
    headers.erase(std::string{});

    const auto includes = b.list(entity, "mapper_enum_includes");
    for (const auto& h : headers)
        b.set(b.append_object(includes), "path", h);

    const auto pk_timestamp = std::ranges::any_of(pk_columns,
        [&](value_id c) { return is(c, "is_timestamp"); });
    const auto key_timestamp = std::ranges::any_of(natural_keys,
        [&](value_id k) { return is(k, "is_timestamp"); });
    b.set(entity, "mapper_uses_optional", std::ranges::any_of(columns,
        [&](value_id c) { return uses_optional(t, c); }) || enum_wrapped);
    b.set(entity, "mapper_uses_string",
        std::ranges::any_of(pk_columns, [&](value_id c) { return is(c, "is_int"); }) ||
        std::ranges::any_of(columns, [&](value_id c) {
            return is(c, "is_date") && is(c, "is_required_date") &&
                is(c, "render_is_optional_date");
        }) || enum_wrapped);
    b.set(entity, "mapper_uses_datetime", std::ranges::any_of(columns,
        [&](value_id c) { return uses_datetime(t, c); }) ||
        pk_timestamp || key_timestamp);
    b.set(entity, "mapper_uses_string_view", pk_timestamp || key_timestamp ||
        std::ranges::any_of(columns,
            [&](value_id c) { return is(c, "is_required_timestamp"); }));
}

void derive_uuid_include_gates(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto pk = values::member(t, entity, "primary_key");
    b.set(entity, "repository_uses_uuid_io",
        std::ranges::any_of(values::items(t, pk, "columns"),
            [&](value_id c) { return values::truthy(t, c, "is_uuid"); }) ||
        values::truthy(t, entity, "has_parent_id") ||
        values::truthy(t, pk, "party_sql_key"));

    const auto find_by_code = values::member(t, entity, "service_find_by_code");
    for (const auto operand : {values::member(t, entity, "has_parent_id"),
             values::member(t, entity, "service_find_by_uuid"),
             values::member(t, find_by_code, "parent_column")}) {
        if (values::truthy(t, operand)) {
            b.set_copy(entity, "has_uuid_include", operand);
            return;
        }
    }
    b.set(entity, "has_uuid_include",
        std::ranges::any_of(values::items(t, entity, "foreign_keys"),
            [&](value_id fk) { return values::truthy(t, fk, "list_by_uuid"); }));
}

}
