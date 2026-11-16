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
#ifndef COMPASS_BENCH_TYPES_INCLUDE_TABLE_HPP
#define COMPASS_BENCH_TYPES_INCLUDE_TABLE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace compass::bench::types {

/**
 * @brief The two spellings of a header name.
 */
enum class include_kind : std::uint8_t { quoted, angle };

/**
 * @brief Target of an include found outside the checkout.
 */
inline constexpr std::uint32_t external_target = 0xFFFF'FFFF;

/**
 * @brief Target of an include found in no search directory.
 */
inline constexpr std::uint32_t unresolved_target = 0xFFFF'FFFE;

/**
 * @brief Every #include directive found, as parallel columns.
 *
 * Row i was written on line lines[i] of file id includers[i]. Its header
 * name is names[name_offsets[i], name_offsets[i] + name_sizes[i]).
 * targets is empty until resolution, then holds a file id, external_target
 * or unresolved_target per row. computed counts directives whose operand is
 * a macro rather than a header name; they have no row.
 */
struct include_table {
    std::vector<std::uint32_t> includers;
    std::vector<std::uint32_t> lines;
    std::vector<include_kind> kinds;
    std::vector<std::uint32_t> name_offsets;
    std::vector<std::uint32_t> name_sizes;
    std::string names;
    std::vector<std::uint32_t> targets;
    std::uint64_t computed = 0;

    [[nodiscard]] std::size_t size() const noexcept {
        return includers.size();
    }

    [[nodiscard]] std::string_view name(std::size_t i) const noexcept {
        return std::string_view(names).substr(name_offsets[i], name_sizes[i]);
    }
};

}

#endif
