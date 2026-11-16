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
#include <functional>
#include <string>
#include <vector>
#include "compass.codegen/ops/values.hpp"
#include "entity_text.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::model_tables;
using compass::model::types::value_id;

namespace {

constexpr std::string_view numeric_cpp_types[] = {
    "int", "long", "long long", "short", "double", "float",
    "std::int16_t", "std::int32_t", "std::int64_t",
    "std::uint16_t", "std::uint32_t", "std::uint64_t", "std::size_t"};

/*
 * _key_as_text: one key column's value in the text form a key parameter
 * takes.
 */
std::string key_as_text(const model_tables& t, std::string_view expression,
    value_id column) {
    const std::string e(expression);
    if (values::truthy(t, column, "is_uuid"))
        return "boost::uuids::to_string(" + e + ")";
    if (values::truthy(t, column, "is_timestamp"))
        return "ores::platform::time::datetime::to_db_string(" + e + ")";
    if (std::ranges::find(numeric_cpp_types,
            values::str(t, column, "cpp_type")) != std::end(numeric_cpp_types))
        return "std::to_string(" + e + ")";
    return e;
}

/*
 * _enrich_primary_key_field: the type flags, uuid_check_fn, a default
 * cpp_type and render_label of the primary key or one of its columns.
 */
void enrich_key_field(builder& b, value_id field) {
    const auto& t = b.tables();
    const auto type = values::py_str(t, values::member(t, field, "type"),
        "uuid");
    const std::string cpp_type(values::str(t, field, "cpp_type"));
    const bool is_uuid = type == "uuid";
    b.set(field, "is_uuid", is_uuid);
    b.set(field, "is_text", type == "text");
    b.set(field, "is_int", type == "integer" || type == "int");
    b.set(field, "is_timestamp", type == "timestamp" || type == "timestamptz" ||
        type == "timestamp with time zone" ||
        cpp_type.find("time_point") != std::string::npos);
    b.set(field, "is_date",
        type == "date" || cpp_type == "std::chrono::year_month_day");
    if (is_uuid && b.find(field, "uuid_check_fn").is_none())
        b.set(field, "uuid_check_fn", "ores_utility_nil_uuid_fn()");
    if (b.find(field, "cpp_type").is_none())
        b.set(field, "cpp_type", is_uuid ? "boost::uuids::uuid" : "std::string");
    b.set(field, "render_label",
        text::render_label(values::str(t, field, "column")));
}

/*
 * One key column as the key strings read it: its name, the member it is
 * reached through and its row.
 */
struct key_column {
    std::string name;
    std::string prefix;
    value_id row;
};

std::string joined(const std::vector<key_column>& columns,
    std::string_view separator,
    const std::function<std::string(const key_column&)>& one) {
    std::string out;
    for (std::size_t i = 0; i < columns.size(); ++i) {
        if (i != 0)
            out += separator;
        out += one(columns[i]);
    }
    return out;
}

std::string group_prefix(const model_tables& t, value_id c) {
    return values::truthy(t, c, "group_prefix")
        ? values::py_str(t, values::member(t, c, "group_prefix"))
        : std::string();
}

/*
 * _column_name: column, field or name, the first one stated.
 */
std::string column_name(const model_tables& t, value_id c) {
    for (const auto* key : {"column", "field", "name"}) {
        if (values::truthy(t, c, key))
            return values::py_str(t, values::member(t, c, key));
    }
    return {};
}

/*
 * declared_key_field: the key the model declares on the wire, or empty.
 */
std::string declared_key_field(const model_tables& t, value_id entity) {
    if (values::truthy(t, entity, "key_field"))
        return values::py_str(t, values::member(t, entity, "key_field"));
    const auto presentation = values::member(t, entity, "presentation");
    if (values::truthy(t, presentation, "key_field"))
        return values::py_str(t, values::member(t, presentation, "key_field"));
    return {};
}

/*
 * declared_key_column: the declared key's own column, from the primary
 * key's columns, the natural keys or the columns, or none.
 */
value_id declared_key_column(const model_tables& t, value_id entity,
    const std::string& name) {
    if (name.empty())
        return value_id::none();
    auto sources = values::items(t, values::member(t, entity, "primary_key"),
        "columns");
    for (const auto* key : {"natural_keys", "columns"}) {
        const auto more = values::items(t, entity, key);
        sources.insert(sources.end(), more.begin(), more.end());
    }
    for (const auto c : sources) {
        if (column_name(t, c) == name)
            return c;
    }
    return value_id::none();
}

/*
 * The columns and natural keys marked :sql_key:.
 */
std::vector<value_id> sql_key_extras(const model_tables& t, value_id entity) {
    std::vector<value_id> out;
    for (const auto* key : {"columns", "natural_keys"}) {
        for (const auto c : values::items(t, entity, key)) {
            if (values::truthy(t, c, "sql_key"))
                out.push_back(c);
        }
    }
    return out;
}

std::string name_or_column(const model_tables& t, value_id c) {
    const auto name = values::member(t, c, "name");
    return values::truthy(t, name) ? values::py_str(t, name)
        : values::py_str(t, values::member(t, c, "column"), "None");
}

void derive_storage_key(builder& b, value_id entity, value_id pk,
    const std::vector<key_column>& columns) {
    const auto& t = b.tables();
    std::vector<std::string> sql_keys;
    for (const auto& c : columns)
        sql_keys.push_back(c.name);
    std::string party_member = "party_id";
    bool party_sql_key = false;
    for (const auto c : sql_key_extras(t, entity)) {
        const auto name = name_or_column(t, c);
        sql_keys.push_back(name);
        if (name == "party_id" && !party_sql_key) {
            party_sql_key = true;
            party_member = group_prefix(t, c) + "party_id";
        }
    }
    b.set(pk, "party_sql_key", party_sql_key);
    b.set(pk, "party_member", party_member);
    const auto sql_joined = [&](std::string_view separator,
        const std::function<std::string(const std::string&)>& one) {
        std::string out;
        for (std::size_t i = 0; i < sql_keys.size(); ++i)
            out += (i == 0 ? "" : std::string(separator)) + one(sql_keys[i]);
        return out;
    };
    b.set(pk, "column_list", sql_joined(", ",
        [](const std::string& c) { return c; }));
    b.set(pk, "gist_column_clause", sql_joined("\n        ",
        [](const std::string& c) { return c + " WITH =,"; }));
    b.set(pk, "where_new", sql_joined(" and ",
        [](const std::string& c) { return c + " = NEW." + c; }));
    b.set(pk, "where_old", sql_joined(" and ",
        [](const std::string& c) { return c + " = OLD." + c; }));
    b.set(pk, "index_suffix", joined(columns, "_",
        [](const key_column& c) { return c.name; }));
}

void derive_repository_key(builder& b, value_id entity, value_id pk,
    const std::vector<key_column>& columns) {
    const auto& t = b.tables();
    const auto each = [&](std::string_view separator,
        const std::function<std::string(const key_column&)>& one) {
        return joined(columns, separator, one);
    };
    b.set(pk, "where_and", each(" && ", [](const key_column& c) {
        return "\"" + c.name + "\"_c == " + c.name; }));
    b.set(pk, "order_by_cols", each(", ", [](const key_column& c) {
        return "\"" + c.name + "\"_c"; }));
    const auto params = each(", ", [](const key_column& c) {
        return "const std::string& " + c.name; });
    const auto params_unnamed = each(", ", [](const key_column&) {
        return std::string("const std::string&"); });
    const auto args = each(", ", [](const key_column& c) { return c.name; });
    b.set(pk, "params", params);
    b.set(pk, "params_unnamed", params_unnamed);
    b.set(pk, "args", args);
    const bool single_uuid = columns.size() == 1 &&
        values::truthy(t, columns[0].row, "is_uuid");
    b.set(pk, "typed_params", single_uuid
        ? "const boost::uuids::uuid& " + columns[0].name : params);
    b.set(pk, "typed_params_unnamed", single_uuid
        ? std::string("const boost::uuids::uuid&") : params_unnamed);
    b.set(pk, "typed_args", single_uuid
        ? "boost::uuids::to_string(" + columns[0].name + ")" : args);
    b.set(pk, "is_single_uuid", single_uuid);

    const auto record_args = [&](const std::string& prefix) {
        return each(", ", [&](const key_column& c) {
            return key_as_text(t, prefix + c.name, c.row); });
    };
    b.set(pk, "key_request_args", record_args("request.key."));
    b.set(pk, "removal_key_args", record_args("request.removal.key."));
    auto singular = values::truthy(t, entity, "entity_singular")
        ? values::py_str(t, values::member(t, entity, "entity_singular"))
        : values::truthy(t, entity, "name_singular")
        ? values::py_str(t, values::member(t, entity, "name_singular"))
        : std::string();
    b.set(pk, "version_key_args", record_args("request.key." + singular + "."));
}

void copy_key_columns(builder& b, value_id pk,
    const std::vector<key_column>& columns) {
    const auto list = b.list(pk, "key_columns");
    for (const auto& c : columns)
        b.append_copy(list, c.row);
    mark_last_item(b, list);
}

void derive_value_key(builder& b, value_id pk,
    const std::vector<key_column>& columns) {
    const auto& t = b.tables();
    const auto each = [&](std::string_view separator,
        const std::function<std::string(const key_column&)>& one) {
        return joined(columns, separator, one);
    };
    const auto v_arg = [&](const key_column& c) {
        return key_as_text(t, "v." + c.prefix + c.name, c.row);
    };
    b.set(pk, "v_args", each(", ", v_arg));
    b.set(pk, "value_text", columns.empty() ? std::string() : v_arg(columns[0]));
    for (std::size_t i = 1; i < columns.size(); ++i)
        b.set(columns[i].row, "value_text", v_arg(columns[i]));
    b.set(pk, "row_args", each(", ", [&](const key_column& c) {
        return key_as_text(t, "row." + c.prefix + c.name, c.row); }));
    b.set(pk, "batch_keys_args", each(", ", [](const key_column& c) {
        return c.name + "_keys"; }));
    b.set(pk, "log_fields", "\"" + each(" << \" ", [](const key_column& c) {
        return c.name + ": \" << " + c.name; }));
    b.set(pk, "value_log_fields", "\"" + each(" << \" ",
        [](const key_column& c) {
            return c.name + ": \" << v." + c.prefix + c.name; }));
}

void derive_batch_key(builder& b, value_id pk,
    const std::vector<key_column>& columns) {
    const auto& t = b.tables();
    const auto each = [&](std::string_view separator,
        const std::function<std::string(const key_column&)>& one) {
        return joined(columns, separator, one);
    };
    b.set(pk, "batch_params", each(", ", [](const key_column& c) {
        return "const std::vector<std::string>& " + c.name + "s"; }));
    b.set(pk, "batch_params_unnamed", each(", ", [](const key_column&) {
        return std::string("const std::vector<std::string>&"); }));
    b.set(pk, "batch_args", each(", ", [](const key_column& c) {
        return c.name + "s"; }));
    b.set(pk, "batch_empty_check", each(" || ", [](const key_column& c) {
        return c.name + "s.empty()"; }));
    b.set(pk, "batch_in_and", each(" && ", [](const key_column& c) {
        return "\"" + c.name + "\"_c.in(" + c.name + "s)"; }));
    b.set(pk, "batch_tuple_type", each(", ", [](const key_column&) {
        return std::string("std::string"); }));
    b.set(pk, "batch_requested_insert", "requested.emplace(" +
        each(", ", [](const key_column& c) { return c.name + "s[i]"; }) +
        ");");
    const auto first = columns.empty() ? std::string() : columns[0].name;
    b.set(pk, "batch_requested_size_check", first + "s.size()");
    b.set(pk, "batch_tuple_from_item", each(", ", [&](const key_column& c) {
        return key_as_text(t, "item." + c.prefix + c.name, c.row); }));
    b.set(pk, "has_timestamp_key", std::ranges::any_of(columns,
        [&](const key_column& c) {
            return values::truthy(t, c.row, "is_timestamp"); }));
    const std::vector<key_column> rest(columns.begin() +
        std::min<std::size_t>(1, columns.size()), columns.end());
    b.set(pk, "batch_size_mismatch_check", joined(rest, " || ",
        [&](const key_column& c) {
            return c.name + "s.size() != " + first + "s.size()"; }));
    b.set(pk, "batch_loop_args", each(", ", [](const key_column& c) {
        return c.name + "s[i]"; }));
}

void derive_notify_key(builder& b, value_id entity, value_id pk,
    const std::vector<key_column>& columns) {
    const auto& t = b.tables();
    const auto declared_name = declared_key_field(t, entity);
    const auto declared = declared_key_column(t, entity, declared_name);
    std::vector<std::pair<std::string, std::string>> notify;
    if (!declared.is_none()) {
        const auto column = values::member(t, declared, "column");
        notify.emplace_back(column.is_none() ? column_name(t, declared)
                                             : values::py_str(t, column),
            values::py_str(t, values::member(t, declared, "type"), "None"));
    } else {
        for (const auto& c : columns)
            notify.emplace_back(c.name,
                values::py_str(t, values::member(t, c.row, "type"), "None"));
    }
    const auto each = [&](std::string_view separator,
        const std::function<std::string(const std::string&,
            const std::string&)>& one) {
        std::string out;
        for (std::size_t i = 0; i < notify.size(); ++i)
            out += (i == 0 ? "" : std::string(separator)) +
                one(notify[i].first, notify[i].second);
        return out;
    };
    b.set(pk, "notify_declarations", each("\n    ",
        [](const std::string& c, const std::string& type) {
            return "changed_" + c + " " + type + ";"; }));
    b.set(pk, "notify_assign_old", each("\n        ",
        [](const std::string& c, const std::string&) {
            return "changed_" + c + " := OLD." + c + ";"; }));
    b.set(pk, "notify_assign_new", each("\n        ",
        [](const std::string& c, const std::string&) {
            return "changed_" + c + " := NEW." + c + ";"; }));
    if (!declared.is_none())
        b.set(pk, "key_equals_v", "decoded->key." + declared_name + " == v." +
            group_prefix(t, declared) + declared_name);
    else
        b.set(pk, "key_equals_v", joined(columns, " && ",
            [](const key_column& c) {
                return "decoded->key." + c.name + " == v." + c.prefix + c.name;
            }));
    b.set(pk, "notify_key_object", "jsonb_build_object(" + each(", ",
        [](const std::string& c, const std::string&) {
            return "'" + c + "', changed_" + c; }) + ")");
    b.set(pk, "notify_id_array", each(", ",
        [](const std::string& c, const std::string&) {
            return "changed_" + c; }));
}

}

void derive_primary_key(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto pk = values::member(t, entity, "primary_key");
    if (pk.is_none())
        return;
    enrich_key_field(b, pk);
    const auto rows = values::items(t, pk, "columns");
    for (const auto c : rows)
        enrich_key_field(b, c);
    const auto list = values::member(t, pk, "columns");
    if (!list.is_none())
        mark_last_item(b, list);
    b.set(pk, "is_compound", rows.size() > 1);

    std::vector<key_column> columns;
    for (const auto c : rows) {
        const auto name = values::truthy(t, c, "column")
            ? values::py_str(t, values::member(t, c, "column"))
            : values::py_str(t, values::member(t, c, "name"), "None");
        columns.push_back({name, group_prefix(t, c), c});
    }
    for (const auto& c : columns) {
        b.set(c.row, "key_text", key_as_text(t, "key." + c.name, c.row));
        b.set(c.row, "removal_key_text",
            key_as_text(t, "removal.key." + c.name, c.row));
        b.set(c.row, "row_text",
            key_as_text(t, "row." + c.prefix + c.name, c.row));
    }
    const bool timestamp_key = std::ranges::any_of(rows,
        [&](value_id c) { return values::truthy(t, c, "is_timestamp"); });
    b.set(entity, "has_timestamp_key_columns", timestamp_key);
    if (timestamp_key)
        b.set(entity, "has_date_or_timestamp_natural_keys", true);

    derive_storage_key(b, entity, pk, columns);
    derive_repository_key(b, entity, pk, columns);
    copy_key_columns(b, pk, columns);
    derive_value_key(b, pk, columns);
    derive_batch_key(b, pk, columns);
    derive_notify_key(b, entity, pk, columns);
}

void project_primary_key_extra_columns(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto pk = values::member(t, entity, "primary_key");
    if (pk.is_none())
        return;
    const auto rows = values::items(t, pk, "columns");
    const auto list = b.list(pk, "extra_columns");
    for (std::size_t i = 1; i < rows.size(); ++i)
        b.append_copy(list, rows[i]);
}

}
