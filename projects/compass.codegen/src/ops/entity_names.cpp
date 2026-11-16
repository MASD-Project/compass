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
#include "entity_text.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::value_id;

namespace {

/*
 * ' '.join(s.split()): the words of @p s, separated by one space.
 */
std::string one_line(std::string_view s) {
    std::string out;
    std::size_t at = 0;
    for (;;) {
        const auto start = s.find_first_not_of(" \t\n\r\f\v", at);
        if (start == std::string_view::npos)
            return out;
        const auto end = s.find_first_of(" \t\n\r\f\v", start);
        if (!out.empty())
            out += ' ';
        out += s.substr(start, end == std::string_view::npos
            ? std::string_view::npos : end - start);
        if (end == std::string_view::npos)
            return out;
        at = end;
    }
}

void set_default(builder& b, value_id entity, std::string_view key,
    const std::string& value) {
    if (values::member(b.tables(), entity, key).is_none())
        b.set(entity, key, value);
}

}

void format_description(builder& b, value_id entity) {
    const auto& t = b.tables();
    if (values::member(t, entity, "description").is_none())
        return;
    const std::string description(values::str(t, entity, "description"));
    b.set(entity, "description_formatted", text::comment_block(description));
    const auto lines = b.list(entity, "description_lines");
    for (const auto line : text::split(description, '\n'))
        b.append(lines, std::string(line));
    b.set(entity, "description_oneline", one_line(description));
}

void derive_component_names(builder& b, value_id entity) {
    const auto& t = b.tables();
    if (values::member(t, entity, "component").is_none())
        return;
    const std::string component(values::str(t, entity, "component"));
    const std::string sub(values::str(t, entity, "subcomponent"));
    b.set(entity, "component_upper", text::upper(component));
    if (!sub.empty()) {
        b.set(entity, "component_include", component + "." + sub);
        b.set(entity, "component_dir", "ores." + component + "/" + sub);
        b.set(entity, "component_core", component + ".core");
        b.set(entity, "component_core_dir", "ores." + component + "/core");
        b.set(entity, "component_service", component + ".service");
        b.set(entity, "component_service_dir",
            "ores." + component + "/service");
    } else {
        set_default(b, entity, "component_include", component);
        b.set(entity, "component_dir", "ores." + component);
        set_default(b, entity, "component_core", component);
        b.set(entity, "component_core_dir", "ores." + component);
        set_default(b, entity, "component_service", component);
        b.set(entity, "component_service_dir", "ores." + component);
    }
    for (const auto key : {"component_include", "component_core",
             "component_service"}) {
        b.set(entity, std::string(key) + "_upper",
            text::upper_snake(values::str(b.tables(), entity, key)));
    }
    b.set(entity, "component_is_database", component == "database");

    const std::string cached_by(
        values::member(t, entity, "cached_by").is_none()
            ? std::string_view(component)
            : values::str(t, entity, "cached_by"));
    const auto dot = cached_by.find('.');
    const auto cache_component = cached_by.substr(0, dot);
    const auto cache_subcomponent = dot == std::string::npos
        ? std::string("core") : cached_by.substr(dot + 1);
    b.set(entity, "cache_component", cache_component);
    b.set(entity, "cache_component_upper", text::upper(cache_component));
    b.set(entity, "cache_subcomponent", cache_subcomponent);
    b.set(entity, "cache_subcomponent_upper", text::upper(cache_subcomponent));
}

void derive_entity_names(builder& b, value_id entity) {
    const auto& t = b.tables();
    if (!values::member(t, entity, "entity_singular").is_none()) {
        const std::string singular(values::str(t, entity, "entity_singular"));
        const auto words = text::split(singular, '_');
        std::string pascal;
        for (const auto w : words)
            pascal += text::capitalize(w);
        const auto pascal_short = text::capitalize(words.back());
        b.set(entity, "entity_singular_upper", text::upper(singular));
        b.set(entity, "entity_singular_words", std::string(words.back()));
        b.set(entity, "entity_pascal", pascal);
        b.set(entity, "entity_snake", singular);
        b.set(entity, "entity_upper", text::upper(singular));
        b.set(entity, "entity_pascal_short", pascal_short);
        b.set(entity, "entity_pascal_short_plural", pascal_short + "s");
    }
    if (!values::member(t, entity, "entity_plural").is_none()) {
        const std::string plural(values::str(t, entity, "entity_plural"));
        b.set(entity, "entity_pascal_short_plural",
            text::capitalize(text::split(plural, '_').back()));
        b.set(entity, "entity_plural_upper", text::upper(plural));
        auto spaced = plural;
        for (auto& c : spaced) {
            if (c == '_')
                c = ' ';
        }
        b.set(entity, "entity_plural_words", spaced);
        b.set(entity, "entity_plural_words_cap", text::title(spaced));
    }
    if (!values::member(t, entity, "entity_title").is_none())
        b.set(entity, "entity_title_lower",
            text::lower(values::str(t, entity, "entity_title")));
}

}
