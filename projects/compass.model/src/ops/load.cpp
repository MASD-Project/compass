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
#include "compass.model/ops/load.hpp"

#include "compass.org/ops/document_fields.hpp"
#include "loaders.hpp"
#include "text.hpp"

namespace compass::model::ops {

using types::model_type;
using types::value_id;

std::optional<model_type> model_type_of(std::string_view type) noexcept {
    constexpr std::string_view prefix = "ores.codegen.";
    if (!type.starts_with(prefix))
        return std::nullopt;
    type.remove_prefix(prefix.size());
    for (const auto t : types::model_types) {
        if (types::name_of(t) == type)
            return t;
    }
    return std::nullopt;
}

std::string_view declared_type(const document& d) {
    const auto& t = *d.tables;
    const auto k = compass::org::ops::find_keyword(t, "type");
    if (k.is_none())
        return {};
    return trim(t.keywords.values[k].of(d.source));
}

std::string_view frontmatter_or(const outline& o, std::string_view key,
    std::string_view fallback) {
    const auto r = frontmatter(o, key);
    return r.found ? r.value : fallback;
}

std::string_view file_name(const document& d) noexcept {
    const auto slash = d.path.rfind('/');
    return slash == std::string_view::npos ? d.path : d.path.substr(slash + 1);
}

void collect_implementations(load_context& c, value_id parent,
    std::string_view key) {
    const auto& o = c.o;
    value_id out = value_id::none();
    for (std::size_t i = 0; i < o.block_nodes.size(); ++i) {
        const auto kind = o.block_implements[i];
        if (kind.empty())
            continue;
        if (out.is_none())
            out = c.b.object(parent, key);
        c.b.append(c.b.ensure_list(out, kind), o.block_codes[i]);
    }
}

types::model_id load_model(builder& b, const document& d, model_type t,
    const document_finder& find, std::string* error) {
    const types::model_id id{
        static_cast<std::uint32_t>(b.tables().models.size())};
    const auto root = b.add_model(d.path, t);
    outline o;
    build_outline(d.source, *d.tables, o);
    load_context c{b, d, o, find};
    try {
        switch (t) {
        case model_type::component:
            if (d.path.ends_with("component_overview.org"))
                load_component_overview(c, root);
            else
                load_component(c, root);
            break;
        case model_type::entity:
            load_entity(c, root);
            break;
        case model_type::field_group:
            load_field_group(c, root);
            break;
        default:
            throw model_error("model type not loaded yet: " +
                std::string(types::name_of(t)));
        }
    } catch (const model_error& e) {
        auto& v = b.tables().values;
        v.firsts[root] = value_id::none();
        v.lasts[root] = value_id::none();
        b.set(root, "error", "ValueError");
        if (error != nullptr)
            *error = e.what();
    }
    return id;
}

}
