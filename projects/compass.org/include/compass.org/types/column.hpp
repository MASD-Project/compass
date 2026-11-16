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
#ifndef COMPASS_ORG_TYPES_COLUMN_HPP
#define COMPASS_ORG_TYPES_COLUMN_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

namespace compass::org::types {

/**
 * @brief What a column holds: a value with no pointer and no destructor, so
 * its array can be written to an object and mapped back as it is.
 */
template <typename Value>
concept column_value = std::is_trivially_copyable_v<Value>;

/**
 * @brief Read-only run of one column, indexed by the handle of its table.
 *
 * It is the form a column takes when it is not owned: the rows of a parsed
 * document seen through a view, one document's rows inside a corpus column,
 * or a column mapped from an object.
 */
template <typename Handle, column_value Value>
struct column_view {
    std::span<const Value> values;

    [[nodiscard]] const Value& operator[](Handle h) const noexcept {
        return values[h.value];
    }

    [[nodiscard]] std::size_t size() const noexcept { return values.size(); }
};

/**
 * @brief One column of a table: a value per row, indexed by the table's
 * handle.
 *
 * Values are trivially copyable and hold no pointer, so the array can be
 * written to a pack and mapped back as it is.
 */
template <typename Handle, column_value Value>
struct column {
    std::vector<Value> values;

    [[nodiscard]] const Value& operator[](Handle h) const noexcept {
        return values[h.value];
    }

    [[nodiscard]] Value& operator[](Handle h) noexcept {
        return values[h.value];
    }

    [[nodiscard]] std::size_t size() const noexcept { return values.size(); }

    /**
     * @brief The rows [first, first + count), indexed from zero.
     */
    [[nodiscard]] column_view<Handle, Value> slice(std::size_t first,
        std::size_t count) const noexcept {
        return {std::span<const Value>(values).subspan(first, count)};
    }
};

/**
 * @brief Calls f once per column of a table, in the order the table lists
 * them.
 *
 * Given several tables of one shape, f gets the same column of each: this
 * is how a view is pointed at owned columns or at a mapped object.
 */
template <typename F, typename Table, typename... Tables>
constexpr void for_each_column(F&& f, Table&& first, Tables&&... rest) {
    std::remove_cvref_t<Table>::for_each_column(f, first, rest...);
}

/**
 * @brief Bytes one row takes across the columns of a table.
 *
 * Fails to compile unless every column of the table is listed and all of
 * them are indexed by the same handle.
 */
template <typename Table, typename Handle, typename... Values>
[[nodiscard]] consteval std::size_t row_bytes(
    column<Handle, Values> Table::*...) noexcept {
    static_assert(sizeof(Table) ==
        sizeof...(Values) * sizeof(column<Handle, std::uint8_t>),
        "list every column of the table");
    return (sizeof(Values) + ...);
}

}

#endif
