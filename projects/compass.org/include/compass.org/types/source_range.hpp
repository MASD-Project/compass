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
#ifndef COMPASS_ORG_TYPES_SOURCE_RANGE_HPP
#define COMPASS_ORG_TYPES_SOURCE_RANGE_HPP

#include <cstdint>
#include <string_view>

namespace compass::org::types {

/**
 * @brief Half-open byte range [begin, end) in the source of one document.
 *
 * Text is never copied into the tables: a title, a value or a body is the
 * range it occupies in the source. An empty range stands for absent text.
 */
struct source_range {
    std::uint32_t begin = 0;
    std::uint32_t end = 0;

    [[nodiscard]] constexpr std::uint32_t size() const noexcept {
        return end - begin;
    }

    [[nodiscard]] constexpr bool empty() const noexcept {
        return begin == end;
    }

    [[nodiscard]] constexpr std::string_view of(
        std::string_view source) const noexcept {
        return source.substr(begin, size());
    }

    friend constexpr bool operator==(source_range, source_range) noexcept =
        default;
};

static_assert(sizeof(source_range) == 8);

}

#endif
