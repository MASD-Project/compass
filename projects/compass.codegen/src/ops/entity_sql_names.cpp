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
#include <string>
#include <vector>
#include "compass.codegen/ops/entity_registry.hpp"
#include "compass.codegen/ops/values.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::model_tables;
using compass::model::types::value_id;
using compass::model::types::value_kind;

namespace {

/*
 * PostgreSQL truncates identifiers to this many bytes.
 */
constexpr std::size_t identifier_limit = 63;

std::string stated_or(const model_tables& t, value_id object,
    std::string_view key, std::string_view fallback) {
    return values::py_str(t, values::member(t, object, key), fallback);
}

/*
 * The longest suffix composed onto the base: _tbl, _insert_fn,
 * _insert_trg, _delete_fn and _delete_rule always, _touch_version_fn and
 * _hierarchy_fn when the entity has them.
 */
std::size_t longest_suffix(const model_tables& t, value_id entity) {
    std::size_t longest = 12;
    if (values::truthy(t, entity, "generate_touch_function"))
        longest = std::max<std::size_t>(longest, 17);
    if (values::truthy(t, entity, "has_parent_id"))
        longest = std::max<std::size_t>(longest, 13);
    return longest;
}

std::string quoted_list(const std::vector<std::string>& names) {
    std::string out;
    for (const auto& n : names)
        out += (out.empty() ? "\"" : ", \"") + n + "\"";
    return out;
}

std::vector<std::string> strings_of(const model_tables& t, value_id list) {
    std::vector<std::string> out;
    for (const auto v : values::items(t, list))
        out.push_back(values::py_str(t, v));
    return out;
}

}

void derive_sql_names(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto sql = values::member(t, entity, "sql");
    const auto product = stated_or(t, entity, "product", "ores");
    const auto component = stated_or(t, entity, "component", "unknown");
    const std::string table(values::str(t, sql, "tablename"));
    std::string base;
    if (table.ends_with("_tbl"))
        base = table.substr(0, table.size() - 4);
    else if (!table.empty())
        base = table;
    else
        base = product + "_" + component + "_" +
            stated_or(t, entity, "entity_plural", "unknown");
    const auto suffix = longest_suffix(t, entity);
    if (base.size() + suffix > identifier_limit)
        base.resize(identifier_limit - suffix);
    b.set(entity, "sql_name_base", base);

    for (const auto fk : values::items(t, entity, "foreign_keys")) {
        if (!values::truthy(t, fk, "enforce"))
            continue;
        if (!values::truthy(t, fk, "constraint_name"))
            b.set(fk, "constraint_name",
                base + "_" + stated_or(t, fk, "column", "None") + "_fk");
    }

    auto rls_base = base;
    const auto prefix = product + "_" + component + "_";
    if (rls_base.starts_with(prefix))
        rls_base = rls_base.substr(prefix.size());
    constexpr std::size_t rls_suffix =
        std::string_view("_tbl_tenant_isolation_policy").size();
    if (values::truthy(t, sql, "rls_tenant_isolation") &&
        rls_base.size() + rls_suffix > identifier_limit)
        rls_base.resize(identifier_limit - rls_suffix);
    b.set(entity, "rls_table_base", rls_base);
}

void resolve_pinned_keys(builder& b, value_id entity,
    const types::entity_registry& registry) {
    const auto& t = b.tables();
    const auto pins = values::items(t, entity, "pinned_keys");
    if (pins.empty())
        return;
    const bool has_tenant = values::truthy(t, entity, "has_tenant_id");
    const std::string base(values::str(t, entity, "sql_name_base"));
    for (const auto pin : pins) {
        const auto columns = strings_of(t, values::member(t, pin, "columns"));
        const auto targets = strings_of(t,
            values::member(t, pin, "target_columns"));
        const auto* parent = parent_of(registry, values::str(t, pin, "table"));
        b.set(pin, "matches_tenant", has_tenant);
        b.set(pin, "enforced", parent != nullptr && parent->immutable);
        const auto pairs = b.list(pin, "column_pairs");
        for (std::size_t i = 0; i < std::min(columns.size(), targets.size());
             ++i) {
            const auto o = b.append_object(pairs);
            b.set(o, "column", columns[i]);
            b.set(o, "target_column", targets[i]);
        }
        if (!columns.empty())
            b.set(pin, "first_column", columns.front());
        std::vector<std::string> tenant;
        if (has_tenant)
            tenant.emplace_back("tenant_id");
        auto own = tenant;
        own.insert(own.end(), columns.begin(), columns.end());
        auto theirs = tenant;
        theirs.insert(theirs.end(), targets.begin(), targets.end());
        b.set(pin, "column_list", quoted_list(own));
        b.set(pin, "target_column_list", quoted_list(theirs));
        b.set(pin, "constraint_name",
            base + "_" + stated_or(t, pin, "name", "None") + "_pin");
    }
    const auto enforced = b.list(entity, "enforced_pinned_keys");
    const auto checked = b.list(entity, "checked_pinned_keys");
    for (const auto pin : pins)
        b.append_copy(values::truthy(t, pin, "enforced") ? enforced : checked,
            pin);
}

void resolve_routed_check(builder& b, value_id entity,
    const types::entity_registry& registry) {
    const auto& t = b.tables();
    const auto sql = values::member(t, entity, "sql");
    if (!values::truthy(t, sql, "routed_by") ||
        !values::truthy(t, sql, "routed_column"))
        return;
    const auto routes = registry.trade_type_routes.find(
        values::str(t, sql, "routed_by"));
    if (routes == registry.trade_type_routes.end())
        return;
    const auto entity_singular = values::str(t, entity, "entity_singular");
    std::string listed;
    for (const auto& r : routes->second) {
        if (r.instrument == entity_singular)
            listed += (listed.empty() ? "'" : ", '") + r.code + "'";
    }
    const auto check = "\"" + std::string(values::str(t, sql, "routed_column")) +
        "\" in (" + listed + ")";

    const auto stated = b.detached_list();
    for (const auto c : values::items(t, sql, "extra_checks"))
        b.append_copy(stated, c);
    const auto checks = b.list(sql, "extra_checks");
    b.append(checks, check);
    for (const auto c : values::items(t, stated))
        b.append_copy(checks, c);
}

}
