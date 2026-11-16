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
#include <vector>
#include "compass.codegen/ops/values.hpp"
#include "compass.model/ops/economic.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::value_id;

namespace {

bool reaches_no_member(const compass::model::types::model_tables& t,
    value_id column) {
    return values::truthy(t, column, "is_identity_group_column") ||
        values::truthy(t, column, "sql_only");
}

}

std::vector<value_id> entity_struct_member_columns(const builder& b,
    value_id entity) {
    const auto& t = b.tables();
    std::vector<value_id> members;
    if (values::truthy(t, entity, "has_domain_groups"))
        return members;
    if (!values::truthy(t, entity, "has_identity_group")) {
        for (const auto c : values::items(t,
                 values::member(t, entity, "primary_key"), "columns"))
            members.push_back(c);
        for (const auto c : values::items(t, entity, "natural_keys"))
            members.push_back(c);
    }
    for (const auto c : values::items(t, entity, "columns")) {
        if (!reaches_no_member(t, c))
            members.push_back(c);
    }
    return members;
}

void derive_economic_declaration(builder& b, value_id entity,
    const types::entity_registry& registry) {
    const auto& t = b.tables();
    const auto grouped = values::truthy(t, entity, "has_domain_groups");
    compass::model::ops::economic e{.declared = true, .members = {}};
    if (!grouped) {
        std::vector<value_id> carried;
        for (const auto c : values::items(t, entity, "columns")) {
            if (reaches_no_member(t, c))
                carried.push_back(c);
        }
        e = compass::model::ops::economic_declaration(b,
            values::str(t, entity, "entity_singular"),
            entity_struct_member_columns(b, entity), carried);
    }
    b.set(entity, "has_economic_declaration", e.declared);
    const auto fields = b.list(entity, "economic_fields");
    for (const auto& m : e.members)
        b.append(fields, m);
    b.set(entity, "has_economic_fields", !e.members.empty());
    if (!e.declared)
        return;

    const auto declares = [&](std::string_view group) {
        return registry.economic_field_groups.contains(group);
    };
    std::vector<std::string> canonical;
    if (grouped) {
        for (const auto g : values::items(t, entity, "domain_groups")) {
            if (declares(values::str(t, g, "field_group")))
                canonical.emplace_back(values::str(t, g, "member"));
        }
    } else {
        const auto identity = values::str(t, entity, "domain_identity_group");
        if (!identity.empty() && declares(identity))
            canonical.emplace_back("identity");
        canonical.insert(canonical.end(), e.members.begin(), e.members.end());
        const auto audit = values::str(t, entity, "domain_audit_group");
        if (!audit.empty() && declares(audit))
            canonical.emplace_back("audit");
    }
    const auto list = b.list(entity, "canonical_members");
    for (const auto& m : canonical)
        b.append(list, m);
    b.set(entity, "has_canonical_members", !canonical.empty());
}

}
