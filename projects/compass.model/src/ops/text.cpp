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
#include "text.hpp"

namespace compass::model::ops {

namespace {

constexpr bool is_ascii_word(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_';
}

constexpr bool is_key_start(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/**
 * @brief Whether the code point is a Python \w character, for the scripts
 * the models are written in: letters and digits of the Latin, Greek and
 * Cyrillic blocks and anything past them that is not punctuation.
 */
constexpr bool is_word_code_point(std::uint32_t cp) noexcept {
    if (cp < 0x80)
        return is_ascii_word(static_cast<char>(cp));
    if (cp < 0xC0)
        return cp == 0xAA || cp == 0xB2 || cp == 0xB3 || cp == 0xB5 ||
            cp == 0xB9 || cp == 0xBA || (cp >= 0xBC && cp <= 0xBE);
    if (cp == 0xD7 || cp == 0xF7)
        return false;
    if (cp >= 0x2000 && cp <= 0x2BFF)
        return false;
    if (cp >= 0x3000 && cp <= 0x303F)
        return false;
    return true;
}

std::uint32_t code_point_at(std::string_view s, std::size_t i) noexcept {
    const auto b = static_cast<unsigned char>(s[i]);
    if (b < 0x80)
        return b;
    int extra = b >= 0xF0 ? 3 : b >= 0xE0 ? 2 : 1;
    std::uint32_t cp = b & (0x3F >> extra);
    for (int k = 1; k <= extra && i + k < s.size(); ++k)
        cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
    return cp;
}

/**
 * @brief Whether the code point that ends just before byte @p i is \w.
 */
bool word_before(std::string_view s, std::size_t i) noexcept {
    if (i == 0)
        return false;
    auto start = i - 1;
    while (start > 0 && (static_cast<unsigned char>(s[start]) & 0xC0) == 0x80)
        --start;
    return is_word_code_point(code_point_at(s, start));
}

bool word_at(std::string_view s, std::size_t i) noexcept {
    return i < s.size() && is_word_code_point(code_point_at(s, i));
}

std::string_view ltrim(std::string_view s) noexcept {
    while (!s.empty() && is_space(s.front()))
        s.remove_prefix(1);
    return s;
}

std::string_view rtrim(std::string_view s) noexcept {
    while (!s.empty() && is_space(s.back()))
        s.remove_suffix(1);
    return s;
}

}

std::string_view trim(std::string_view s) noexcept {
    return rtrim(ltrim(s));
}

bool iequals(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (lower(a[i]) != lower(b[i]))
            return false;
    }
    return true;
}

std::string to_lower(std::string_view s) {
    std::string out(s);
    for (auto& c : out)
        c = lower(c);
    return out;
}

void split_lines(std::string_view source, std::vector<std::string_view>& lines,
    std::vector<std::uint32_t>& starts) {
    lines.clear();
    starts.clear();
    std::size_t i = 0;
    while (i < source.size()) {
        auto end = i;
        while (end < source.size() && source[end] != '\n' &&
            source[end] != '\r')
            ++end;
        lines.push_back(source.substr(i, end - i));
        starts.push_back(static_cast<std::uint32_t>(i));
        if (end < source.size() && source[end] == '\r' &&
            end + 1 < source.size() && source[end + 1] == '\n')
            ++end;
        i = end + 1;
    }
}

std::optional<std::string_view> after_keyword(std::string_view line,
    std::string_view keyword) noexcept {
    const auto rest = ltrim(line);
    if (rest.size() < keyword.size() ||
        !iequals(rest.substr(0, keyword.size()), keyword))
        return std::nullopt;
    return rest.substr(keyword.size());
}

std::optional<heading_line> heading_title(std::string_view line) noexcept {
    std::size_t stars = 0;
    while (stars < line.size() && line[stars] == '*')
        ++stars;
    if (stars == 0 || stars >= line.size() || !is_space(line[stars]) ||
        stars + 1 >= line.size())
        return std::nullopt;
    return heading_line{stars, trim(line.substr(stars))};
}

src_line src_header(std::string_view line) noexcept {
    auto rest = ltrim(*after_keyword(line, "#+begin_src"));
    std::size_t end = 0;
    while (end < rest.size() && !is_space(rest[end]))
        ++end;
    return {rest.substr(0, end), trim(rest.substr(end))};
}

std::string_view header_argument(std::string_view rest,
    std::string_view name) noexcept {
    for (auto at = rest.find(name); at != std::string_view::npos;
         at = rest.find(name, at + 1)) {
        auto i = at + name.size();
        if (i >= rest.size() || !is_space(rest[i]))
            continue;
        while (i < rest.size() && is_space(rest[i]))
            ++i;
        auto end = i;
        while (end < rest.size() && !is_space(rest[end]))
            ++end;
        if (end == i)
            continue;
        auto value = rest.substr(i, end - i);
        while (!value.empty() && (value.front() == '"' || value.front() == '\''))
            value.remove_prefix(1);
        while (!value.empty() && (value.back() == '"' || value.back() == '\''))
            value.remove_suffix(1);
        return value;
    }
    return {};
}

std::optional<pair> drawer_property(std::string_view line) noexcept {
    auto rest = ltrim(line);
    if (rest.size() < 2 || rest[0] != ':' || !is_key_start(rest[1]))
        return std::nullopt;
    std::size_t end = 2;
    while (end < rest.size() && (is_ascii_word(rest[end]) ||
        rest[end] == '.' || rest[end] == '-'))
        ++end;
    const auto key = rest.substr(1, end - 1);
    rest = ltrim(rest.substr(end));
    if (rest.empty() || rest[0] != ':')
        return std::nullopt;
    return pair{key, trim(rest.substr(1))};
}

std::optional<pair> frontmatter_line(std::string_view line) noexcept {
    if (line.size() < 3 || line[0] != '#' || line[1] != '+' ||
        !is_key_start(line[2]))
        return std::nullopt;
    std::size_t end = 3;
    while (end < line.size() && is_ascii_word(line[end]))
        ++end;
    const auto key = line.substr(2, end - 2);
    auto rest = ltrim(line.substr(end));
    if (rest.empty() || rest[0] != ':')
        return std::nullopt;
    return pair{key, trim(rest.substr(1))};
}

bool is_table_rule(std::string_view line) noexcept {
    const auto t = trim(line);
    if (t.size() < 3 || t.front() != '|' || t.back() != '|')
        return false;
    for (std::size_t i = 1; i + 1 < t.size(); ++i) {
        if (t[i] != '-' && t[i] != '+' && t[i] != '|')
            return false;
    }
    return true;
}

std::optional<std::vector<std::string_view>> table_row(std::string_view line) {
    const auto t = trim(line);
    if (t.size() < 3 || t.front() != '|' || t.back() != '|')
        return std::nullopt;
    auto inner = t.substr(1, t.size() - 2);
    std::vector<std::string_view> cells;
    for (;;) {
        const auto bar = inner.find('|');
        cells.push_back(trim(inner.substr(0, bar)));
        if (bar == std::string_view::npos)
            break;
        inner.remove_prefix(bar + 1);
    }
    return cells;
}

std::optional<std::string_view> bullet(std::string_view line) noexcept {
    const auto rest = ltrim(line);
    if (rest.size() < 2 || rest[0] != '-' || !is_space(rest[1]))
        return std::nullopt;
    return trim(rest.substr(1));
}

std::string strip_verbatim(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '=' && !word_before(text, i) && i + 1 < text.size() &&
            text[i + 1] != '=' && !is_space(text[i + 1])) {
            const auto close = text.find('=', i + 2);
            if (close != std::string_view::npos && !word_at(text, close + 1)) {
                out.append(text.substr(i + 1, close - i - 1));
                i = close + 1;
                continue;
            }
        }
        out += text[i];
        ++i;
    }
    return out;
}

std::string collapse_spaces(std::string_view s) {
    std::string out;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && is_space(s[i]))
            ++i;
        auto end = i;
        while (end < s.size() && !is_space(s[end]))
            ++end;
        if (end > i) {
            if (!out.empty())
                out += ' ';
            out.append(s.substr(i, end - i));
        }
        i = end;
    }
    return out;
}

bool truthy(std::string_view s) noexcept {
    const auto t = trim(s);
    return iequals(t, "true") || iequals(t, "yes") || t == "1";
}

}
