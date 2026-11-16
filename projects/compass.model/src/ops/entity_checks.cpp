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
#include <algorithm>
#include <set>
#include <string>
#include "entity.hpp"
#include "text.hpp"

namespace compass::model::ops {

namespace {

using types::value_id;

std::vector<value_id> items(const builder& b, value_id list) {
    std::vector<value_id> out;
    if (list.is_none())
        return out;
    const auto& v = b.tables().values;
    for (auto i = v.firsts[list]; !i.is_none(); i = v.nexts[i])
        out.push_back(i);
    return out;
}

std::string field_group_path(load_context& c, std::string_view group) {
    const auto slash = c.d.path.rfind('/');
    const auto dir = slash == std::string_view::npos
        ? std::string() : std::string(c.d.path.substr(0, slash + 1));
    const auto beside = dir + std::string(group) + "_field_group.org";
    if (c.find(beside) || std::ranges::count(group, '.') != 2)
        return beside;
    const auto second = group.find('.', group.find('.') + 1);
    return "projects/" + std::string(group.substr(0, second)) + "/modeling/" +
        std::string(group) + "_field_group.org";
}

}

void resolve_domain_group_fields(load_context& c, value_id de) {
    auto& b = c.b;
    const auto groups = items(b, b.find(de, "domain_groups"));
    if (groups.empty())
        return;
    for (const auto older : {"domain_identity_group", "domain_audit_group"}) {
        if (truthy(b, b.find(de, older)))
            throw model_error("declares Domain groups and " +
                std::string(older));
    }
    const auto cpp = b.find(de, "cpp");
    const auto includes = cpp.is_none() ? cpp : b.find(cpp, "includes");
    if (!includes.is_none() && truthy(b, b.find(includes, "domain")))
        throw model_error("declares Domain groups and Domain includes");

    const auto resolved = b.list(de, "domain_group_fields");
    std::set<std::string> covered;
    for (const auto group : groups) {
        const std::string member(string_of(b, b.find(group, "member")));
        const std::string name(string_of(b, b.find(group, "field_group")));
        const auto path = field_group_path(c, name);
        const auto d = c.find(path);
        if (!d)
            throw model_error("domain group names a missing field group");
        outline fo;
        build_outline(d->source, *d->tables, fo);
        load_context fc{b, *d, fo, c.find};
        const auto holder = b.detached_object();
        load_field_group(fc, holder);
        const auto fg = b.find(holder, "field_group");
        for (const auto field : items(b, b.find(fg, "fields"))) {
            const auto field_name = text_value(b, b.find(field, "name"));
            const auto entry = b.append_object(resolved);
            b.set(entry, "name", member + "." + field_name);
            covered.insert(field_name);
            const auto cpp_type = b.find(field, "cpp_type");
            if (cpp_type.is_none())
                b.set(entry, "cpp_type", "");
            else
                b.set_copy(entry, "cpp_type", cpp_type);
            const auto def = b.find(field, "default_value");
            if (def.is_none())
                b.set(entry, "default_value", "");
            else
                b.set_copy(entry, "default_value", def);
        }
    }
    std::set<std::string> declared;
    for (const auto col : items(b, b.find(de, "columns")))
        declared.insert(text_value(b, b.find(col, "name")));
    if (const auto pk = b.find(de, "primary_key"); !pk.is_none()) {
        if (truthy(b, b.find(pk, "column")))
            declared.insert(text_value(b, b.find(pk, "column")));
        for (const auto col : items(b, b.find(pk, "columns"))) {
            const auto name = member_name(b, col);
            if (!name.empty())
                declared.emplace(name);
        }
    }
    for (const auto& name : declared) {
        if (!covered.contains(name))
            throw model_error("column " + name + " belongs to no domain group");
    }
}

void validate_entity(load_context& c, value_id de) {
    auto& b = c.b;
    for (const auto key : {"schema", "product", "component",
        "subcomponent"}) {
        if (std::string_view(key) == "subcomponent" &&
            (truthy(b, b.find(de, "no_subcomponent")) ||
             truthy(b, b.find(de, "sql_only"))))
            continue;
        if (b.find(de, key).is_none())
            throw model_error("Missing required flag: " + std::string(key));
    }
    const auto pk = b.find(de, "primary_key");
    if (truthy(b, b.find(de, "no_primary_key"))) {
        if (!pk.is_none())
            throw model_error("States :no_primary_key: and a primary key");
    } else if (pk.is_none()) {
        throw model_error("Missing primary key");
    } else if (b.find(pk, "column").is_none()) {
        throw model_error("Primary key missing required property: column");
    }
    const auto columns = items(b, b.find(de, "columns"));
    for (const auto col : columns) {
        if (b.find(col, "type").is_none())
            throw model_error("Column missing required property: type");
    }
    auto checked = columns;
    for (const auto col : items(b, b.find(de, "natural_keys")))
        checked.push_back(col);
    if (!pk.is_none()) {
        for (const auto col : items(b, b.find(pk, "columns")))
            checked.push_back(col);
    }
    constexpr std::string_view bare[] = {"bool", "int", "std::int64_t",
        "std::uint64_t", "float", "double", "std::chrono::year_month_day",
        "std::chrono::system_clock::time_point"};
    for (const auto col : checked) {
        const bool nullable = truthy(b, b.find(col, "nullable"));
        const bool has_default = !b.find(col, "default_value").is_none();
        if (truthy(b, b.find(col, "is_enum")) && !nullable && !has_default)
            throw model_error("Enum column has no :default_value:");
    }
    for (const auto col : checked) {
        const auto cpp_type = string_of(b, b.find(col, "cpp_type"));
        if (truthy(b, b.find(col, "nullable")) &&
            std::ranges::find(bare, cpp_type) != std::end(bare) &&
            b.find(col, "default_value").is_none())
            throw model_error("nullable bare column has no :default_value:");
    }
}

}
