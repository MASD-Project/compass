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
#ifndef COMPASS_DOCS_OPS_REGIONS_HPP
#define COMPASS_DOCS_OPS_REGIONS_HPP

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace compass::docs::ops {

/**
 * @brief Which regions between a pair of markers a generator replaces:
 * the first, as re.search and a splice do, or every one, as re.sub does.
 */
enum class replace : std::uint8_t { first, every };

/**
 * @brief @p text with the region from @p begin to the nearest @p end after
 * it, markers included, replaced by @p block; nothing when no region is
 * found.
 */
[[nodiscard]] std::optional<std::string> replace_region(std::string_view text,
    std::string_view begin, std::string_view end, std::string_view block,
    replace which);

/**
 * @brief What a marker names as its writer when compass writes the
 * region: this, then the codec's name.
 */
inline constexpr std::string_view compass_writer = "compass generate: ";

/**
 * @brief A region a codec writes between "# BEGIN generated kind (compass
 * generate: codec)" and "# END generated kind". former holds the BEGIN
 * markers pages may still carry from the scripts that wrote the region
 * before compass did.
 */
struct generated_region {
    std::string_view kind;
    std::string_view codec;
    std::span<const std::string_view> former;
};

/**
 * @brief The BEGIN marker compass writes for @p r.
 */
[[nodiscard]] std::string begin_marker(const generated_region& r);

/**
 * @brief The END marker of @p r.
 */
[[nodiscard]] std::string end_marker(const generated_region& r);

/**
 * @brief @p text with the regions of @p r, found under its BEGIN marker or
 * any former one, replaced by @p block as replace_region replaces them;
 * nothing when no region is found. With replace::first only the first
 * marker that finds a region is used.
 */
[[nodiscard]] std::optional<std::string> replace_region(std::string_view text,
    const generated_region& r, std::string_view block, replace which);

/**
 * @brief "\n".join(@p lines).
 */
[[nodiscard]] std::string join_lines(const std::vector<std::string>& lines);

}

#endif
