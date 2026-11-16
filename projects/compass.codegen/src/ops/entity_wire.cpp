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
#include "entity_wire.hpp"

#include <algorithm>
#include <array>
#include "compass.codegen/ops/values.hpp"

namespace compass::codegen::ops::wire {

namespace {

constexpr std::array<std::string_view, 17> server_owned_fields{
    "tenant_id", "party_id", "version", "modified_by", "performed_by",
    "recorded_at", "valid_from", "valid_to", "started_at", "completed_at",
    "last_event_at", "command_published_at", "terms_accepted_digest",
    "terms_accepted_by", "terms_accepted_at", "change_reason_code",
    "change_commentary"};

bool contains(const std::vector<std::string>& names, std::string_view name) {
    return std::ranges::find(names, name) != names.end();
}

void add_unique(std::vector<std::string>& names, std::string_view name) {
    if (!contains(names, name))
        names.emplace_back(name);
}

std::string parent_id_field(const model_tables& t, value_id entity) {
    if (!values::truthy(t, entity, "has_parent_id"))
        return {};
    const auto presentation = values::member(t, entity, "presentation");
    for (const auto [object, key] : {std::pair{entity, "parent_id_field"},
             std::pair{presentation, "parent_id_field"},
             std::pair{presentation, "parent_key_field"}}) {
        if (values::truthy(t, object, key))
            return values::py_str(t, values::member(t, object, key));
    }
    return {};
}

}

bool is_service_preserved(std::string_view name) {
    constexpr std::array<std::string_view, 7> preserved{"started_at",
        "completed_at", "last_event_at", "command_published_at",
        "terms_accepted_digest", "terms_accepted_by", "terms_accepted_at"};
    return std::ranges::find(preserved, name) != preserved.end();
}

std::string column_name(const model_tables& t, value_id column) {
    for (const auto key : {"column", "field", "name"}) {
        if (values::truthy(t, column, key))
            return values::py_str(t, values::member(t, column, key));
    }
    return {};
}

std::vector<value_id> write_record_columns(const model_tables& t,
    value_id entity) {
    std::vector<value_id> declared = values::items(t,
        values::member(t, entity, "primary_key"), "columns");
    std::ranges::copy(values::items(t, entity, "natural_keys"),
        std::back_inserter(declared));
    std::ranges::copy(values::items(t, entity, "columns"),
        std::back_inserter(declared));
    std::vector<std::string> seen;
    std::vector<value_id> out;
    for (const auto c : declared) {
        const auto name = column_name(t, c);
        if (contains(seen, name))
            continue;
        seen.push_back(name);
        out.push_back(c);
    }
    return out;
}

std::vector<std::string> key_column_names(const model_tables& t,
    value_id entity) {
    std::vector<std::string> out;
    for (const auto c : values::items(t,
             values::member(t, entity, "primary_key"), "columns"))
        out.push_back(column_name(t, c));
    for (const auto c : values::items(t, entity, "natural_keys"))
        out.push_back(column_name(t, c));
    return out;
}

std::string column_cpp_type(const model_tables& t, value_id entity,
    std::string_view name) {
    for (const auto c : write_record_columns(t, entity)) {
        if (column_name(t, c) != name)
            continue;
        const auto type = values::str(t, c, "cpp_type");
        return type.empty() ? "std::string" : std::string(type);
    }
    return "std::string";
}

std::vector<std::string> relation_columns(const model_tables& t,
    value_id entity) {
    std::vector<std::string> names;
    for (const auto e : values::items(t, entity, "extra_list_requests")) {
        if (values::truthy(t, e, "filter_column"))
            names.push_back(values::py_str(t,
                values::member(t, e, "filter_column")));
    }
    const auto parent = parent_id_field(t, entity);
    if (!parent.empty() && !contains(names, parent))
        names.push_back(parent);
    return names;
}

std::string declared_key_field(const model_tables& t, value_id entity) {
    if (values::truthy(t, entity, "key_field"))
        return values::py_str(t, values::member(t, entity, "key_field"));
    const auto presentation = values::member(t, entity, "presentation");
    if (values::truthy(t, presentation, "key_field"))
        return values::py_str(t, values::member(t, presentation, "key_field"));
    return {};
}

value_id declared_key_column(const model_tables& t, value_id entity) {
    const auto name = declared_key_field(t, entity);
    if (name.empty())
        return value_id::none();
    std::vector<value_id> declared = values::items(t,
        values::member(t, entity, "primary_key"), "columns");
    std::ranges::copy(values::items(t, entity, "natural_keys"),
        std::back_inserter(declared));
    std::ranges::copy(values::items(t, entity, "columns"),
        std::back_inserter(declared));
    for (const auto c : declared) {
        if (column_name(t, c) == name)
            return c;
    }
    return value_id::none();
}

std::string_view strip_optional(std::string_view cpp_type) {
    constexpr std::string_view prefix = "std::optional<";
    if (cpp_type.starts_with(prefix) && cpp_type.ends_with('>'))
        return cpp_type.substr(prefix.size(),
            cpp_type.size() - prefix.size() - 1);
    return cpp_type;
}

std::vector<field> write_record_for(const model_tables& t, value_id entity) {
    const auto keys = key_column_names(t, entity);
    std::vector<field> out;
    for (const auto c : write_record_columns(t, entity)) {
        const auto name = column_name(t, c);
        if (values::is_true(t, c, "server_owned"))
            continue;
        if (!contains(keys, name) &&
            std::ranges::find(server_owned_fields, name) !=
                server_owned_fields.end())
            continue;
        const auto type = values::str(t, c, "cpp_type");
        out.push_back({.name = name,
            .cpp_type = type.empty() ? "std::string" : std::string(type),
            .default_value = {},
            .domain_member = std::string(values::str(t, c, "group_prefix")) +
                name,
            .render_is_enum = values::truthy(t, c, "render_is_enum") ||
                values::truthy(t, c, "is_enum")});
    }
    return out;
}

filter_spec filter_members(const model_tables& t, value_id entity) {
    const auto relations = relation_columns(t, entity);
    std::vector<std::string> filterable;
    std::vector<std::string> searchable;
    for (const auto c : write_record_columns(t, entity)) {
        if (values::is_true(t, c, "filterable"))
            filterable.push_back(column_name(t, c));
        if (values::is_true(t, c, "searchable"))
            searchable.push_back(column_name(t, c));
    }
    std::vector<std::string> equals;
    if (values::truthy(t, entity, "list_filter_column"))
        equals.push_back(values::py_str(t,
            values::member(t, entity, "list_filter_column")));
    for (const auto& n : relations)
        add_unique(equals, n);
    for (const auto& n : filterable)
        add_unique(equals, n);

    std::vector<std::string> key_columns;
    for (const auto c : values::items(t,
             values::member(t, entity, "primary_key"), "columns"))
        key_columns.push_back(column_name(t, c));
    std::vector<std::string> one_of;
    if (key_columns.size() == 1)
        one_of.push_back(key_columns.front());
    for (const auto& n : relations)
        add_unique(one_of, n);
    for (const auto& n : filterable)
        add_unique(one_of, n);

    filter_spec out;
    out.searchable = std::move(searchable);
    using kind = filter_member::kind;
    for (const auto& name : equals) {
        const auto type = column_cpp_type(t, entity, name);
        const std::string read(strip_optional(type));
        out.members.push_back({.k = kind::equals, .member = name,
            .column = name, .cpp_type = type, .read_type = read,
            .is_nullable = type != read});
    }
    for (const auto& name : one_of) {
        const std::string read(strip_optional(
            column_cpp_type(t, entity, name)));
        out.members.push_back({.k = kind::one_of, .member = name + "_one_of",
            .column = name, .cpp_type = "std::vector<" + read + ">",
            .read_type = read,
            .is_key = key_columns.size() == 1 && name == key_columns.front()});
    }
    for (const auto& name : filterable) {
        const auto declared = column_cpp_type(t, entity, name);
        const auto type = strip_optional(declared);
        const auto is_date = type == "std::chrono::year_month_day";
        const auto is_timestamp =
            type == "std::chrono::system_clock::time_point";
        if (!is_date && !is_timestamp)
            continue;
        for (const auto from : {true, false}) {
            out.members.push_back({.k = kind::range,
                .member = name + (from ? "_from" : "_to"), .column = name,
                .cpp_type = "std::string", .is_from = from,
                .is_timestamp = is_timestamp});
        }
    }
    return out;
}

void append_field(compass::model::ops::builder& b, value_id list,
    const field& f, const compass::model::ops::custom_types& custom) {
    const auto o = b.append_object(list);
    b.set(o, "name", f.name);
    b.set(o, "cpp_type", f.cpp_type);
    b.set(o, "comment", "");
    if (!f.default_value.empty())
        b.set(o, "default", f.default_value);
    const auto ts = compass::model::ops::ts_type(f.cpp_type, custom);
    if (!ts.empty())
        b.set(o, "ts_type", ts);
    if (f.domain_member) {
        b.set(o, "domain_member", *f.domain_member);
        b.set(o, "render_is_enum", f.render_is_enum);
    }
}

}
