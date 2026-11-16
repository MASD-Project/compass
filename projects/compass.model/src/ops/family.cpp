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
#include <map>
#include <string>
#include <vector>
#include "loaders.hpp"
#include "text.hpp"

namespace compass::model::ops {

namespace {

using types::value_id;

/**
 * @brief One row of * Parts: a member name, its entity, its cardinality
 * and, for one_of, its group.
 */
struct part {
    std::string_view name;
    std::string_view entity;
    std::string_view cardinality;
    std::string_view group;
};

std::vector<part> parts_of(const outline& o, node_id document) {
    const auto section_node = section(o, document, "Parts");
    std::vector<part> out;
    if (section_node.is_none())
        return out;
    for (const auto& row : first_table_rows(o, section_node)) {
        const part p{trim(row.value_or("part")), trim(row.value_or("entity")),
            trim(row.value_or("cardinality")), trim(row.value_or("group"))};
        if (p.name.empty() || p.entity.empty())
            throw model_error("every row of '* Parts' must name a part and an "
                "entity");
        if (std::ranges::find(out, p.name, &part::name) != out.end())
            throw model_error("the part '" + std::string(p.name) +
                "' is listed more than once");
        if (p.cardinality != "one" && p.cardinality != "optional" &&
            p.cardinality != "one_of")
            throw model_error("the part '" + std::string(p.name) +
                "' must state its cardinality as one, optional or one_of");
        if ((p.cardinality == "one_of") != !p.group.empty())
            throw model_error("the part '" + std::string(p.name) +
                "' must name a group exactly when its cardinality is one_of");
        out.push_back(p);
    }
    return out;
}

/**
 * @brief _part_cardinality_view: the parts each element fills once, the
 * ones it fills at most once, and the groups of alternatives, in the
 * order each group is first named.
 */
void write_cardinality_view(builder& b, value_id family,
    const std::vector<part>& parts) {
    std::vector<std::pair<std::string_view, std::vector<std::string_view>>>
        groups;
    for (const auto& p : parts) {
        if (p.cardinality != "one_of")
            continue;
        auto it = std::ranges::find(groups, p.group,
            &decltype(groups)::value_type::first);
        if (it == groups.end())
            it = groups.insert(groups.end(), {p.group, {}});
        it->second.push_back(p.name);
    }
    for (const auto& [name, members] : groups) {
        if (members.size() < 2)
            throw model_error("the group '" + std::string(name) +
                "' names one part; a group needs two or more");
    }
    for (const auto cardinality : {"one", "optional"}) {
        const auto list = b.list(family,
            "cardinality_" + std::string(cardinality));
        for (const auto& p : parts) {
            if (p.cardinality == cardinality)
                b.set(b.append_object(list), "part", p.name);
        }
    }
    const auto list = b.list(family, "cardinality_groups");
    for (const auto& [name, members] : groups) {
        const auto g = b.append_object(list);
        std::string label;
        for (const auto m : members)
            label += (label.empty() ? "" : " or ") + std::string(m);
        b.set(g, "label", label);
        const auto terms = b.list(g, "terms");
        for (std::size_t i = 0; i < members.size(); ++i) {
            const auto t = b.append_object(terms);
            b.set(t, "part", members[i]);
            b.set(t, "last", i + 1 == members.size());
        }
    }
}

}

void load_family(load_context& c, value_id root) {
    auto& b = c.b;
    const auto& o = c.o;
    const node_id document{0};
    const auto family = b.object(root, "family");
    for (const auto key : {"product", "component", "subcomponent",
        "entity_singular", "entity_plural", "entity_title", "brief"}) {
        if (const auto r = frontmatter(o, key); r.found)
            b.set(family, key, r.value);
    }
    if (const auto text = body(o, document); !text.empty())
        b.set(family, "description", text);
    if (trim(frontmatter_or(o, "component")).empty())
        throw model_error("a family model must declare #+component:");
    const auto central = trim(property(o, section(o, document, "Flags"),
        "central_entity").value);
    if (central.empty())
        throw model_error("a family model must name its central row with "
            "':central_entity:' in '* Flags'");
    b.set(family, "central_entity", central);
    const auto parts = parts_of(o, document);
    const auto list = b.list(family, "parts");
    for (const auto& p : parts) {
        const auto row = b.append_object(list);
        b.set(row, "part", p.name);
        b.set(row, "entity", p.entity);
        b.set(row, "cardinality", p.cardinality);
        b.set(row, "group", p.group);
    }
    write_cardinality_view(b, family, parts);
}

}
