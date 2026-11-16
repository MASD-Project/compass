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

#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include "compass.codegen/ops/entity_registry.hpp"
#include "compass.codegen/ops/entity_steps.hpp"
#include "compass.codegen/ops/values.hpp"
#include "model_steps.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::value_id;

namespace {

bool is_word(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_' ||
        static_cast<unsigned char>(c) >= 0x80;
}

/*
 * (?<![\w:])([a-z][a-z0-9_]*)(?![\w:]): the lower-case names a C++ type
 * spells on their own, not as part of a qualified or longer name.
 */
std::vector<std::string_view> bare_names(std::string_view type) {
    std::vector<std::string_view> out;
    std::size_t i = 0;
    while (i < type.size()) {
        if (!is_word(type[i])) {
            ++i;
            continue;
        }
        const auto start = i;
        while (i < type.size() && is_word(type[i]))
            ++i;
        const auto name = type.substr(start, i - start);
        const bool preceded = start > 0 && type[start - 1] == ':';
        const bool followed = i < type.size() && type[i] == ':';
        if (!preceded && !followed && name[0] >= 'a' && name[0] <= 'z' &&
            std::ranges::all_of(name, [](char c) {
                return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                    c == '_';
            }))
            out.push_back(name);
    }
    return out;
}

/*
 * _batch_shape_includes: the headers declaring the shape of every element
 * type of the batch, and, for an element that is a field group of the
 * catalogue's directory, those of its fields' types, and so on down.
 */
std::vector<std::string> batch_shape_includes(builder& b, value_id catalogue,
    const types::entity_registry& registry, std::string_view model_path,
    const field_group_models& groups) {
    const auto& t = b.tables();
    const std::string component(values::py_str(t,
        values::member(t, catalogue, "component"), "None"));
    const auto owner = b.detached_object();
    b.set(owner, "component_include", component + ".api");
    const auto& names = directory_of(registry, model_path).group_names;
    const auto prefix = "ores." + component + ".";
    std::vector<std::string> out;
    std::set<std::string, std::less<>> seen;
    const auto visit = [&](this const auto& self,
        const std::vector<std::string>& cpp_types) -> void {
        for (auto& h : member_headers_of(b, owner, cpp_types, registry,
                 model_path)) {
            if (std::ranges::find(out, h) == out.end())
                out.push_back(std::move(h));
        }
        for (const auto& cpp_type : cpp_types) {
            for (const auto name : bare_names(cpp_type)) {
                if (!names.contains(name) || seen.contains(name))
                    continue;
                seen.emplace(name);
                const auto group = groups.find(prefix + std::string(name));
                if (group == groups.end())
                    throw std::runtime_error(std::string(model_path) +
                        ": no field group model " + prefix +
                        std::string(name));
                std::vector<std::string> fields;
                for (const auto f : values::items(b.tables(), group->second,
                         "fields"))
                    fields.emplace_back(values::str(b.tables(), f,
                        "cpp_type"));
                self(fields);
            }
        }
    };
    std::vector<std::string> elements;
    for (const auto item : values::items(t, catalogue, "batch"))
        elements.emplace_back(values::str(t, item, "cpp_type"));
    visit(elements);
    return out;
}

}

value_id trade_type_catalogue_context(builder& b, value_id library,
    value_id model, std::string_view model_key,
    const types::entity_registry& registry, const types::entity_site& site,
    const field_group_models& groups) {
    const auto [context, catalogue] = base_context(b, library, model,
        model_key, "trade_type_catalogue");
    if (catalogue.is_none())
        return context;
    const auto includes = batch_shape_includes(b, catalogue, registry,
        site.path, groups);
    const auto list = b.list(catalogue, "batch_shape_includes");
    for (const auto& h : includes)
        b.append(list, h);
    b.set_copy(context, "trade_type_catalogue", catalogue);
    return context;
}

}
