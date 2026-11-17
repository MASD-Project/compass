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
 * @brief "\n".join(@p lines).
 */
[[nodiscard]] std::string join_lines(const std::vector<std::string>& lines);

}

#endif
