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
#ifndef COMPASS_PLATFORM_TEXT_DECODING_HPP
#define COMPASS_PLATFORM_TEXT_DECODING_HPP

#include <string>
#include <string_view>

/**
 * @brief How Python's read_text() turns the bytes of a file into text,
 * over UTF-8 bytes: which bytes it rejects, drops or replaces, and how it
 * reads line ends.
 */
namespace compass::platform::text {

/**
 * @brief True when @p text is UTF-8 that Python's strict decoder accepts:
 * no stray continuation byte, no overlong form, no surrogate and nothing
 * past U+10FFFF. A script that reads a file with read_text() skips or fails
 * on any other.
 */
[[nodiscard]] bool is_utf8(std::string_view text) noexcept;

/**
 * @brief @p text decoded with errors="ignore": each byte that does not
 * start a valid character dropped.
 */
[[nodiscard]] std::string decode_ignoring(std::string_view text);

/**
 * @brief @p text decoded with errors="replace": each byte that does not
 * start a valid character turned into U+FFFD.
 */
[[nodiscard]] std::string decode_replacing(std::string_view text);

/**
 * @brief @p text as Python's read_text() gives it: every \r\n and lone
 * \r turned into \n.
 */
[[nodiscard]] std::string universal_newlines(std::string_view text);

}

#endif
