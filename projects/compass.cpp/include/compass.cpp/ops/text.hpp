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
#ifndef COMPASS_CPP_OPS_TEXT_HPP
#define COMPASS_CPP_OPS_TEXT_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief The character classes and line rules of the Python scripts the
 * checks of this component port, over UTF-8 bytes.
 *
 * Python reads a source as text, so its \s, \w, str.isspace and
 * str.splitlines are Unicode-aware. Here a byte of 0x80 or more counts as
 * a word character, which is what Python decides for the letters of a
 * multi-byte character, and the non-ASCII spaces are not spaces.
 */
namespace compass::cpp::ops {

/**
 * @brief Python's \s and str.isspace for an ASCII byte: blank, \t to \r
 * and the separators \x1c to \x1f.
 */
[[nodiscard]] constexpr bool is_space(char c) noexcept {
    return c == ' ' || (c >= '\t' && c <= '\r') || (c >= '\x1c' &&
        c <= '\x1f');
}

/**
 * @brief Python's \w: an ASCII letter, digit or underscore, or a byte of a
 * multi-byte character.
 */
[[nodiscard]] constexpr bool is_word(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_' ||
        static_cast<unsigned char>(c) >= 0x80;
}

/**
 * @brief [A-Za-z0-9_]: a character of an identifier as the scripts spell
 * it out.
 */
[[nodiscard]] constexpr bool is_identifier(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_';
}

/**
 * @brief True when a \b holds before position @p i of @p text whose next
 * character is a word character: the one before it is not.
 */
[[nodiscard]] constexpr bool word_starts(std::string_view text,
    std::size_t i) noexcept {
    return i == 0 || !is_word(text[i - 1]);
}

/**
 * @brief The position of the first byte at or after @p i that is not a
 * space.
 */
[[nodiscard]] std::size_t skip_spaces(std::string_view text,
    std::size_t i) noexcept;

/**
 * @brief True when @p text holds @p word at @p i, ASCII letters compared
 * without case.
 */
[[nodiscard]] bool matches_folded(std::string_view text, std::size_t i,
    std::string_view word) noexcept;

/**
 * @brief The lines of @p text as str.splitlines() gives them: split at
 * \n, \r, \r\n, \v, \f, \x1c, \x1d, \x1e, U+0085, U+2028 and U+2029,
 * without the separators, and no empty line after a final one.
 */
[[nodiscard]] std::vector<std::string_view> split_lines(
    std::string_view text);

/**
 * @brief @p text without the spaces at either end, as str.strip() leaves
 * it.
 */
[[nodiscard]] std::string_view strip(std::string_view text) noexcept;

/**
 * @brief The 1-based line of byte @p offset: one more than the newlines
 * before it.
 */
[[nodiscard]] std::uint32_t line_of(std::string_view text,
    std::size_t offset) noexcept;

/**
 * @brief The offset of the first byte of 1-based line @p line, counting
 * newlines; the size of @p text when it has fewer lines.
 */
[[nodiscard]] std::uint32_t offset_of_line(std::string_view text,
    std::uint32_t line) noexcept;

/**
 * @brief How many characters of UTF-8 the bytes @p text hold: those that
 * are not continuation bytes.
 */
[[nodiscard]] std::size_t characters(std::string_view text) noexcept;

/**
 * @brief How many times @p what occurs in @p text without overlap, as
 * str.count counts.
 */
[[nodiscard]] std::size_t count(std::string_view text,
    std::string_view what) noexcept;

}

#endif
