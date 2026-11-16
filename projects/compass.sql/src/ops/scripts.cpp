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
#include "compass.sql/ops/scripts.hpp"

#include <algorithm>
#include <optional>

namespace compass::sql::ops {

namespace {

bool is_space(char c) noexcept {
    return c == ' ' || (c >= '\t' && c <= '\r') ||
        (c >= '\x1c' && c <= '\x1f');
}

}

std::string blank_comments(std::string_view text) {
    std::string out(text);
    bool quoted = false;
    for (std::size_t i = 0; i < text.size();) {
        const char ch = text[i];
        if (quoted) {
            if (ch == '\'') {
                if (i + 1 < text.size() && text[i + 1] == '\'') {
                    i += 2;
                    continue;
                }
                quoted = false;
            }
            ++i;
            continue;
        }
        if (ch == '\'') {
            quoted = true;
            ++i;
            continue;
        }
        if (text.substr(i, 2) == "--") {
            for (; i < text.size() && text[i] != '\n'; ++i)
                out[i] = ' ';
            continue;
        }
        if (text.substr(i, 2) == "/*") {
            for (; i < text.size() && text.substr(i, 2) != "*/"; ++i) {
                if (text[i] != '\n')
                    out[i] = ' ';
            }
            for (int k = 0; k < 2 && i < text.size(); ++k)
                out[i++] = ' ';
            continue;
        }
        ++i;
    }
    return out;
}

std::vector<std::string_view> split_top(std::string_view text,
    char separator) {
    std::vector<std::string_view> parts;
    int depth = 0;
    std::optional<char> quote;
    std::size_t start = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char ch = text[i];
        if (quote) {
            if (ch == *quote) {
                if (i + 1 < text.size() && text[i + 1] == *quote) {
                    ++i;
                    continue;
                }
                quote.reset();
            }
        } else if (ch == '\'' || ch == '"') {
            quote = ch;
        } else if (ch == '(' || ch == '[') {
            ++depth;
        } else if (ch == ')' || ch == ']') {
            --depth;
        } else if (ch == separator && depth == 0) {
            parts.push_back(strip(text.substr(start, i - start)));
            start = i + 1;
        }
    }
    parts.push_back(strip(text.substr(start)));
    return parts;
}

std::string_view strip(std::string_view text) noexcept {
    while (!text.empty() && is_space(text.front()))
        text.remove_prefix(1);
    while (!text.empty() && is_space(text.back()))
        text.remove_suffix(1);
    return text;
}

std::string_view first_word(std::string_view text) noexcept {
    text = strip(text);
    std::size_t end = 0;
    while (end < text.size() && !is_space(text[end]))
        ++end;
    return text.substr(0, end);
}

bool component_less(std::string_view a, std::string_view b) noexcept {
    const auto n = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) {
        if (a[i] == b[i])
            continue;
        if (a[i] == '/')
            return true;
        if (b[i] == '/')
            return false;
        return static_cast<unsigned char>(a[i]) <
            static_cast<unsigned char>(b[i]);
    }
    return a.size() < b.size();
}

std::uint32_t offset_of_line(std::string_view text,
    std::uint32_t line) noexcept {
    std::size_t at = 0;
    for (std::uint32_t l = 1; l < line; ++l) {
        const auto next = text.find('\n', at);
        if (next == std::string_view::npos)
            return static_cast<std::uint32_t>(text.size());
        at = next + 1;
    }
    return static_cast<std::uint32_t>(at);
}

}
