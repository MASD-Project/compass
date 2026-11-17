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
#ifndef COMPASS_SQL_OPS_LEXER_HPP
#define COMPASS_SQL_OPS_LEXER_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief The one lexer of the SQL gates. It follows PostgreSQL's lexer
 * where the gates need it: block comments nest, -- comments run to the end
 * of the line, '...' and "..." double their quote to escape it, a
 * $tag$...$tag$ body runs to the same tag, and a backslash that starts a
 * line is a psql meta-command to the end of that line.
 *
 * It works on bytes: a byte of 0x80 or more is a word character, and the
 * spaces are ASCII. E'...' is a word E and a '...' string, and a
 * backslash in it escapes nothing.
 */
namespace compass::sql::ops {

enum class token_kind : std::uint8_t {
    word,
    quoted_name,
    string,
    dollar_body,
    line_comment,
    block_comment,
    meta,
    symbol
};

/**
 * @brief A token: its kind and its bytes, from offset in the text the
 * lexer was given. A token that does not close runs to the end of the
 * text.
 */
struct token {
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
    token_kind kind = token_kind::symbol;
};

/**
 * @brief The tokens of a text, without the spaces between them, and the
 * outermost block comment still open at its end: how deep the comments
 * there nest, 0 when every one closes, and where it opened.
 */
struct lexed {
    std::vector<token> tokens;
    std::uint32_t open_depth = 0;
    std::uint32_t open_offset = 0;
};

/**
 * @brief The tokens of @p text, whose offsets are counted from @p base.
 * Inside a dollar body the meta-command rule is off, as psql reads the
 * body as part of a statement.
 */
[[nodiscard]] lexed lex(std::string_view text, std::uint32_t base = 0,
    bool meta_commands = true);

/**
 * @brief The bytes of @p t in @p text, where @p text starts at offset
 * @p base.
 */
[[nodiscard]] inline std::string_view text_of(std::string_view text,
    const token& t, std::uint32_t base = 0) noexcept {
    return text.substr(t.offset - base, t.size);
}

/**
 * @brief The text between the tags of the dollar body @p body.
 */
[[nodiscard]] std::string_view dollar_content(std::string_view body) noexcept;

/**
 * @brief The value of the string token @p quoted: its bytes without the
 * outer quotes, each doubled quote once.
 */
[[nodiscard]] std::string string_value(std::string_view quoted);

/**
 * @brief Whether @p word is @p lower_word, compared without ASCII case.
 */
[[nodiscard]] bool is_keyword(std::string_view word,
    std::string_view lower_word) noexcept;

/**
 * @brief @p text with its ASCII letters lower-cased.
 */
[[nodiscard]] std::string lowered(std::string_view text);

}

#endif
