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
#include "model_steps.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::value_id;

namespace {

std::string get_or(const builder& b, value_id model, std::string_view key,
    std::string_view fallback) {
    const auto v = values::member(b.tables(), model, key);
    return std::string(v.is_none() ? fallback : values::str(b.tables(), v));
}

}

void set_component_path_vars(builder& b, value_id model) {
    const auto component = get_or(b, model, "component", "unknown");
    const auto sub = get_or(b, model, "subcomponent", "");
    const auto own = "ores." + component;
    const bool split = !sub.empty();
    const auto include = split ? component + "." + sub
        : get_or(b, model, "component_include", component);
    const auto core = split ? component + ".core"
        : get_or(b, model, "component_core", component);
    const auto service = split ? component + ".service"
        : get_or(b, model, "component_service", component);
    const auto facet = get_or(b, model, "generator_facet_name", "generators");
    const auto cached_by = get_or(b, model, "cached_by", component);
    const auto dot = cached_by.find('.');
    const auto cache_component = cached_by.substr(0, dot);
    const auto cache_subcomponent = dot == std::string::npos
        ? std::string("core") : cached_by.substr(dot + 1);

    b.set(model, "component", component);
    b.set(model, "component_dir", split ? own + "/" + sub : own);
    b.set(model, "component_core_dir", split ? own + "/core" : own);
    b.set(model, "component_service_dir", split ? own + "/service" : own);
    b.set(model, "component_include", include);
    b.set(model, "component_core", core);
    b.set(model, "component_service", service);
    b.set(model, "cache_component_dir", "ores." + cache_component);
    b.set(model, "cache_component", cache_component);
    b.set(model, "cache_subcomponent", cache_subcomponent);
    b.set(model, "generator_facet_name", facet);
}

}
