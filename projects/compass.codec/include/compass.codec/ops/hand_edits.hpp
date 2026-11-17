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
#ifndef COMPASS_CODEC_OPS_HAND_EDITS_HPP
#define COMPASS_CODEC_OPS_HAND_EDITS_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace compass::codec::ops {

/**
 * @brief A block of a file that a write would delete: the 1-based line it
 * starts on, how many non-blank lines it holds and the first of them,
 * trimmed.
 */
struct dropped_block {
    std::uint32_t line = 0;
    std::uint32_t lines = 0;
    std::string first;
};

/**
 * @brief Fewest non-blank lines a block must hold to be reported.
 */
inline constexpr std::uint32_t dropped_block_minimum = 3;

/**
 * @brief The block of @p on_disk that writing @p generated over it deletes
 * outright, when it looks like code someone wrote by hand; else nothing.
 *
 * The two are compared line by line with the diff compass diff prints,
 * with no context, so each hunk is one place where the file changes. A
 * hunk's weight is the non-blank lines it removes that @p generated holds
 * nowhere, trimmed of surrounding white space, less the non-blank lines
 * it adds. The heaviest hunk is reported when it weighs at least
 * dropped_block_minimum: a block the output drops, as when it lacks a
 * function written into a generated file. A block the output rewrites
 * with as many lines of its own, as a template change does, weighs
 * nothing, and neither do lines it only moves or reindents. lines is the
 * weight; line and first name the first line the hunk removes that counts.
 * The cost is the diff and a hash set of the generated lines.
 */
[[nodiscard]] std::optional<dropped_block> dropped_hand_lines(
    std::string_view on_disk, std::string_view generated);

}

#endif
