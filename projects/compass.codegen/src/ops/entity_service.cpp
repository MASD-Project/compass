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

namespace {

bool any_truthy(const model_tables& t, value_id list, std::string_view key) {
    for (const auto v : values::items(t, list)) {
        if (values::truthy(t, v, key))
            return true;
    }
    return false;
}

}

void derive_service_gates(builder& b, value_id entity,
    const types::entity_site& site) {
    const auto& t = b.tables();
    const auto derived = !site.protocol_owned;
    b.set(entity, "protocol_derived", derived);

    const auto pk = values::member(t, entity, "primary_key");
    const auto foreign_keys = values::member(t, entity, "foreign_keys");
    const auto write_fields = values::member(t, entity, "write_fields");
    const auto declared_uuid = !values::truthy(t, entity, "key_is_primary") &&
        values::truthy(t, entity, "declared_key_is_uuid");
    bool fk_list_by_uuid = false;
    for (const auto fk : values::items(t, foreign_keys)) {
        fk_list_by_uuid = fk_list_by_uuid || (values::truthy(t, fk, "list_by") &&
            values::truthy(t, fk, "list_by_uuid"));
    }
    b.set(entity, "service_pages_versions",
        derived && values::truthy(t, entity, "list_versions_operations"));
    b.set(entity, "service_uses_datetime",
        (derived && values::truthy(t, entity, "put_operations") &&
         (any_truthy(t, write_fields, "date_from_string") ||
          any_truthy(t, write_fields, "timestamp_from_string"))) ||
        values::truthy(t, pk, "has_timestamp_key"));
    b.set(entity, "service_parses_declared_uuid", declared_uuid);
    b.set(entity, "service_uses_uuid",
        values::truthy(t, entity, "has_uuid_include") ||
        values::truthy(t, pk, "is_single_uuid") || declared_uuid);
    b.set(entity, "service_lists_by_as_of",
        any_truthy(t, foreign_keys, "list_by_as_of"));
    b.set(entity, "service_uses_uuid_io",
        any_truthy(t, values::member(t, pk, "columns"), "is_uuid") ||
        (derived && any_truthy(t,
            values::member(t, entity, "list_scoped_operations"),
            "leading_is_uuid")) ||
        fk_list_by_uuid || values::truthy(t, entity, "service_find_by_uuid") ||
        values::truthy(t, values::member(t, entity, "service_find_by_code"),
            "parent_column") ||
        declared_uuid || values::truthy(t, entity, "has_parent_id"));
    b.set(entity, "repository_uses_utility",
        values::truthy(t, entity, "has_filter_record") ||
        values::truthy(t, entity, "has_parent_id") ||
        values::truthy(t, pk, "is_compound"));
}

void derive_events(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto plural = values::truthy(t, entity, "entity_plural") ?
        values::py_str(t, values::member(t, entity, "entity_plural")) :
        values::py_str(t, values::member(t, entity, "entity_singular"), "") +
            "s";
    const auto prefix = values::py_str(t,
        values::member(t, entity, "component"), "") + ".v1.events." + plural;
    b.set(entity, "event_prefix", prefix);
    const auto events = b.list(entity, "events");
    for (const auto action : {"created", "updated", "deleted"}) {
        const auto e = b.append_object(events);
        b.set(e, "action", action);
        b.set(e, "subject", prefix + "." + action);
    }
}

}
