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
#include "compass.cpp/ops/test_case_reachability.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <iterator>
#include <set>
#include <utility>
#include "compass.cpp/ops/lexer.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/text.hpp"

namespace compass::cpp::ops {

namespace {

constexpr std::array<std::string_view, 12> test_macros{
    "TEMPLATE_TEST_CASE_METHOD_SIG", "TEMPLATE_PRODUCT_TEST_CASE",
    "TEMPLATE_TEST_CASE_METHOD", "TEMPLATE_LIST_TEST_CASE",
    "TEST_CASE_PERSISTENT_FIXTURE", "TEMPLATE_TEST_CASE_SIG",
    "TEST_CASE_METHOD_SIG", "TEST_CASE_METHOD", "TEMPLATE_TEST_CASE",
    "SCENARIO_METHOD", "TEST_CASE", "SCENARIO"};

constexpr std::array<std::string_view, 9> test_suffixes{
    ".cpp", ".cc", ".cxx", ".c++", ".hpp", ".h", ".hxx", ".ipp", ".inc"};

constexpr std::array<std::string_view, 5> skipped_directories{
    "node_modules", "venv", ".venv", "build", "external"};

using name_set = std::set<std::string, std::less<>>;

constexpr bool starts_identifier(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

constexpr bool is_letter(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

constexpr bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }

std::size_t identifier_end(std::string_view text, std::size_t i) noexcept {
    while (i < text.size() && is_identifier(text[i]))
        ++i;
    return i;
}

/**
 * @brief The text with its splices removed, and for each byte the line and
 * the offset of the source it came from.
 */
struct spliced_text {
    std::string text;
    std::vector<std::uint32_t> lines;
    std::vector<std::uint32_t> offsets;
};

spliced_text unsplice(std::string_view text) {
    spliced_text out;
    out.text.reserve(text.size());
    out.lines.reserve(text.size());
    out.offsets.reserve(text.size());
    std::uint32_t line = 1;
    for (std::size_t i = 0; i < text.size();) {
        const auto c = text[i];
        if (c == '\\' && text.substr(i + 1, 2) == "\r\n") {
            i += 3;
            ++line;
            continue;
        }
        if (c == '\\' && text.substr(i + 1, 1) == "\n") {
            i += 2;
            ++line;
            continue;
        }
        out.text.push_back(c);
        out.lines.push_back(line);
        out.offsets.push_back(static_cast<std::uint32_t>(i));
        if (c == '\n')
            ++line;
        ++i;
    }
    return out;
}

/**
 * @brief Whether the quote at @p start opens a character literal rather
 * than separating digits: it does not follow a digit, and another quote
 * closes it within eight characters on the same line.
 */
bool closes_char_literal(std::string_view text, std::size_t start) {
    if (start > 0 && is_digit(text[start - 1]))
        return false;
    const auto end = text.find('\'', start + 1);
    if (end == std::string_view::npos ||
        characters(text.substr(start, end - start)) > 8)
        return false;
    return text.substr(start, end - start).find('\n') ==
        std::string_view::npos;
}

/**
 * @brief The end of a raw string whose quote is at @p quote: past the
 * closing )delim", or the end of the text.
 */
std::size_t raw_string_end(std::string_view text, std::size_t quote,
    std::size_t open_paren) {
    const auto delim = text.substr(quote + 1, open_paren - quote - 1);
    std::string closer(")");
    closer += delim;
    closer.push_back('"');
    const auto stop = text.find(closer, open_paren);
    return stop == std::string_view::npos ? text.size()
                                          : stop + delim.size() + 2;
}

std::size_t string_end(std::string_view text, std::size_t i) noexcept {
    auto j = i + 1;
    while (j < text.size() && text[j] != '"')
        j += text[j] == '\\' ? 2 : 1;
    return j;
}

}

namespace {

std::vector<std::pair<std::size_t, std::size_t>> raw_string_spans(
    std::string_view text) {
    std::vector<std::pair<std::size_t, std::size_t>> spans;
    const auto n = text.size();
    for (std::size_t i = 0; i < n;) {
        if (text.substr(i, 2) == "//") {
            const auto end = text.find('\n', i);
            i = end == std::string_view::npos ? n : end;
        } else if (text.substr(i, 2) == "/*") {
            const auto end = text.find("*/", i + 2);
            i = end == std::string_view::npos ? n : end + 2;
        } else if (text[i] == '"' && i > 0 && text[i - 1] == 'R') {
            const auto open_paren = text.find('(', i);
            if (open_paren == std::string_view::npos) {
                i = n;
                continue;
            }
            const auto end = raw_string_end(text, i, open_paren);
            spans.emplace_back(i - 1, end);
            i = end;
        } else if (text[i] == '"') {
            i = string_end(text, i) + 1;
        } else if (text[i] == '\'' && closes_char_literal(text, i)) {
            i = text.find('\'', i + 1) + 1;
        } else {
            ++i;
        }
    }
    return spans;
}

/**
 * @brief The line of the first raw string that removing the splices of
 * @p text moves, or 0 when none moves.
 */
std::uint32_t raw_string_moved(std::string_view text,
    const spliced_text& spliced) {
    using span = std::pair<std::size_t, std::size_t>;
    const auto original = raw_string_spans(text);
    std::set<span> before(original.begin(), original.end());
    std::set<span> after;
    for (const auto& [start, stop] : raw_string_spans(spliced.text))
        after.emplace(spliced.offsets[start], spliced.offsets[stop - 1] + 1);
    std::vector<span> differing;
    std::ranges::set_symmetric_difference(before, after,
        std::back_inserter(differing));
    return differing.empty() ? 0 : line_of(text, differing.front().first);
}

/**
 * @brief The keyword of a directive line, ^\s*#\s*([A-Za-z_]+); empty
 * when the line is not one.
 */
std::string_view directive_of(std::string_view line) noexcept {
    auto i = skip_spaces(line, 0);
    if (i >= line.size() || line[i] != '#')
        return {};
    i = skip_spaces(line, i + 1);
    auto end = i;
    while (end < line.size() && is_letter(line[end]))
        ++end;
    return line.substr(i, end - i);
}

/**
 * @brief The end of the name of a #define on @p line, or npos.
 */
std::size_t define_name_end(std::string_view line, std::size_t& name)
    noexcept {
    auto i = skip_spaces(line, 0);
    if (i >= line.size() || line[i] != '#')
        return std::string_view::npos;
    i = skip_spaces(line, i + 1);
    if (line.substr(i, 6) != "define")
        return std::string_view::npos;
    const auto after = i + 6;
    name = skip_spaces(line, after);
    if (name == after || name >= line.size() ||
        !starts_identifier(line[name]))
        return std::string_view::npos;
    return identifier_end(line, name);
}

/**
 * @brief True when @p text names one of @p names as a whole identifier
 * after a word boundary.
 */
bool names_any(std::string_view text, const name_set& names) {
    for (std::size_t i = 0; i < text.size();) {
        if (starts_identifier(text[i]) && word_starts(text, i)) {
            const auto end = identifier_end(text, i);
            if (names.contains(text.substr(i, end - i)))
                return true;
            i = end == i ? i + 1 : end;
        } else {
            ++i;
        }
    }
    return false;
}

/**
 * @brief How many test case declarations @p line holds: a name of
 * @p names, whole, after a word boundary, then optional spaces and "(".
 */
std::uint64_t declarations(std::string_view line, const name_set& names) {
    std::uint64_t count = 0;
    for (std::size_t i = 0; i < line.size();) {
        if (!(starts_identifier(line[i]) && word_starts(line, i))) {
            ++i;
            continue;
        }
        const auto end = identifier_end(line, i);
        if (names.contains(line.substr(i, end - i))) {
            const auto paren = skip_spaces(line, end);
            if (paren < line.size() && line[paren] == '(') {
                ++count;
                i = paren + 1;
                continue;
            }
        }
        i = end;
    }
    return count;
}

}

bool is_test_source(std::string_view path) {
    constexpr std::string_view root = "projects/";
    if (!path.starts_with(root))
        return false;
    const auto parts = components_of(path.substr(root.size()));
    bool in_tests = false;
    for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
        if (std::ranges::find(skipped_directories, parts[i]) !=
            skipped_directories.end())
            return false;
        in_tests = in_tests || parts[i] == "tests";
    }
    std::string suffix(suffix_of(path));
    std::ranges::transform(suffix, suffix.begin(), [](char c) {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    });
    if (std::ranges::find(test_suffixes, suffix) == test_suffixes.end())
        return false;
    return in_tests || stem_of(path).ends_with("_tests");
}

scan_result scan_source(std::string_view text) {
    const auto l = lex(text);
    std::vector<std::pair<std::uint32_t, std::string>> logical;
    std::string current;
    std::uint32_t line = 1;
    std::uint32_t first = 1;
    std::size_t splice = 0;
    for (std::size_t i = 0; i < l.code.size(); ++i) {
        const char c = l.code[i];
        if (c != '\n') {
            current.push_back(c);
            continue;
        }
        ++line;
        if (splice < l.splices.size() && l.splices[splice] == i) {
            ++splice;
            current.pop_back();
            if (i >= 2 && text[i - 1] == '\r')
                current.pop_back();
            continue;
        }
        logical.emplace_back(first, std::move(current));
        current.clear();
        first = line;
    }
    if (!current.empty())
        logical.emplace_back(first, std::move(current));

    name_set names(test_macros.begin(), test_macros.end());
    const name_set macros_only = names;
    for (const auto& [line, content] : logical) {
        std::size_t name = 0;
        const auto end = define_name_end(content, name);
        if (end != std::string_view::npos &&
            names_any(content.substr(end), macros_only))
            names.emplace(content.substr(name, end - name));
    }

    const auto original = split_lines(text);
    const auto at = [&](std::uint32_t line) {
        return line >= 1 && line <= original.size()
            ? std::string(strip(original[line - 1])) : std::string();
    };

    scan_result r;
    std::vector<std::pair<std::uint32_t, std::string>> open;
    for (const auto& [line, content] : logical) {
        const auto keyword = directive_of(content);
        if (!keyword.empty()) {
            if (keyword == "if" || keyword == "ifdef" || keyword == "ifndef")
                open.emplace_back(line, at(line));
            else if (keyword == "endif" && !open.empty())
                open.pop_back();
            else if (keyword == "endif")
                r.problems.push_back({line, at(line),
                    reading_problem::unopened_endif});
            continue;
        }
        const auto count = declarations(content, names);
        r.declared += count;
        if (count == 0 || open.empty())
            continue;
        for (std::uint64_t k = 0; k < count; ++k)
            r.cases.push_back({line, at(line), open.back().first,
                open.back().second});
    }
    for (const auto& [line, opened] : open)
        r.problems.push_back({line, opened,
            reading_problem::unclosed_conditional});
    if (const auto moved = raw_string_moved(text, unsplice(text));
        moved != 0)
        r.problems.push_back({moved, at(moved),
            reading_problem::raw_string_moves});
    return r;
}

}
