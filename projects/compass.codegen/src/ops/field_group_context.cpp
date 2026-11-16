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
#include "compass.codegen/ops/model_contexts.hpp"

#include <string>
#include <vector>
#include "compass.codegen/ops/entity_steps.hpp"
#include "compass.codegen/ops/values.hpp"
#include "compass.model/ops/typescript.hpp"
#include "entity_text.hpp"
#include "model_steps.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::value_id;

namespace {

/*
 * The description as doxygen lines, " * " baked in and each line
 * right-stripped.
 */
void derive_description_lines(builder& b, value_id group) {
    const auto& t = b.tables();
    if (!values::truthy(t, group, "description"))
        return;
    const std::string description(values::str(t, group, "description"));
    const auto lines = b.list(group, "description_lines");
    for (const auto line : text::split(description, '\n')) {
        auto prefixed = " * " + std::string(line);
        prefixed.erase(prefixed.find_last_not_of(" \t\n\r\f\v") + 1);
        b.append(lines, prefixed);
    }
}

void derive_include_names(builder& b, value_id group) {
    const auto& t = b.tables();
    const auto component = values::member(t, group, "component").is_none()
        ? std::string("unknown")
        : std::string(values::str(t, group, "component"));
    const std::string sub(values::str(t, group, "subcomponent"));
    if (!sub.empty()) {
        b.set(group, "component_include", component + "." + sub);
        b.set(group, "component_dir", "ores." + component + "/" + sub);
    } else {
        if (values::member(t, group, "component_include").is_none())
            b.set(group, "component_include", component);
        b.set(group, "component_dir", "ores." + component);
    }
    b.set(group, "component_include_upper",
        text::upper_snake(values::str(b.tables(), group, "component_include")));
}

void derive_wire_facts(builder& b, value_id group,
    const types::entity_registry& registry, const types::entity_site& site) {
    const auto& t = b.tables();
    b.set(group, "has_generated_wire_codec", site.has_generated_wire_codec);
    std::int64_t left_out = 0;
    std::vector<std::string> cpp_types;
    for (const auto f : values::items(t, group, "fields")) {
        if (values::truthy(t, f, "no_wire"))
            ++left_out;
        cpp_types.emplace_back(values::str(t, f, "cpp_type"));
    }
    b.set_integer(group, "wire_left_out", left_out);
    const auto headers = member_headers_of(b, group, cpp_types, registry,
        site.path);
    const auto list = b.list(group, "wire_codec_includes");
    for (const auto& h : headers)
        b.append(list, h);
}

void derive_ts_names(builder& b, value_id group,
    const types::entity_registry& registry) {
    const auto& t = b.tables();
    const std::string singular(values::str(t, group, "entity_singular"));
    b.set(group, "entity_pascal", compass::model::ops::pascal_case(singular));
    std::vector<std::string> cpp_types;
    for (const auto f : values::items(t, group, "fields")) {
        const std::string cpp_type(values::str(b.tables(), f, "cpp_type"));
        cpp_types.push_back(cpp_type);
        const auto mapped = compass::model::ops::ts_domain_type(cpp_type,
            registry.custom_types);
        if (!mapped.empty())
            b.set(f, "ts_type", mapped);
    }
    const auto imports = compass::model::ops::field_group_ts_imports(
        values::str(b.tables(), group, "component"), singular, cpp_types,
        registry.custom_types);
    const auto list = b.list(group, "ts_imports");
    for (const auto& i : imports) {
        const auto row = b.append_object(list);
        b.set(row, "pascal", i.pascal);
        b.set(row, "module", i.module);
    }
}

}

value_id field_group_context(builder& b, value_id library, value_id model,
    std::string_view model_key, const types::entity_registry& registry,
    const types::entity_site& site) {
    const auto [context, group] = base_context(b, library, model, model_key,
        "field_group");
    if (group.is_none())
        return context;
    const auto& t = b.tables();
    derive_description_lines(b, group);
    derive_include_names(b, group);
    derive_wire_facts(b, group, registry, site);
    if (!values::member(t, group, "entity_singular").is_none())
        b.set(group, "entity_singular_upper",
            text::upper(values::str(b.tables(), group, "entity_singular")));
    const auto fields = values::items(b.tables(), group, "fields");
    if (!fields.empty())
        b.set(fields.back(), "last", true);
    derive_ts_names(b, group, registry);
    b.set_copy(context, "field_group", group);
    return context;
}

}
