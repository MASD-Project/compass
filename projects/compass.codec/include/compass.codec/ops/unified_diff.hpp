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
#ifndef COMPASS_CODEC_OPS_UNIFIED_DIFF_HPP
#define COMPASS_CODEC_OPS_UNIFIED_DIFF_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace compass::codec::ops {

/**
 * @brief The unified diff of the file at @p path from @p before to
 * @p after, in the format of git diff: a ---/+++ header naming a/path and
 * b/path, or /dev/null for a side that is absent, then hunks with
 * @p context lines around each change, found by Myers' O(ND) algorithm on
 * lines. A last line without a newline is followed by "\ No newline at end
 * of file". When either side holds a NUL byte in its first 8000 bytes, as
 * git decides it, the diff is the single line "Binary files ... differ".
 *
 * @return empty when both sides are equal.
 */
[[nodiscard]] std::string unified_diff(std::string_view path,
    const std::optional<std::string_view>& before,
    const std::optional<std::string_view>& after, std::size_t context = 3);

}

#endif
