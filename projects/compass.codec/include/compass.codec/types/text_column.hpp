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
#ifndef COMPASS_CODEC_TYPES_TEXT_COLUMN_HPP
#define COMPASS_CODEC_TYPES_TEXT_COLUMN_HPP

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>
#include "compass.org/types/column.hpp"

namespace compass::codec::types {

/**
 * @brief One column of byte strings: the bytes of every row end to end.
 *
 * The bytes of row h run from firsts[h] up to firsts of the next row, or up
 * to the end of bytes for the last row. Offset bounds the bytes the column
 * can hold.
 */
template <typename Handle, std::unsigned_integral Offset = std::uint32_t>
struct text_column {
    compass::org::types::column<Handle, Offset> firsts;
    std::vector<char> bytes;

    [[nodiscard]] std::size_t size() const noexcept { return firsts.size(); }

    [[nodiscard]] std::string_view operator[](Handle h) const noexcept {
        const std::size_t first = firsts[h];
        const std::size_t end = h.next().value < firsts.size()
            ? firsts[h.next()] : bytes.size();
        return {bytes.data() + first, end - first};
    }

    void add(std::string_view text) {
        firsts.values.push_back(static_cast<Offset>(bytes.size()));
        bytes.insert(bytes.end(), text.begin(), text.end());
    }

    /**
     * @brief Adds the rows of @p other after the rows of this column.
     */
    void append(const text_column& other) {
        const auto shift = static_cast<Offset>(bytes.size());
        for (const auto first : other.firsts.values)
            firsts.values.push_back(first + shift);
        bytes.insert(bytes.end(), other.bytes.begin(), other.bytes.end());
    }
};

}

#endif
