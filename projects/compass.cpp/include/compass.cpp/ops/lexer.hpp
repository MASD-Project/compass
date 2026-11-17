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
#ifndef COMPASS_CPP_OPS_LEXER_HPP
#define COMPASS_CPP_OPS_LEXER_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief The one C++ lexer every C++ gate reads its sources through.
 */
namespace compass::cpp::ops {

/**
 * @brief Bytes [begin, end) of a source.
 */
struct span {
    std::uint32_t begin = 0;
    std::uint32_t end = 0;
};

/**
 * @brief One C++ source as the first translation phases see it, byte for
 * byte, so an offset into code is an offset into the source.
 *
 * code is the source with every comment and literal turned into blanks,
 * newlines kept, and the backslash of each line splice blanked. A literal
 * is a string, a raw string with or without a delimiter, or a character
 * literal, with its encoding prefix; a quote inside a number, as in
 * 1'000, separates digits and opens nothing. A // comment and a string
 * run on across a splice; a raw string ignores splices, as the compiler
 * reverts them there.
 */
struct lexed {
    std::string code;
    /** Each literal, prefix and quotes included, in order. */
    std::vector<span> literals;
    /** Each newline a backslash continues, in order. */
    std::vector<std::uint32_t> splices;
    /** Each preprocessor directive, from its # to the end of its logical
     * line, in order. */
    std::vector<span> directives;
    /** Each run of lines a conditional excludes: a branch of an #if or
     * #elif whose condition is 0, or one after a branch whose condition is
     * 1 was taken. Any other condition may hold, so its branch is live.
     * A run ends with the directive that ends it, so a directive inside
     * one is dead too. */
    std::vector<span> dead;

    [[nodiscard]] bool is_dead(std::uint32_t offset) const noexcept;
};

[[nodiscard]] lexed lex(std::string_view source);

/**
 * @brief @p source with its comments blanked and its literals kept, byte
 * for byte, for a reader that matches what a literal holds.
 */
[[nodiscard]] std::string without_comments(std::string_view source);

/**
 * @brief The keyword of the directive @p d of @p l, such as "include" or
 * "if", and the code after it to the end of its logical line.
 */
struct directive_parts {
    std::string_view keyword;
    std::string_view rest;
    std::uint32_t rest_offset = 0;
};

[[nodiscard]] directive_parts parts_of(const lexed& l, span d) noexcept;

}

#endif
