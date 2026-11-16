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
#ifndef COMPASS_SQL_OPS_SCRIPTS_HPP
#define COMPASS_SQL_OPS_SCRIPTS_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief The helpers ORE Studio's SQL gate scripts share, over the text of
 * one script as read_text() gives it.
 */
namespace compass::sql::ops {

/**
 * @brief @p text with its -- and block comments turned into blanks, byte
 * for byte, newlines kept. A single-quoted literal, with '' inside it, is
 * not searched for comments; block comments do not nest.
 */
[[nodiscard]] std::string blank_comments(std::string_view text);

/**
 * @brief @p text split on @p separator at bracket depth zero, outside
 * single- and double-quoted strings, each part stripped.
 */
[[nodiscard]] std::vector<std::string_view> split_top(std::string_view text,
    char separator = ',');

/**
 * @brief @p text without the ASCII spaces at either end.
 */
[[nodiscard]] std::string_view strip(std::string_view text) noexcept;

/**
 * @brief The first run of non-spaces of @p text, as str.split()[0].
 */
[[nodiscard]] std::string_view first_word(std::string_view text) noexcept;

/**
 * @brief True when @p a sorts before @p b as pathlib sorts paths: by their
 * components, in turn.
 */
[[nodiscard]] bool component_less(std::string_view a,
    std::string_view b) noexcept;

/**
 * @brief The offset of the first byte of 1-based line @p line, counting
 * newlines; the size of @p text when it has fewer lines.
 */
[[nodiscard]] std::uint32_t offset_of_line(std::string_view text,
    std::uint32_t line) noexcept;

}

#endif
