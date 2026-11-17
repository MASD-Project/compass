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
#include "compass.cpp/ops/enum_attributes.hpp"

#include <array>
#include "compass.cpp/ops/text.hpp"

namespace compass::cpp::ops {

namespace {

constexpr bool starts_identifier(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/**
 * @brief The end of the identifier [A-Za-z_]\w* at @p i, or @p i when
 * there is none.
 */
std::size_t identifier_end(std::string_view text, std::size_t i) noexcept {
    if (i >= text.size() || !starts_identifier(text[i]))
        return i;
    ++i;
    while (i < text.size() && is_word(text[i]))
        ++i;
    return i;
}

/**
 * @brief The position after @p word at @p i and the spaces that follow
 * it, when at least one does; npos otherwise.
 */
std::size_t word_then_spaces(std::string_view text, std::size_t i,
    std::string_view word) noexcept {
    if (text.substr(i, word.size()) != word)
        return std::string_view::npos;
    const auto after = i + word.size();
    const auto next = skip_spaces(text, after);
    return next == after ? std::string_view::npos : next;
}

bool attribute_at(std::string_view body, std::size_t i) noexcept {
    if (body.substr(i, 19) == "BOOST_SYMBOL_EXPORT" ||
        body.substr(i, 19) == "BOOST_SYMBOL_IMPORT")
        return true;
    if (body.substr(i, 10) == "__declspec") {
        auto j = skip_spaces(body, i + 10);
        if (j >= body.size() || body[j] != '(')
            return false;
        j = skip_spaces(body, j + 1);
        return body.substr(j, 9) == "dllimport" ||
            body.substr(j, 9) == "dllexport";
    }
    if (body.substr(i, 13) == "__attribute__") {
        auto j = skip_spaces(body, i + 13);
        if (body.substr(j, 2) != "((")
            return false;
        j = skip_spaces(body, j + 2);
        return body.substr(j, 10) == "visibility";
    }
    return false;
}

bool has_attribute(std::string_view body) noexcept {
    for (std::size_t i = 0; i < body.size(); ++i) {
        if ((body[i] == 'B' || body[i] == '_') && attribute_at(body, i))
            return true;
    }
    return false;
}

/**
 * @brief True when the text before @p keyword ends with "using" and at
 * least one space.
 */
bool follows_using(std::string_view text, std::size_t keyword) noexcept {
    auto j = keyword;
    while (j > 0 && is_space(text[j - 1]))
        --j;
    return j < keyword && j >= 5 && text.substr(j - 5, 5) == "using" &&
        word_starts(text, j - 5);
}

}

std::string erase_comments_and_literals(std::string_view text) {
    std::string out(text);
    const auto length = text.size();
    std::size_t index = 0;
    while (index < length) {
        const auto current = text[index];
        if (current == '/' && text.substr(index, 2) == "//") {
            while (index < length && text[index] != '\n')
                out[index++] = ' ';
        } else if (current == '/' && text.substr(index, 2) == "/*") {
            while (index < length && text.substr(index, 2) != "*/") {
                if (text[index] != '\n')
                    out[index] = ' ';
                ++index;
            }
            for (auto at = index; at < std::min(index + 2, length); ++at)
                out[at] = ' ';
            index += 2;
        } else if (current == '"' || current == '\'') {
            const auto quote = current;
            out[index++] = ' ';
            while (index < length && text[index] != quote) {
                if (text[index] == '\\' && index + 1 < length)
                    out[index++] = ' ';
                if (index < length && text[index] != '\n')
                    out[index] = ' ';
                ++index;
            }
            if (index < length)
                out[index] = ' ';
            ++index;
        } else {
            ++index;
        }
    }
    return out;
}

void add_visibility_macros(std::string_view erased,
    std::set<std::string, std::less<>>& macros) {
    std::string joined;
    joined.reserve(erased.size());
    for (std::size_t i = 0; i < erased.size(); ++i) {
        if (erased[i] == '\\' && i + 1 < erased.size() &&
            erased[i + 1] == '\n') {
            joined.push_back(' ');
            ++i;
        } else {
            joined.push_back(erased[i]);
        }
    }
    for (const auto line : split_lines(joined)) {
        auto i = skip_spaces(line, 0);
        if (i >= line.size() || line[i] != '#')
            continue;
        i = word_then_spaces(line, skip_spaces(line, i + 1), "define");
        if (i == std::string_view::npos)
            continue;
        const auto end = identifier_end(line, i);
        if (end == i || end >= line.size() || !is_space(line[end]))
            continue;
        if (has_attribute(line.substr(end)))
            macros.emplace(line.substr(i, end - i));
    }
}

namespace {

template <typename Predicate>
std::vector<enum_site> sites_of(std::string_view erased,
    Predicate treat_as_macro) {
    std::vector<enum_site> out;
    std::vector<std::string_view> lines;
    bool split = false;
    for (auto p = erased.find("enum"); p != std::string_view::npos;) {
        const auto ws = p + 4;
        auto end = skip_spaces(erased, ws);
        if (!word_starts(erased, p) || end == ws) {
            p = erased.find("enum", p + 1);
            continue;
        }
        for (const auto key : {std::string_view("class"),
                 std::string_view("struct")}) {
            if (const auto after = word_then_spaces(erased, end, key);
                after != std::string_view::npos) {
                end = after;
                break;
            }
        }
        const auto keyword = p;
        p = erased.find("enum", end);
        if (follows_using(erased, keyword))
            continue;

        const auto first = identifier_end(erased, end);
        if (first == end)
            continue;
        const auto name = erased.substr(end, first - end);
        const auto is_macro = treat_as_macro(name);
        auto cursor = first;
        if (is_macro) {
            const auto second_at = skip_spaces(erased, cursor);
            const auto second = identifier_end(erased, second_at);
            if (second == second_at)
                continue;
            cursor = second;
        }
        cursor = skip_spaces(erased, cursor);
        if (cursor >= erased.size() || (erased[cursor] != ':' &&
            erased[cursor] != '{' && erased[cursor] != ';'))
            continue;

        if (!split) {
            lines = split_lines(erased);
            split = true;
        }
        const auto line = line_of(erased, keyword);
        out.push_back({static_cast<std::uint32_t>(keyword),
            line <= lines.size() ? std::string(strip(lines[line - 1]))
                                 : std::string(),
            is_macro ? std::string(name) : std::string()});
    }
    return out;
}

}

std::vector<enum_site> enum_sites(std::string_view erased,
    const std::set<std::string, std::less<>>& macros) {
    return sites_of(erased,
        [&](std::string_view name) { return macros.contains(name); });
}

std::vector<enum_site> macro_enum_sites(std::string_view erased) {
    return sites_of(erased, [](std::string_view) { return true; });
}

}
