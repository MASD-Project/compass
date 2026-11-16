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
#include "compass.codegen/ops/entity_registry.hpp"
#include "compass.codegen/ops/values.hpp"
#include "entity_text.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::model_tables;
using compass::model::types::value_id;
using compass::model::types::value_kind;

namespace {

std::string name_or_column(const model_tables& t, value_id c) {
    const auto name = values::member(t, c, "name");
    return values::truthy(t, name) ? values::py_str(t, name)
        : values::py_str(t, values::member(t, c, "column"));
}

bool any_fixed(const model_tables& t, value_id entity, std::string_view name) {
    for (const auto* list : {"columns", "natural_keys"}) {
        for (const auto c : values::items(t, entity, list)) {
            if (values::truthy(t, c, "fixed") && name_or_column(t, c) == name)
                return true;
        }
    }
    return false;
}

/*
 * The as_of opt-out: on unless the model says false, no or 0.
 */
bool as_of_lookup(const model_tables& t, value_id entity) {
    const auto v = values::member(t, entity, "has_as_of_lookup");
    if (v.is_none() || values::is(t, v, value_kind::null))
        return true;
    const auto s = text::lower(values::strip(values::py_str(t, v)));
    return s != "false" && s != "no" && s != "0";
}

}

void derive_party_scope(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto sql = values::member(t, entity, "sql");
    const auto index_prefix = values::member(t, sql, "index_prefix");
    const auto plural = values::member(t, entity, "entity_plural");
    if (!index_prefix.is_none())
        b.set_copy(entity, "index_name_prefix", index_prefix);
    else if (!plural.is_none())
        b.set_copy(entity, "index_name_prefix", plural);
    else
        b.set(entity, "index_name_prefix", "unknown");

    value_id party_column = value_id::none();
    for (const auto c : values::items(t, entity, "columns")) {
        if (party_column.is_none() && values::str(t, c, "name") == "party_id")
            party_column = c;
    }
    bool party_key = false;
    for (const auto k : values::items(t, entity, "natural_keys"))
        party_key = party_key || values::str(t, k, "column") == "party_id";
    const bool has_party = !party_column.is_none() || party_key;
    b.set(entity, "has_party_column", has_party);
    b.set(entity, "party_column_optional", !party_column.is_none() &&
        values::str(t, party_column, "cpp_type").starts_with("std::optional"));

    bool parent_is_party = false;
    for (const auto fk : values::items(t, entity, "foreign_keys"))
        parent_is_party = parent_is_party ||
            values::truthy(t, fk, "parent_is_party");
    b.set(entity, "party_scoped_writes",
        (values::truthy(t, sql, "party_id_from_session") || has_party ||
         values::truthy(t, sql, "party_scoped_writes")) && !parent_is_party);
}

void derive_storage_shape(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto sql = values::member(t, entity, "sql");
    const bool current_state = values::truthy(t, sql, "current_state");
    b.set(entity, "current_state", current_state);
    b.set(entity, "has_as_of_lookup", as_of_lookup(t, entity));
    b.set(entity, "immutable", values::truthy(t, sql, "immutable"));
    b.set(entity, "append_insert", values::truthy(t, sql, "append_insert"));
    b.set(entity, "nullable_tenant_id",
        values::truthy(t, sql, "nullable_tenant_id"));
    b.set(entity, "has_newest_read",
        current_state && values::truthy(t, sql, "newest_by"));

    const auto fixed = b.list(entity, "fixed_columns");
    for (const auto& name : column_names_of(t, entity)) {
        if (any_fixed(t, entity, name))
            b.set(b.append_object(fixed), "column", name);
    }
    b.set(entity, "immutable_tenant_key", immutable_tenant_key_of(t, entity));

    const bool has_tenant_id = values::truthy(t, entity, "has_tenant_id");
    const bool system_scope = values::truthy(t, sql, "system_scope");
    const bool nullable_tenant = values::truthy(t, sql, "nullable_tenant_id");
    const bool hypertable = values::truthy(t, sql, "hypertable");
    const bool tenant_in_pk = has_tenant_id && !current_state &&
        !system_scope && !nullable_tenant && !hypertable;
    b.set(entity, "has_tenant_in_pk", tenant_in_pk);
    b.set(entity, "validates_tenant_on_insert", tenant_in_pk ||
        (current_state && has_tenant_id && !system_scope && !nullable_tenant));

    if (!values::is(t, sql, value_kind::object))
        return;
    if (hypertable)
        b.set(sql, "hypertable", true);
    b.set(sql, "bitemporal_soft_update",
        values::str(t, sql, "bitemporal_trigger") == "soft_update_delete");
    b.set(sql, "rls_hot_path",
        values::str(t, sql, "rls_policy_style") == "hot_path");
}

}
