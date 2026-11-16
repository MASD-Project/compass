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
#ifndef COMPASS_ORG_TYPES_TYPE_REGISTRY_HPP
#define COMPASS_ORG_TYPES_TYPE_REGISTRY_HPP

#include <cstddef>
#include <cstdint>
#include <vector>
#include "compass.org/types/column.hpp"
#include "compass.org/types/handle.hpp"
#include "compass.org/types/name_table.hpp"

namespace compass::org::types {

using type_id = handle<struct type_tag>;
using type_entry_id = handle<struct type_entry_tag>;

/**
 * @brief Whether a type is a document the contract governs or a codegen
 * model.
 */
enum class type_kind : std::uint8_t { document, model };

/**
 * @brief The cybernetic level a contract assigns to a type; none when the
 * contract states no level or one the registry does not know.
 */
enum class type_level : std::uint8_t { none, s1, s2, s3, s4, s5, cross };

/**
 * @brief The registered types, one row per #+type: value, sorted by name.
 *
 * The rows are in byte order of their names, so a lookup is a binary search
 * and two names are equal when their handles are. folders is none when the
 * contract names no folder. The entries of t run from todo_firsts[t] up to
 * todo_firsts of the next row, or to the end of the entry table for the last
 * row: first the active TODO keywords, then from done_firsts[t] the done
 * keywords, then from section_firsts[t] the required section titles. A type
 * is stateful when it has any TODO keyword.
 */
struct type_table {
    column<type_id, name_id> names;
    column<type_id, type_kind> kinds;
    column<type_id, type_level> levels;
    column<type_id, name_id> folders;
    column<type_id, type_entry_id> todo_firsts;
    column<type_id, type_entry_id> done_firsts;
    column<type_id, type_entry_id> section_firsts;

    [[nodiscard]] std::size_t size() const noexcept { return names.size(); }
};

inline constexpr std::size_t type_row_bytes = row_bytes(&type_table::names,
    &type_table::kinds, &type_table::levels, &type_table::folders,
    &type_table::todo_firsts, &type_table::done_firsts,
    &type_table::section_firsts);

static_assert(type_row_bytes == 22);

/**
 * @brief The TODO keywords and section titles of every type, end to end.
 */
struct type_entry_table {
    column<type_entry_id, name_id> names;

    [[nodiscard]] std::size_t size() const noexcept { return names.size(); }
};

/**
 * @brief The type registry: the types, their entries and the interned
 * strings both refer to.
 *
 * Each distinct string has one row in names, as in the tables of a
 * document.
 */
struct type_registry {
    type_table types;
    type_entry_table entries;
    name_table names;
    std::vector<char> name_bytes;
};

}

#endif
