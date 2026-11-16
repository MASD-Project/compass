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
#ifndef COMPASS_CODEGEN_TYPES_COMPILED_TEMPLATE_HPP
#define COMPASS_CODEGEN_TYPES_COMPILED_TEMPLATE_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include "compass.org/types/column.hpp"
#include "compass.org/types/handle.hpp"

namespace compass::codegen::types {

using compass::org::types::column;

using node_id = compass::org::types::handle<struct node_tag>;

/**
 * @brief What a node of a compiled template does. Comments and delimiter
 * changes render nothing, so compiling drops them.
 */
enum class node_kind : std::uint8_t {
    /** Copies its range of the source. */
    text,
    /** {{name}}: the value of name, escaped. */
    escaped,
    /** {{{name}}} or {{&name}}: the value of name as it is. */
    literal,
    /** {{#name}}: its body once for each item name gives. */
    section,
    /** {{^name}}: its body once when name is falsy. */
    inverted,
    /** {{>name}}: the partial name, indented. */
    partial
};

/**
 * @brief A Mustache template parsed once: its nodes in document order.
 *
 * A node's range is a range of the source: the text a text node copies, or
 * the name a tag looks up. The body of a section or an inverted section is
 * the nodes after it up to its end; any other node's end is the next node.
 * A partial's indent is the whitespace its standalone tag stood after.
 */
struct compiled_template {
    std::string source;
    column<node_id, node_kind> kinds;
    column<node_id, std::uint32_t> firsts;
    column<node_id, std::uint32_t> sizes;
    column<node_id, node_id> ends;
    column<node_id, std::uint32_t> indent_firsts;
    column<node_id, std::uint32_t> indent_sizes;

    [[nodiscard]] std::size_t size() const noexcept { return kinds.size(); }

    [[nodiscard]] std::string_view text(node_id n) const noexcept {
        return std::string_view(source).substr(firsts[n], sizes[n]);
    }

    [[nodiscard]] std::string_view indent(node_id n) const noexcept {
        return std::string_view(source).substr(indent_firsts[n],
            indent_sizes[n]);
    }
};

}

#endif
