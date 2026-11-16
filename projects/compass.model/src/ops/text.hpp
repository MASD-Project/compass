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
#ifndef COMPASS_MODEL_OPS_TEXT_HPP
#define COMPASS_MODEL_OPS_TEXT_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/*
 * The line rules of ores.codegen's parse_org, one function per regular
 * expression it applies, written out by hand. Each names the expression it
 * stands for. \s and \w are taken over ASCII, with any non-ASCII letter a
 * word character, which is what the model documents need.
 */
namespace compass::model::ops {

[[nodiscard]] constexpr bool is_space(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
        c == '\v';
}

[[nodiscard]] constexpr char lower(char c) noexcept {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

[[nodiscard]] std::string_view trim(std::string_view s) noexcept;
[[nodiscard]] bool iequals(std::string_view a, std::string_view b) noexcept;
[[nodiscard]] std::string to_lower(std::string_view s);

/**
 * @brief Splits @p source as str.splitlines() does for \\n, \\r\\n and \\r,
 * with the byte offset each line starts at.
 */
void split_lines(std::string_view source, std::vector<std::string_view>& lines,
    std::vector<std::uint32_t>& starts);

/**
 * @brief What follows ^\\s*<keyword> matched in any case, or nothing.
 */
[[nodiscard]] std::optional<std::string_view> after_keyword(
    std::string_view line, std::string_view keyword) noexcept;

struct heading_line {
    std::size_t level = 0;
    std::string_view title;
};

/**
 * @brief ^(\\*+)\\s+(.+?)\\s*$
 */
[[nodiscard]] std::optional<heading_line> heading_title(
    std::string_view line) noexcept;

struct src_line {
    std::string_view language;
    std::string_view rest;
};

/**
 * @brief ^\\s*#\\+begin_src\\s+(\\S+)(?:\\s+(.*))?\\s*$, for a line known to
 * match it.
 */
[[nodiscard]] src_line src_header(std::string_view line) noexcept;

/**
 * @brief The first <name>\\s+(\\S+) in @p rest, without quotes at either
 * end, or empty.
 */
[[nodiscard]] std::string_view header_argument(std::string_view rest,
    std::string_view name) noexcept;

struct pair {
    std::string_view key;
    std::string_view value;
};

/**
 * @brief ^\\s*:([A-Za-z_][A-Za-z0-9_.-]*)\\s*:\\s*(.*)$, the value trimmed.
 */
[[nodiscard]] std::optional<pair> drawer_property(
    std::string_view line) noexcept;

/**
 * @brief ^#\\+([A-Za-z_][A-Za-z_0-9]*)\\s*:\\s*(.*)$, the value trimmed.
 */
[[nodiscard]] std::optional<pair> frontmatter_line(
    std::string_view line) noexcept;

/**
 * @brief ^\\s*\\|[-+|]+\\|\\s*$
 */
[[nodiscard]] bool is_table_rule(std::string_view line) noexcept;

/**
 * @brief ^\\s*\\|(.+)\\|\\s*$, split at every | and each cell trimmed.
 */
[[nodiscard]] std::optional<std::vector<std::string_view>> table_row(
    std::string_view line);

/**
 * @brief ^\\s*-\\s+(.*)$, the item trimmed.
 */
[[nodiscard]] std::optional<std::string_view> bullet(
    std::string_view line) noexcept;

/**
 * @brief Replaces (?<!\\w)=([^=\\s][^=]*?)=(?!\\w) with its group.
 */
[[nodiscard]] std::string strip_verbatim(std::string_view text);

/**
 * @brief " ".join(s.split()).
 */
[[nodiscard]] std::string collapse_spaces(std::string_view s);

/**
 * @brief The Python truth of a flag: true, yes or 1 in any case after
 * trimming.
 */
[[nodiscard]] bool truthy(std::string_view s) noexcept;

}

#endif
