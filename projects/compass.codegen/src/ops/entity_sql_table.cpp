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
#include "compass.codegen/ops/values.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::model_tables;
using compass::model::types::value_id;
using compass::model::types::value_kind;

namespace {

bool is_container(const model_tables& t, value_id v) {
    return values::is(t, v, value_kind::list) ||
        values::is(t, v, value_kind::object);
}

struct coding_scheme_flags {
    bool required = false;
    bool nullable = false;
};

coding_scheme_flags coding_scheme_of(const model_tables& t, value_id entity) {
    const auto scheme = values::str(t, entity, "coding_scheme");
    return {.required = scheme == "required", .nullable = scheme == "nullable"};
}

void write_coding_scheme_flags(builder& b, value_id target,
    coding_scheme_flags f, bool has_image_id) {
    b.set(target, "has_coding_scheme", f.required);
    b.set(target, "has_nullable_coding_scheme", f.nullable);
    b.set(target, "has_any_coding_scheme", f.required || f.nullable);
    b.set(target, "has_image_id", has_image_id);
}

std::string description_comment(std::string_view description) {
    if (description.empty())
        return "--";
    std::string out;
    std::size_t start = 0;
    for (;;) {
        const auto end = description.find('\n', start);
        const auto line = description.substr(start,
            end == std::string_view::npos ? std::string_view::npos
                                          : end - start);
        if (start != 0)
            out += '\n';
        if (values::strip(line).empty())
            out += "--";
        else
            out.append("-- ").append(line);
        if (end == std::string_view::npos)
            return out;
        start = end + 1;
    }
}

std::string sql_check_constraints(const model_tables& t, value_id checks) {
    std::string out;
    for (const auto c : values::items(t, checks)) {
        if (!out.empty())
            out += ",\n";
        out.append("    check (")
            .append(values::str(t, c, "expression"))
            .append(")");
    }
    return out;
}

/*
 * _sql_column_from: the name, type, nullable and default the schema
 * template reads, with default as the text str() gives it.
 */
void append_sql_column(builder& b, value_id columns, value_id source,
    std::string_view name_key) {
    const auto column = b.append_object(columns);
    const auto& t = b.tables();
    b.set_copy(column, "name", values::member(t, source, name_key));
    b.set_copy(column, "type", values::member(t, source, "type"));
    if (const auto v = values::member(b.tables(), source, "nullable");
        !v.is_none())
        b.set_copy(column, "nullable", v);
    if (const auto v = values::member(b.tables(), source, "default");
        !v.is_none())
        b.set(column, "default", values::py_str(b.tables(), v));
}

/*
 * The table's primary key is a shallow copy of the entity's: its own
 * scalars as loaded, its containers (the columns list) shared with the
 * entity and so as enriched.
 */
void write_primary_key(builder& b, value_id table, value_id loaded,
    value_id entity) {
    const auto pk = b.object(table, "primary_key");
    const auto loaded_pk = values::member(b.tables(), loaded, "primary_key");
    const auto final_pk = values::member(b.tables(), entity, "primary_key");
    for (const auto m : values::items(b.tables(), loaded_pk)) {
        const std::string key(values::key_of(b.tables(), m));
        const auto shared = values::member(b.tables(), final_pk, key);
        b.set_copy(pk, key, is_container(b.tables(), m) && !shared.is_none()
            ? shared : m);
    }
    if (values::member(b.tables(), pk, "is_text").is_none()) {
        const auto type = values::member(b.tables(), pk, "type");
        if (!type.is_none())
            b.set(pk, "is_text", values::is(b.tables(), type,
                value_kind::string) && values::str(b.tables(), type) == "text");
    }
}

/*
 * A list the table takes with de.get(key, []): shared with the entity, so
 * as enriched, when the loaded entity has it; a new empty list otherwise.
 */
void write_shared_list(builder& b, value_id table, value_id loaded,
    value_id entity, std::string_view key) {
    if (values::member(b.tables(), loaded, key).is_none()) {
        b.list(table, key);
        return;
    }
    b.set_copy(table, key, values::member(b.tables(), entity, key));
}

}

void mark_last_item(builder& b, value_id list, std::string_view key) {
    const auto items = values::items(b.tables(), list);
    if (!values::is(b.tables(), list, value_kind::list) || items.empty() ||
        !values::is(b.tables(), items.back(), value_kind::object))
        return;
    b.set(items.back(), key, true);
}

void derive_sql_table_flags(builder& b, value_id entity) {
    mark_last_item(b, values::member(b.tables(), entity, "indexes"));
    mark_last_item(b, values::member(b.tables(), entity, "check_constraints"));
    mark_last_item(b, values::member(b.tables(),
        values::member(b.tables(), entity, "insert_trigger"), "validations"));

    if (const auto fn = values::member(b.tables(), entity, "validation_fn");
        !fn.is_none()) {
        const std::string scope(values::str(b.tables(), fn, "tenant_scope"));
        b.set(fn, "scope_system", scope == "system");
        b.set(fn, "scope_both", scope == "both");
        b.set(fn, "scope_tenant", scope == "tenant");
        if (values::member(b.tables(), fn, "order_by").is_none())
            b.set_copy(fn, "order_by", values::member(b.tables(),
                values::member(b.tables(), entity, "primary_key"), "column"));
    }

    write_coding_scheme_flags(b, entity, coding_scheme_of(b.tables(), entity),
        values::truthy(b.tables(), entity, "image_id"));
}

void project_sql_table(builder& b, value_id context, value_id loaded,
    value_id entity) {
    const auto table = b.object(context, "table");
    for (const auto key : {"product", "schema", "component", "entity_singular",
             "entity_plural", "description"}) {
        if (const auto v = values::member(b.tables(), loaded, key);
            !v.is_none())
            b.set_copy(table, key, v);
    }
    if (const auto v = values::member(b.tables(), loaded, "coding_scheme");
        !v.is_none())
        b.set_copy(table, "coding_scheme", v);
    else
        b.set(table, "coding_scheme", "none");
    const auto tenant = values::member(b.tables(), loaded, "has_tenant_id");
    const bool has_tenant_id = tenant.is_none() ||
        values::truthy(b.tables(), tenant);
    b.set(table, "has_tenant_id", has_tenant_id);
    if (const auto v = values::member(b.tables(), loaded, "image_id");
        !v.is_none())
        b.set_copy(table, "image_id", v);

    write_primary_key(b, table, loaded, entity);

    const auto columns = b.list(table, "columns");
    for (const auto k : values::items(b.tables(), loaded, "natural_keys"))
        append_sql_column(b, columns, k, "column");
    for (const auto c : values::items(b.tables(), loaded, "columns"))
        append_sql_column(b, columns, c, "name");

    for (const auto key : {"validation_fn", "insert_trigger"}) {
        if (!values::member(b.tables(), loaded, key).is_none())
            b.set_copy(table, key, values::member(b.tables(), entity, key));
    }
    write_shared_list(b, table, loaded, entity, "check_constraints");
    write_shared_list(b, table, loaded, entity, "indexes");

    mark_last_item(b, columns);
    b.set(table, "description_comment", description_comment(
        std::string(values::str(b.tables(), loaded, "description"))));
    write_coding_scheme_flags(b, table, coding_scheme_of(b.tables(), loaded),
        values::truthy(b.tables(), loaded, "image_id"));
    b.set(table, "has_tenant_id", has_tenant_id);
    const auto checks = values::member(b.tables(), loaded, "check_constraints");
    b.set(table, "has_check_constraints", values::truthy(b.tables(), checks));
    b.set(table, "sql_check_constraints",
        sql_check_constraints(b.tables(), checks));
}

}
