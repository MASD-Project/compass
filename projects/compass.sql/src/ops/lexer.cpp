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
#include "compass.sql/ops/lexer.hpp"

namespace compass::sql::ops {

namespace {

constexpr auto npos = std::string_view::npos;

constexpr bool is_space(char c) noexcept {
    return c == ' ' || (c >= '\t' && c <= '\r') || (c >= '\x1c' &&
        c <= '\x1f');
}

constexpr bool is_name(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_';
}

constexpr bool is_word(char c) noexcept {
    return is_name(c) || static_cast<unsigned char>(c) >= 0x80;
}

constexpr char lower(char c) noexcept {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

/**
 * @brief The dollar-quote tag at @p i, $[A-Za-z_0-9]*$, or empty.
 */
std::string_view dollar_tag(std::string_view text, std::size_t i) noexcept {
    if (text[i] != '$')
        return {};
    auto j = i + 1;
    while (j < text.size() && is_name(text[j]))
        ++j;
    if (j >= text.size() || text[j] != '$')
        return {};
    return text.substr(i, j + 1 - i);
}

std::size_t after_quoted(std::string_view text, std::size_t i, char quote) {
    const auto n = text.size();
    for (auto j = i + 1; j < n; ++j) {
        if (text[j] != quote)
            continue;
        if (j + 1 < n && text[j + 1] == quote) {
            ++j;
            continue;
        }
        return j + 1;
    }
    return n;
}

}

lexed lex(std::string_view text, std::uint32_t base, bool meta_commands) {
    lexed out;
    const auto n = text.size();
    out.tokens.reserve(n / 6);
    const auto add = [&](std::size_t from, std::size_t to, token_kind k) {
        out.tokens.push_back({base + static_cast<std::uint32_t>(from),
            static_cast<std::uint32_t>(to - from), k});
    };
    bool line_start = true;
    std::size_t i = 0;
    while (i < n) {
        const auto c = text[i];
        if (is_space(c)) {
            line_start = line_start || c == '\n';
            ++i;
            continue;
        }
        const auto from = i;
        const bool at_line_start = line_start;
        line_start = false;
        if (c == '-' && i + 1 < n && text[i + 1] == '-') {
            const auto end = text.find('\n', i);
            i = end == npos ? n : end;
            add(from, i, token_kind::line_comment);
            continue;
        }
        if (c == '/' && i + 1 < n && text[i + 1] == '*') {
            std::uint32_t depth = 1;
            i += 2;
            while (i < n && depth > 0) {
                if (text[i] == '/' && i + 1 < n && text[i + 1] == '*') {
                    ++depth;
                    i += 2;
                } else if (text[i] == '*' && i + 1 < n &&
                    text[i + 1] == '/') {
                    --depth;
                    i += 2;
                } else {
                    ++i;
                }
            }
            if (depth > 0) {
                out.open_depth = depth;
                out.open_offset = base + static_cast<std::uint32_t>(from);
            }
            add(from, i, token_kind::block_comment);
            continue;
        }
        if (c == '$') {
            if (const auto tag = dollar_tag(text, i); !tag.empty()) {
                const auto end = text.find(tag, i + tag.size());
                i = end == npos ? n : end + tag.size();
                add(from, i, token_kind::dollar_body);
                continue;
            }
        }
        if (c == '\'' || c == '"') {
            i = after_quoted(text, i, c);
            add(from, i, c == '\'' ? token_kind::string
                : token_kind::quoted_name);
            continue;
        }
        if (c == '\\' && meta_commands && at_line_start) {
            const auto end = text.find('\n', i);
            i = end == npos ? n : end;
            add(from, i, token_kind::meta);
            continue;
        }
        if (is_word(c)) {
            while (i < n && is_word(text[i]))
                ++i;
            add(from, i, token_kind::word);
            continue;
        }
        ++i;
        add(from, i, token_kind::symbol);
    }
    return out;
}

std::string_view dollar_content(std::string_view body) noexcept {
    const auto tag_end = body.find('$', 1);
    if (tag_end == npos)
        return {};
    const auto tag = body.substr(0, tag_end + 1);
    const auto rest = body.substr(tag.size());
    if (rest.size() >= tag.size() && rest.ends_with(tag))
        return rest.substr(0, rest.size() - tag.size());
    return rest;
}

std::string string_value(std::string_view quoted) {
    std::string out;
    if (quoted.size() < 2)
        return out;
    const auto q = quoted.front();
    auto inner = quoted.substr(1);
    if (inner.ends_with(q))
        inner.remove_suffix(1);
    out.reserve(inner.size());
    for (std::size_t i = 0; i < inner.size(); ++i) {
        out.push_back(inner[i]);
        if (inner[i] == q && i + 1 < inner.size() && inner[i + 1] == q)
            ++i;
    }
    return out;
}

bool is_keyword(std::string_view word, std::string_view lower_word) noexcept {
    if (word.size() != lower_word.size())
        return false;
    for (std::size_t i = 0; i < word.size(); ++i) {
        if (lower(word[i]) != lower_word[i])
            return false;
    }
    return true;
}

std::string lowered(std::string_view text) {
    std::string out(text);
    for (auto& c : out)
        c = lower(c);
    return out;
}

}
