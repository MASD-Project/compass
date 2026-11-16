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
#ifndef COMPASS_INGEST_ORG_CONTEXT_HPP
#define COMPASS_INGEST_ORG_CONTEXT_HPP

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "compass.org/types/tables.hpp"
#include "text.hpp"

namespace compass::ingest::org::detail {

namespace types = compass::org::types;

/**
 * @brief How the case of a name is normalised before it is interned.
 */
enum class name_case : std::uint8_t { as_written, lower, upper };

/**
 * @brief A TODO keyword of the document's vocabulary.
 */
struct todo_word {
    std::string_view text;
    types::todo_type type;
};

/**
 * @brief The state both passes of the parser share: the source, the tables
 * being filled and the name interner.
 */
struct context {
    context(std::string_view source, types::tables& tables) noexcept
        : src(source), t(tables) {}

    text src;
    types::tables& t;
    std::vector<todo_word> vocabulary;
    std::string scratch;

    [[nodiscard]] std::string_view name(types::name_id id) const noexcept {
        const auto first = t.names.firsts[id];
        const auto next = id.value + 1 < t.names.size()
            ? t.names.firsts.values[id.value + 1]
            : static_cast<std::uint32_t>(t.name_bytes.size());
        return {t.name_bytes.data() + first, next - first};
    }

    /**
     * @brief The handle of a name, added to the name table on first use.
     */
    [[nodiscard]] types::name_id intern(std::string_view text,
        name_case how = name_case::as_written) {
        if (how != name_case::as_written) {
            scratch.assign(text);
            for (auto& ch : scratch)
                ch = how == name_case::lower ? to_lower(ch) : to_upper(ch);
            text = scratch;
        }
        const auto count = static_cast<std::uint32_t>(t.names.size());
        for (std::uint32_t i = 0; i < count; ++i) {
            if (name(types::name_id{i}) == text)
                return types::name_id{i};
        }
        t.names.firsts.values.push_back(
            static_cast<std::uint32_t>(t.name_bytes.size()));
        t.name_bytes.insert(t.name_bytes.end(), text.begin(), text.end());
        return types::name_id{count};
    }

    /**
     * @brief Appends an element row; cbeg == cend stands for no contents.
     */
    types::element_id add_element(types::element_kind kind, pos begin,
        pos end, pos cbeg, pos cend, pos post_blank,
        types::element_id parent) {
        auto& e = t.elements;
        const types::element_id id{static_cast<std::uint32_t>(e.size())};
        e.kinds.values.push_back(kind);
        e.post_blanks.values.push_back(saturate(post_blank));
        e.ranges.values.push_back({begin, end});
        e.contents.values.push_back(
            cbeg < cend ? types::source_range{cbeg, cend}
                        : types::source_range{});
        e.parents.values.push_back(parent);
        e.subtree_ends.values.push_back(id.next());
        e.object_firsts.values.push_back(types::object_id{0});
        return id;
    }

    [[nodiscard]] static std::uint8_t saturate(pos count) noexcept {
        return static_cast<std::uint8_t>(std::min<pos>(count,
            types::post_blank_saturated));
    }
};

/**
 * @brief Fills the element tree and the element detail tables.
 */
void parse_elements(context& c);

/**
 * @brief Fills the objects and links of the elements already in the tables,
 * and the columns of the heading table that depend on the TODO vocabulary.
 */
void parse_objects(context& c);

}

#endif
