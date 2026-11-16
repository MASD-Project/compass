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
#include "compass.codegen/ops/values.hpp"
#include "compass.model/ops/typescript.hpp"
#include "entity_text.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::ops::pascal_case;
using compass::model::types::value_id;

namespace {

void set_ts_type(builder& b, value_id field,
    const compass::model::ops::custom_types& custom) {
    const auto mapped = compass::model::ops::ts_domain_type(
        values::str(b.tables(), field, "cpp_type"), custom);
    if (!mapped.empty())
        b.set(field, "ts_type", mapped);
}

/*
 * The last segment of a :: qualified name.
 */
std::string_view last_segment(std::string_view qualified) {
    const auto at = qualified.rfind("::");
    return at == std::string_view::npos ? qualified : qualified.substr(at + 2);
}

/*
 * _group_import: the module of a group named dotted or :: qualified, beside
 * the entity when its component owns it.
 */
std::string group_import(std::string_view qualified,
    std::string_view component) {
    std::string dotted(qualified);
    for (auto at = dotted.find("::"); at != std::string::npos;
         at = dotted.find("::", at + 1))
        dotted.replace(at, 2, ".");
    const auto parts = text::split(dotted, '.');
    const auto stem = parts.back();
    const auto owner = parts.size() > 1 ? parts[1] : component;
    if (owner == component)
        return "./" + std::string(stem) + ".js";
    return "../../" + std::string(owner) + "/domain/" + std::string(stem) +
        ".js";
}

}

void derive_ts_types(builder& b, value_id entity,
    const types::entity_registry& registry) {
    const auto& t = b.tables();
    const auto pk = values::member(t, entity, "primary_key");
    std::vector<value_id> fields = values::items(t, entity, "columns");
    std::ranges::copy(values::items(t, entity, "natural_keys"),
        std::back_inserter(fields));
    std::ranges::copy(values::items(t, pk, "columns"),
        std::back_inserter(fields));
    if (values::truthy(t, pk))
        fields.push_back(pk);
    for (const auto f : fields)
        set_ts_type(b, f, registry.custom_types);
}

void derive_ts_group_imports(builder& b, value_id entity) {
    const auto& t = b.tables();
    const std::string component(values::str(t, entity, "component"));
    for (const auto g : values::items(t, entity, "domain_groups")) {
        const std::string type(values::str(t, g, "type_qualified"));
        const std::string group(values::str(t, g, "field_group"));
        b.set(g, "ts_type", pascal_case(last_segment(type)));
        b.set(g, "ts_import", group_import(group, component));
    }
    if (values::truthy(t, entity, "has_identity_group")) {
        const std::string type(values::str(t, entity, "identity_group_type"));
        b.set(entity, "identity_group_pascal", pascal_case(type));
        b.set(entity, "identity_group_import", group_import(type, component));
    }
    if (values::truthy(t, entity, "has_audit_group")) {
        const std::string type(values::str(t, entity, "audit_group_qualified"));
        b.set(entity, "audit_group_pascal", pascal_case(last_segment(type)));
        b.set(entity, "audit_group_import", group_import(type, component));
    }
}

void derive_ts_column_imports(builder& b, value_id entity,
    const types::entity_registry& registry) {
    const auto& t = b.tables();
    std::vector<std::string> already;
    for (const auto g : values::items(t, entity, "domain_groups"))
        already.emplace_back(values::str(t, g, "ts_type"));
    for (const auto key : {"identity_group_pascal", "audit_group_pascal"}) {
        if (!values::member(t, entity, key).is_none())
            already.emplace_back(values::str(t, entity, key));
    }
    std::vector<std::string> cpp_types;
    for (const auto c : values::items(t, entity, "columns"))
        cpp_types.emplace_back(values::str(t, c, "cpp_type"));
    const auto imports = compass::model::ops::field_group_ts_imports(
        values::str(t, entity, "component"),
        values::str(t, entity, "entity_singular"), cpp_types,
        registry.custom_types);

    const auto list = b.list(entity, "ts_column_imports");
    for (const auto& i : imports) {
        if (std::ranges::find(already, i.pascal) != already.end())
            continue;
        const auto row = b.append_object(list);
        b.set(row, "pascal", i.pascal);
        b.set(row, "module", i.module);
    }
}

}
