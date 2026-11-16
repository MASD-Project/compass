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
#include <set>
#include <string>
#include <vector>
#include "compass.codegen/ops/entity_registry.hpp"
#include "compass.codegen/ops/values.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::model_tables;
using compass::model::types::value_id;

namespace {

using prefix_map = std::map<std::string, std::string, std::less<>>;

std::string prefix_of(const prefix_map& prefixes, std::string_view column) {
    const auto it = prefixes.find(column);
    return it == prefixes.end() ? std::string() : it->second;
}

std::string facet_or_default(const types::parent_entity& p) {
    return p.generator_facet_name.empty() ? std::string("generators")
                                          : p.generator_facet_name;
}

bool has_change_reason_code(const types::parent_entity& p) {
    return p.column_prefixes.contains("change_reason_code") &&
        !p.no_audit_columns;
}

/*
 * Whether one of @p p's mandatory foreign keys references the party table.
 */
bool requires_party(const types::entity_registry& registry,
    const types::parent_entity& p) {
    for (const auto& m : p.mandatory_fks) {
        const auto it = registry.tables.find(m.table);
        if (it != registry.tables.end() &&
            it->second.entity_singular == "party")
            return true;
    }
    return false;
}

/*
 * One item of _plan_required_seeds: an ancestor row to seed before the row
 * that references it.
 */
struct seed_item {
    std::string var;
    std::string parent_var;
    const types::mandatory_foreign_key* fk;
    const types::parent_entity* ancestor;
    std::string owner_group_prefix;
};

void plan_required_seeds(const types::entity_registry& registry,
    const std::vector<types::mandatory_foreign_key>& mfks,
    const std::string& parent_var, const std::set<std::string>& path,
    const prefix_map& owner_prefixes, std::vector<seed_item>& items) {
    std::set<std::string> named;
    for (const auto& mfk : mfks) {
        const auto* ancestor = parent_of(registry, mfk.table);
        if (ancestor == nullptr || ancestor->entity_singular.empty() ||
            ancestor->entity_singular == "party" || path.contains(mfk.table))
            continue;
        auto var = parent_var + "_" + ancestor->entity_singular + "_parent";
        if (named.contains(var))
            var = parent_var + "_" + mfk.column + "_parent";
        named.insert(var);
        if (!mfk.use_system_tenant) {
            auto deeper = path;
            deeper.insert(mfk.table);
            plan_required_seeds(registry, ancestor->mandatory_fks, var, deeper,
                ancestor->column_prefixes, items);
        }
        items.push_back({var, parent_var, &mfk, ancestor,
            prefix_of(owner_prefixes, mfk.column)});
    }
}

void write_seed_item(builder& b, value_id list,
    const types::entity_registry& registry, const seed_item& s) {
    const auto& a = *s.ancestor;
    const auto o = b.append_object(list);
    b.set(o, "var", s.var);
    b.set(o, "column", s.fk->column);
    b.set(o, "table", s.fk->table);
    b.set(o, "use_system_tenant", s.fk->use_system_tenant);
    b.set(o, "parent_var", s.parent_var);
    b.set(o, "parent_entity_singular", a.entity_singular);
    b.set(o, "parent_component", a.component);
    b.set(o, "parent_component_include", a.component + ".api");
    b.set(o, "parent_component_core", a.component + ".core");
    b.set(o, "parent_has_identity_group", a.has_identity_group);
    b.set(o, "party_isolated", a.party_isolated);
    b.set(o, "requires_party", requires_party(registry, a));
    b.set(o, "parent_generator_facet_name", facet_or_default(a));
    b.set(o, "target_column", s.fk->target_column);
    b.set(o, "parent_has_audit_group", a.has_audit_group);
    b.set(o, "has_change_reason_code", has_change_reason_code(a));
    b.set(o, "audit_prefix", prefix_of(a.column_prefixes, "change_reason_code"));
    b.set(o, "party_prefix", prefix_of(a.column_prefixes, "party_id"));
    b.set(o, "owner_group_prefix", s.owner_group_prefix);
    b.set(o, "target_group_prefix", prefix_of(a.column_prefixes,
        s.fk->target_column.empty() ? std::string_view("id")
                                    : std::string_view(s.fk->target_column)));
}

/*
 * The group prefix of every column the entity declares, key columns
 * included, a later source winning.
 */
prefix_map group_prefix_by_column(const model_tables& t, value_id entity) {
    prefix_map out;
    auto sources = values::items(t, entity, "columns");
    const auto pk = values::items(t, values::member(t, entity, "primary_key"),
        "columns");
    sources.insert(sources.end(), pk.begin(), pk.end());
    const auto nks = values::items(t, entity, "natural_keys");
    sources.insert(sources.end(), nks.begin(), nks.end());
    for (const auto c : sources) {
        const auto name = values::member(t, c, "name");
        const auto n = values::truthy(t, name) ? values::py_str(t, name)
            : values::py_str(t, values::member(t, c, "column"));
        if (!n.empty())
            out[n] = values::truthy(t, c, "group_prefix")
                ? values::py_str(t, values::member(t, c, "group_prefix"))
                : std::string();
    }
    return out;
}

std::string_view table_of(const model_tables& t, value_id fk) {
    return values::str(t, fk, "table");
}

/*
 * The parent fields of a foreign key the eventing test seeds a parent for.
 */
void write_parent_seed(builder& b, value_id entity, value_id fk,
    const types::entity_registry& registry, const types::parent_entity& p) {
    const auto& t = b.tables();
    const std::string column(values::py_str(t, values::member(t, fk, "column"),
        "None"));
    b.set(fk, "parent_entity_singular", p.entity_singular);
    b.set(fk, "parent_component", p.component);
    b.set(fk, "parent_component_include", p.component + ".api");
    b.set(fk, "parent_component_core", p.component + ".core");
    b.set(fk, "parent_generator_facet_name", facet_or_default(p));
    b.set(fk, "parent_is_party", p.entity_singular == "party");
    b.set(fk, "parent_has_audit_group", p.has_audit_group);
    b.set(fk, "parent_has_change_reason_code", has_change_reason_code(p));
    b.set(fk, "parent_has_identity_group", p.has_identity_group);
    b.set(fk, "parent_audit_prefix",
        prefix_of(p.column_prefixes, "change_reason_code"));
    b.set(fk, "parent_party_prefix", prefix_of(p.column_prefixes, "party_id"));
    b.set(fk, "parent_tenant_prefix",
        prefix_of(p.column_prefixes, "tenant_id"));
    b.set(fk, "parent_parent_party_prefix",
        prefix_of(p.column_prefixes, "parent_party_id"));
    const auto target = values::truthy(t, fk, "target_column")
        ? values::py_str(t, values::member(t, fk, "target_column"))
        : std::string("id");
    b.set(fk, "parent_target_prefix", prefix_of(p.column_prefixes, target));
    b.set(fk, "parent_seed_country_sentinel", p.seed_country_sentinel);
    b.set(fk, "parent_seed_currency", p.seed_currency);

    const auto book = values::member(t, values::member(t, values::member(t,
        entity, "sql"), "party_id_from_book_id"), "book_table");
    const bool supplies_party = values::truthy(t, book) &&
        values::str(t, fk, "table") == values::str(t, book);
    b.set(fk, "parent_supplies_party", supplies_party);
    b.set(fk, "seed_parent_party_id",
        values::truthy(t, fk, "seed_parent_party_id"));
    b.set(fk, "parent_party_isolated", p.party_isolated && !supplies_party);
    b.set(fk, "parent_requires_party", requires_party(registry, p));

    std::vector<seed_item> items;
    plan_required_seeds(registry, p.mandatory_fks, column + "_parent", {},
        p.column_prefixes, items);
    const auto list = b.list(fk, "parent_required_fks");
    for (const auto& s : items)
        write_seed_item(b, list, registry, s);

    bool in_identity = false;
    for (const auto c : values::items(t, entity, "columns")) {
        in_identity = in_identity ||
            (values::str(t, c, "name") == column &&
             values::truthy(t, c, "is_identity_group_column"));
    }
    b.set(fk, "is_identity_group_column", in_identity);
}

bool any_required(const model_tables& t, value_id fk, std::string_view key) {
    for (const auto item : values::items(t, fk, "parent_required_fks")) {
        if (values::truthy(t, item, key))
            return true;
    }
    return false;
}

}

void derive_foreign_key_seeds(builder& b, value_id entity,
    const types::entity_registry& registry) {
    const auto& t = b.tables();
    const auto fks = values::items(t, entity, "foreign_keys");
    if (fks.empty())
        return;
    const auto prefixes = group_prefix_by_column(t, entity);
    for (const auto fk : fks) {
        const auto* parent = parent_of(registry, table_of(t, fk));
        const bool immutable = parent != nullptr && parent->immutable;
        b.set(fk, "target_immutable", immutable);
        if (immutable)
            b.set(fk, "target_no_validity", true);
        if (!values::truthy(t, fk, "enforce"))
            continue;
        b.set(fk, "matches_tenant", !values::truthy(t, fk, "use_no_tenant"));
        b.set(fk, "skip_check", true);
    }
    for (const auto fk : fks) {
        b.set(fk, "group_prefix",
            prefix_of(prefixes, values::str(t, fk, "column")));
        if (values::truthy(t, fk, "nullable") ||
            values::truthy(t, fk, "parent_seed_snippet") ||
            values::truthy(t, fk, "enforce"))
            continue;
        const auto* parent = parent_of(registry, table_of(t, fk));
        if (parent == nullptr || parent->entity_singular.empty())
            continue;
        write_parent_seed(b, entity, fk, registry, *parent);
    }

    bool system_tenant = false;
    bool seed_party = false;
    bool country_sentinel = false;
    bool currency = false;
    for (const auto fk : fks) {
        system_tenant = system_tenant ||
            values::truthy(t, fk, "use_system_tenant") ||
            any_required(t, fk, "use_system_tenant");
        seed_party = seed_party || values::truthy(t, fk, "parent_is_party") ||
            values::truthy(t, fk, "parent_requires_party") ||
            any_required(t, fk, "requires_party");
        country_sentinel = country_sentinel ||
            values::truthy(t, fk, "parent_seed_country_sentinel");
        currency = currency || values::truthy(t, fk, "parent_seed_currency");
    }
    b.set(entity, "has_system_tenant_fks", system_tenant);
    b.set(entity, "seed_party", seed_party);
    b.set(entity, "seed_parent_country_sentinel", country_sentinel);
    b.set(entity, "seed_parent_currency", currency);
}

void project_enforced_foreign_keys(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto fks = values::items(t, entity, "foreign_keys");
    if (fks.empty())
        return;
    const auto list = b.list(entity, "enforced_foreign_keys");
    for (const auto fk : fks) {
        if (values::truthy(t, fk, "enforce"))
            b.append_copy(list, fk);
    }
}

}
