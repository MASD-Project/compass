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
#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "compass.codegen/ops/values.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::model_tables;
using compass::model::types::value_id;

namespace {

/*
 * A dotted field group name, product.component.name; the loader refuses
 * any other shape.
 */
struct group_name {
    std::string product;
    std::string component;
    std::string name;

    std::string qualified() const {
        return product + "::" + component + "::domain::" + name;
    }
    std::string header() const {
        return "\"" + product + "." + component + ".api/domain/" + name +
            ".hpp\"";
    }
};

group_name parse_group(std::string_view dotted) {
    const auto first = dotted.find('.');
    const auto second = dotted.find('.', first + 1);
    return {std::string(dotted.substr(0, first)),
        std::string(dotted.substr(first + 1, second - first - 1)),
        std::string(dotted.substr(second + 1))};
}

/*
 * domain_group_fields split at their first dot: the member a field is
 * reached through, and the field's name.
 */
struct group_field {
    std::string member;
    std::string name;
};

std::vector<group_field> group_fields(const model_tables& t, value_id entity) {
    std::vector<group_field> out;
    for (const auto f : values::items(t, entity, "domain_group_fields")) {
        const auto name = values::str(t, f, "name");
        const auto dot = name.find('.');
        if (dot == std::string_view::npos)
            out.push_back({std::string(name), {}});
        else
            out.push_back({std::string(name.substr(0, dot)),
                std::string(name.substr(dot + 1))});
    }
    return out;
}

/*
 * _member_of: the prefix, "member.", a column is reached through, by
 * field name; the last field of a name wins.
 */
std::map<std::string, std::string, std::less<>> member_of(
    const model_tables& t, value_id entity) {
    std::map<std::string, std::string, std::less<>> out;
    for (const auto& f : group_fields(t, entity))
        out[f.name] = f.member + ".";
    return out;
}

std::string prefix_of(const std::map<std::string, std::string, std::less<>>& m,
    std::string_view name, std::string_view fallback) {
    const auto i = m.find(name);
    return i == m.end() ? std::string(fallback) : i->second;
}

/*
 * _set_group_prefix: whether the column is in the identity group, and the
 * member prefix it is reached through.
 */
void set_group_prefix(builder& b, value_id col,
    const std::map<std::string, std::string, std::less<>>& members,
    bool has_identity_group) {
    const auto& t = b.tables();
    auto name = std::string(values::str(t, col, "name"));
    if (name.empty())
        name = values::str(t, col, "column");
    const bool in_identity = has_identity_group &&
        values::str(t, col, "group") == "identity";
    const auto fallback = in_identity ? "identity." : "";
    b.set(col, "is_identity_group_column", in_identity);
    b.set(col, "group_prefix", name.empty() ? std::string(fallback)
        : prefix_of(members, name, fallback));
}

}

void derive_field_groups(builder& b, value_id entity) {
    const auto& t = b.tables();
    const std::string identity(values::str(t, entity, "domain_identity_group"));
    const std::string audit(values::str(t, entity, "domain_audit_group"));
    b.set(entity, "has_identity_group", !identity.empty());
    b.set(entity, "has_audit_group", !audit.empty());
    if (!identity.empty())
        b.set(entity, "identity_group_type", parse_group(identity).name);
    if (!audit.empty())
        b.set(entity, "audit_group_qualified", parse_group(audit).qualified());

    const std::string component(values::str(t, entity, "component"));
    const auto groups = values::items(t, entity, "domain_groups");
    for (const auto g : groups) {
        const auto name = parse_group(values::str(t, g, "field_group"));
        b.set(g, "type_qualified", name.component == component
            ? name.name : name.qualified());
        b.set(g, "header", name.header());
    }
    b.set(entity, "has_domain_groups", !groups.empty());
}

void derive_group_prefixes(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto members = member_of(t, entity);
    const bool identity = values::truthy(t, entity, "has_identity_group");
    const bool audit = values::truthy(t, entity, "has_audit_group");
    for (const auto c : values::items(t, entity, "columns"))
        set_group_prefix(b, c, members, identity);
    b.set(entity, "scaffold_prefix",
        prefix_of(members, "version", identity ? "identity." : ""));
    b.set(entity, "audit_prefix",
        prefix_of(members, "recorded_at", audit ? "audit." : ""));
}

void derive_stamp_members(builder& b, value_id entity) {
    const auto& t = b.tables();
    static constexpr std::array<std::string_view, 5> stamped{"tenant_id",
        "party_id", "modified_by", "performed_by", "change_reason_code"};
    std::map<std::string, std::set<std::string, std::less<>>, std::less<>>
        member_fields;
    for (const auto& f : group_fields(t, entity))
        member_fields[f.member].insert(f.name);

    std::vector<std::string> stamp;
    const auto groups = values::items(t, entity, "domain_groups");
    if (!groups.empty()) {
        for (const auto g : groups) {
            const std::string member(values::str(t, g, "member"));
            const auto i = member_fields.find(member);
            if (i != member_fields.end() && std::ranges::any_of(stamped,
                    [&](std::string_view s) { return i->second.contains(s); }))
                stamp.push_back(member);
        }
    } else {
        for (const auto key : {"scaffold_prefix", "audit_prefix"}) {
            auto name = std::string(values::str(t, entity, key));
            while (name.ends_with('.'))
                name.pop_back();
            if (!name.empty() && std::ranges::find(stamp, name) == stamp.end())
                stamp.push_back(name);
        }
    }

    const auto list = b.list(entity, "stamp_members");
    for (const auto& m : stamp)
        b.set(b.append_object(list), "member", m);
    b.set(entity, "has_stamp_members", !stamp.empty());
}

void derive_key_group_prefixes(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto members = member_of(t, entity);
    const bool identity = values::truthy(t, entity, "has_identity_group");
    const auto pk = values::member(t, entity, "primary_key");
    if (!pk.is_none())
        set_group_prefix(b, pk, members, identity);
    for (const auto c : values::items(t, pk, "columns"))
        set_group_prefix(b, c, members, identity);
    for (const auto k : values::items(t, entity, "natural_keys"))
        set_group_prefix(b, k, members, identity);
}

void inject_group_includes(builder& b, value_id entity) {
    const auto& t = b.tables();
    const std::string identity(values::str(t, entity, "domain_identity_group"));
    const std::string audit(values::str(t, entity, "domain_audit_group"));
    const auto groups = values::items(t, entity, "domain_groups");
    if (identity.empty() && audit.empty() && groups.empty())
        return;

    std::vector<std::string> injected;
    if (!audit.empty())
        injected.push_back(parse_group(audit).header());
    if (!identity.empty())
        injected.push_back(parse_group(identity).header());
    for (const auto g : groups)
        injected.emplace_back(values::str(t, g, "header"));
    std::ranges::sort(injected);

    const auto includes = b.ensure_object(b.ensure_object(entity, "cpp"),
        "includes");
    const auto existing = b.detached_list();
    for (const auto v : values::items(b.tables(), includes, "domain"))
        b.append_copy(existing, v);
    const auto domain = b.list(includes, "domain");
    for (const auto& h : injected)
        b.append(domain, h);
    for (const auto v : values::items(b.tables(), existing))
        b.append_copy(domain, v);
}

}
