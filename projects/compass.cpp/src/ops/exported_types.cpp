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
#include "compass.cpp/ops/exported_types.hpp"

#include "compass.cpp/ops/test_case_reachability.hpp"
#include "compass.cpp/ops/text.hpp"

namespace compass::cpp::ops {

namespace {

constexpr bool is_macro_character(char c) noexcept {
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

/**
 * @brief The end of "ORES_" and the run of [A-Z0-9_] after it, at @p i;
 * npos when @p text does not hold "ORES_" there.
 */
std::size_t ores_name_end(std::string_view text, std::size_t i) noexcept {
    if (text.substr(i, 5) != "ORES_")
        return std::string_view::npos;
    auto end = i + 5;
    while (end < text.size() && is_macro_character(text[end]))
        ++end;
    return end;
}

/**
 * @brief The end of an ORES_[A-Z0-9_]+<tail> name at @p i that a space
 * follows, or npos. The space makes the name the whole run.
 */
std::size_t name_with_tail(std::string_view text, std::size_t i,
    std::string_view tail) noexcept {
    const auto end = ores_name_end(text, i);
    if (end == std::string_view::npos || end - (i + 5) < tail.size() + 1 ||
        text.substr(end - tail.size(), tail.size()) != tail)
        return std::string_view::npos;
    if (end >= text.size() || !is_space(text[end]))
        return std::string_view::npos;
    return end;
}

/**
 * @brief The position after @p word and at least one space at @p i, or
 * npos.
 */
std::size_t after_word_and_spaces(std::string_view text, std::size_t i,
    std::string_view word) noexcept {
    if (text.substr(i, word.size()) != word)
        return std::string_view::npos;
    const auto after = i + word.size();
    const auto next = skip_spaces(text, after);
    return next == after ? std::string_view::npos : next;
}

std::optional<std::string_view> define_at(std::string_view text,
    std::size_t i) {
    i = skip_spaces(text, i);
    if (i >= text.size() || text[i] != '#')
        return std::nullopt;
    i = after_word_and_spaces(text, skip_spaces(text, i + 1), "define");
    if (i == std::string_view::npos)
        return std::nullopt;
    const auto end = name_with_tail(text, i, "_EXPORT");
    if (end == std::string_view::npos ||
        text.substr(skip_spaces(text, end), 12) != "BOOST_SYMBOL")
        return std::nullopt;
    return text.substr(i, end - i);
}

/**
 * @brief An open #if group: whether the code around it is compiled, and
 * whether one of its branches is known to be taken, by a condition of 1.
 * A condition other than 0 or 1 may hold, so its branch counts.
 */
struct conditional {
    bool outer_live = true;
    bool taken = false;
};

bool has_library_guard(std::string_view text) {
    for (auto i = text.find('#'); i != std::string_view::npos;
         i = text.find('#', i + 1)) {
        const auto name = after_word_and_spaces(text,
            skip_spaces(text, i + 1), "ifdef");
        if (name == std::string_view::npos)
            continue;
        const auto end = ores_name_end(text, name);
        if (end == std::string_view::npos)
            continue;
        const auto run = text.substr(name + 5, end - name - 5);
        if (run.find("_LIBRARY", 1) != std::string_view::npos)
            return true;
    }
    return false;
}

}

std::optional<std::string_view> export_macro(std::string_view header) {
    std::optional<std::string_view> macro;
    for (std::size_t line = 0; line <= header.size();) {
        if ((macro = define_at(header, line)))
            break;
        const auto newline = header.find('\n', line);
        if (newline == std::string_view::npos)
            break;
        line = newline + 1;
    }
    if (!macro || !has_library_guard(header))
        return std::nullopt;
    return macro;
}

std::vector<std::string_view> quoted_includes(std::string_view text) {
    const auto blanked = blank_comments_and_literals(text);
    std::vector<conditional> open;
    bool live = true;
    std::vector<std::string_view> out;
    for (std::size_t start = 0; start < text.size();) {
        auto end = text.find('\n', start);
        if (end == std::string_view::npos)
            end = text.size();
        const auto line = std::string_view(blanked).substr(start,
            end - start);
        const auto at = start;
        start = end + 1;
        auto i = skip_spaces(line, 0);
        if (i >= line.size() || line[i] != '#')
            continue;
        i = skip_spaces(line, i + 1);
        auto word_end = i;
        while (word_end < line.size() && is_identifier(line[word_end]))
            ++word_end;
        const auto word = line.substr(i, word_end - i);
        const auto condition = strip(line.substr(word_end));
        if (word == "if" || word == "ifdef" || word == "ifndef") {
            const auto known = word == "if" ? condition : "";
            open.push_back({live, known == "1"});
            live = live && known != "0";
        } else if ((word == "elif" || word == "else") && !open.empty()) {
            auto& c = open.back();
            const auto known = word == "elif" ? condition : "1";
            live = c.outer_live && !c.taken && known != "0";
            c.taken = c.taken || known == "1";
        } else if (word == "endif" && !open.empty()) {
            live = open.back().outer_live;
            open.pop_back();
        } else if (word == "include" && live) {
            const auto quote = at + skip_spaces(
                text.substr(at, end - at), word_end);
            if (quote >= end || text[quote] != '"')
                continue;
            const auto close = text.find('"', quote + 1);
            if (close < end && close > quote + 1)
                out.push_back(text.substr(quote + 1, close - quote - 1));
        }
    }
    return out;
}

std::vector<exported_type> exported_types(std::string_view header) {
    std::vector<exported_type> out;
    if (header.find("AUTO-GENERATED FILE") != std::string_view::npos)
        return out;
    for (const auto line : split_lines(header)) {
        const auto start = skip_spaces(line, 0);
        auto name = after_word_and_spaces(line, start, "class");
        if (name == std::string_view::npos)
            name = after_word_and_spaces(line, start, "struct");
        if (name == std::string_view::npos)
            continue;
        const auto end = name_with_tail(line, name, "_EXPORT");
        if (end == std::string_view::npos)
            continue;
        const auto type = skip_spaces(line, end);
        auto type_end = type;
        while (type_end < line.size() && is_word(line[type_end]))
            ++type_end;
        if (type_end == type)
            continue;
        out.push_back({line.substr(type, type_end - type),
            static_cast<std::uint32_t>(line.data() - header.data()),
            line.substr(name, end - name)});
    }
    return out;
}

std::vector<exported_type> exported_types(std::string_view header,
    std::string_view macro) {
    auto out = exported_types(header);
    std::erase_if(out,
        [&](const exported_type& t) { return t.macro != macro; });
    return out;
}

}
