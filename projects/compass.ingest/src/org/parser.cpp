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
#include "compass.ingest/org/parser.hpp"

#include <limits>
#include <stdexcept>
#include "context.hpp"

namespace compass::ingest::org {

namespace {

using detail::context;
namespace types = compass::org::types;

void clear(types::tables& t) {
    const auto reset = [](auto&... columns) { (columns.values.clear(), ...); };
    reset(t.elements.kinds, t.elements.post_blanks, t.elements.ranges,
        t.elements.contents, t.elements.parents, t.elements.subtree_ends,
        t.elements.object_firsts);
    reset(t.objects.kinds, t.objects.post_blanks, t.objects.ranges,
        t.objects.contents, t.objects.subtree_ends);
    reset(t.headings.elements, t.headings.levels, t.headings.todos,
        t.headings.todo_types, t.headings.priorities, t.headings.comments,
        t.headings.titles, t.headings.tag_firsts, t.headings.property_firsts);
    reset(t.heading_tags.names);
    reset(t.keywords.elements, t.keywords.names, t.keywords.values);
    reset(t.properties.elements, t.properties.names, t.properties.values);
    reset(t.affiliated.owners, t.affiliated.names, t.affiliated.options,
        t.affiliated.values, t.affiliated.object_firsts);
    reset(t.blocks.elements, t.blocks.names, t.blocks.parameters,
        t.blocks.bodies);
    reset(t.lists.elements, t.lists.types);
    reset(t.items.elements, t.items.checkboxes, t.items.tags);
    reset(t.links.objects, t.links.types, t.links.formats, t.links.paths);
    reset(t.names.firsts);
    t.name_bytes.clear();
    t.documents = {};
}

/**
 * @brief Adds the keywords of one #+todo: value to the vocabulary.
 *
 * The words after the bar are done states; with no bar the last word is.
 * A word may carry a fast-access key and logging flags in parentheses,
 * which are not part of the keyword.
 */
void add_sequence(context& c, std::string_view value) {
    const auto first = c.vocabulary.size();
    bool done = false;
    bool barred = false;
    std::size_t i = 0;
    while (i < value.size()) {
        while (i < value.size() && detail::is_ws(value[i]))
            ++i;
        std::size_t j = i;
        while (j < value.size() && !detail::is_ws(value[j]))
            ++j;
        auto word = value.substr(i, j - i);
        i = j;
        if (word.empty())
            continue;
        if (word == "|") {
            done = barred = true;
            continue;
        }
        if (word.back() == ')') {
            const auto open = word.find('(');
            if (open != std::string_view::npos)
                word = word.substr(0, open);
        }
        c.vocabulary.push_back({word,
            done ? types::todo_type::done : types::todo_type::todo});
    }
    if (!barred && c.vocabulary.size() > first)
        c.vocabulary.back().type = types::todo_type::done;
}

void build_vocabulary(context& c) {
    const auto& k = c.t.keywords;
    for (std::uint32_t i = 0; i < k.size(); ++i) {
        const types::keyword_id id{i};
        const auto name = c.name(k.names[id]);
        if (name == "todo" || name == "seq_todo" || name == "typ_todo") {
            const auto v = k.values[id];
            add_sequence(c, c.src.view(v.begin, v.end));
        }
    }
    if (c.vocabulary.empty()) {
        c.vocabulary.push_back({"TODO", types::todo_type::todo});
        c.vocabulary.push_back({"DONE", types::todo_type::done});
    }
}

}

void parse(std::string_view source, compass::org::types::tables& out) {
    if (source.size() >= std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("org document is 4 GiB or larger");
    clear(out);
    context c(source, out);
    detail::parse_elements(c);
    build_vocabulary(c);
    detail::parse_objects(c);
}

}
