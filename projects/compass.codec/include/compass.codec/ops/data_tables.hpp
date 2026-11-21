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
#ifndef COMPASS_CODEC_OPS_DATA_TABLES_HPP
#define COMPASS_CODEC_OPS_DATA_TABLES_HPP

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace compass::codec::ops {

/**
 * @brief The key of the option a codec that reads data tables declares.
 * Its value is what encode_data_tables writes; compass sets it from the
 * check document whose Definition names the codec with :CODEC:, so an
 * edit to a table changes the codec's configuration and runs it again.
 */
inline constexpr std::string_view data_option = "data";

/**
 * @brief One table of a check document: the title of the level-1 heading
 * it sits under, its column names, and per row its cells and the line of
 * the document the row is on.
 */
struct data_table {
    std::string name;
    std::uint32_t line = 0;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
    std::vector<std::uint32_t> row_lines;

    /**
     * @brief The index of the column named @p column, or the column count
     * when there is none.
     */
    [[nodiscard]] std::size_t column(std::string_view column) const;
};

/**
 * @brief The data tables of one check document, and its path.
 */
struct data_tables {
    std::string source;
    std::vector<data_table> tables;

    /**
     * @brief The table named @p name, compared without case, or null.
     */
    [[nodiscard]] const data_table* find(std::string_view name) const;
};

/**
 * @brief The text of @p tables, the value of a codec's data option: a
 * SOURCE line, then per table a TABLE line with its line and name, a
 * COLUMNS line and a ROW line per row with its line, each cell after a
 * tab, with backslash, tab and newline escaped.
 */
[[nodiscard]] std::string encode_data_tables(const data_tables& tables);

/**
 * @brief The tables encode_data_tables wrote as @p text, or why the text
 * is not such.
 */
[[nodiscard]] std::expected<data_tables, std::string> decode_data_tables(
    std::string_view text);

/**
 * @brief A table a codec reads: the heading it sits under, and the
 * columns it must have, in any order among others.
 */
struct table_need {
    std::string_view name;
    std::span<const std::string_view> columns;
};

/**
 * @brief A codec's tables, from the value of its data option @p text, or
 * the reason, written for the reader of the finding, that it cannot have
 * them: no check document gives them, or a table of @p needed is missing
 * or lacks a column.
 */
[[nodiscard]] std::expected<data_tables, std::string> tables_for(
    std::string_view codec, std::string_view text,
    std::span<const table_need> needed);

}

#endif
