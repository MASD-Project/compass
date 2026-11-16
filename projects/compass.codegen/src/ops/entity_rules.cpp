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

#include <map>
#include <string>
#include "compass.codegen/ops/values.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::model_tables;
using compass::model::types::value_id;
using compass::model::types::value_kind;

namespace {

/*
 * Whether each column the entity declares admits null, by name: the
 * columns, then the natural keys, then the primary key's columns, a later
 * one winning.
 */
std::map<std::string, bool, std::less<>> nullable_by_column(
    const model_tables& t, value_id entity) {
    std::map<std::string, bool, std::less<>> out;
    auto sources = values::items(t, entity, "columns");
    const auto nks = values::items(t, entity, "natural_keys");
    sources.insert(sources.end(), nks.begin(), nks.end());
    const auto pk = values::items(t, values::member(t, entity, "primary_key"),
        "columns");
    sources.insert(sources.end(), pk.begin(), pk.end());
    for (const auto c : sources) {
        const auto name = values::member(t, c, "name");
        const auto key = values::py_str(t,
            name.is_none() ? values::member(t, c, "column") : name, "None");
        out[key] = values::truthy(t, c, "nullable");
    }
    return out;
}

void validate_nullable_columns(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto validations = values::items(t,
        values::member(t, entity, "insert_trigger"), "validations");
    bool declared = false;
    for (const auto v : validations)
        declared = declared || values::str(t, v, "column") == "change_reason_code";
    b.set(entity, "change_reason_code_declared", declared);
    const auto nullable = nullable_by_column(t, entity);
    for (const auto v : validations) {
        const auto it = nullable.find(values::py_str(t,
            values::member(t, v, "column"), "None"));
        b.set(v, "nullable", it != nullable.end() && it->second);
    }
}

/*
 * A current-state table has no validity window: no current-only index
 * predicate, no point-in-time read, no touch function and no bi-temporal
 * trigger pair.
 */
void drop_validity_window(builder& b, value_id entity, value_id sql) {
    const auto& t = b.tables();
    for (const auto index : values::items(t, entity, "indexes"))
        b.set(index, "current_only", false);
    b.set(entity, "has_as_of_lookup", false);
    for (const auto fk : values::items(t, entity, "foreign_keys"))
        b.remove(fk, "list_by_as_of");
    b.set(entity, "generate_touch_function", false);
    if (values::is(t, sql, value_kind::object)) {
        b.remove(sql, "has_list_by_as_of");
        b.set(sql, "bitemporal_soft_update", false);
    }
}

void mark_sql_lists(builder& b, value_id entity, value_id sql) {
    const auto& t = b.tables();
    const auto copies = values::member(t, sql, "fk_copy_validations");
    if (!copies.is_none()) {
        mark_last_item(b, copies);
        for (const auto c : values::items(t, copies)) {
            for (const auto* key : {"declare_vars", "copy_empty"}) {
                const auto list = values::member(t, c, key);
                if (!list.is_none())
                    mark_last_item(b, list);
            }
        }
    }
    const auto fks = values::member(t, entity, "foreign_keys");
    if (!fks.is_none())
        mark_last_item(b, fks);
    for (const auto* key : {"text_code_validations", "extra_delete_sets"}) {
        const auto list = values::member(t, sql, key);
        if (!list.is_none())
            mark_last_item(b, list);
    }
}

}

void derive_table_rules(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto sql = values::member(t, entity, "sql");
    const bool current_state = values::is_true(t, entity, "current_state");
    const bool hypertable = values::truthy(t, sql, "hypertable");
    const auto gist = values::member(t, sql, "gist_exclusion");
    if (current_state || hypertable || gist.is_none())
        b.set(entity, "has_gist_exclusion", !current_state && !hypertable);
    else
        b.set_copy(entity, "has_gist_exclusion", gist);
    const bool audit = !values::truthy(t, sql, "no_audit_columns") &&
        !current_state;
    b.set(entity, "has_audit_columns", audit);
    validate_nullable_columns(b, entity);
    if (current_state)
        drop_validity_window(b, entity, sql);
    mark_sql_lists(b, entity, sql);

    const std::string temporal = current_state ? "" : " && \"valid_to\"_c == max";
    b.set(entity, "temporal_filter", temporal);
    b.set(entity, "list_temporal_filter",
        values::truthy(t, entity, "has_as_of_lookup") ? std::string()
                                                      : temporal);
    b.set(entity, "version_filter",
        audit ? " && \"version\"_c == expected" : "");
}

}
