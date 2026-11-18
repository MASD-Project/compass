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
#ifndef COMPASS_CLI_CONFIG_ORG_READER_HPP
#define COMPASS_CLI_CONFIG_ORG_READER_HPP

#include <cstdint>
#include <string_view>
#include <vector>
#include "compass.org/types/tables.hpp"

namespace compass::cli::config {

/**
 * @brief @p cell without the = or ~ that mark it verbatim in org.
 */
[[nodiscard]] std::string_view verbatim(std::string_view cell) noexcept;

/**
 * @brief @p s without the blanks and newlines at either end.
 */
[[nodiscard]] std::string_view trim(std::string_view s) noexcept;

/**
 * @brief The 1-based line of byte @p offset of @p text.
 */
[[nodiscard]] std::uint32_t line_of(std::string_view text,
    std::uint32_t offset) noexcept;

/**
 * @brief Reads the headings and tables of a parsed configuration document:
 * its text and the tables the parser made of it.
 */
struct org_reader {
    std::string_view text;
    compass::org::types::tables_view t;

    /**
     * @brief The level-1 heading titled @p title, or none.
     */
    [[nodiscard]] compass::org::types::heading_id heading(
        std::string_view title) const;

    /**
     * @brief The first table under the level-1 heading @p title, or none.
     */
    [[nodiscard]] compass::org::types::element_id table_under(
        std::string_view title) const;

    /**
     * @brief The text of each cell of @p row, trimmed, without its bar.
     */
    [[nodiscard]] std::vector<std::string_view> cells(
        compass::org::types::element_id row) const;

    /**
     * @brief The rows of @p table but a first row whose first cell is
     * @p header, in order.
     */
    [[nodiscard]] std::vector<compass::org::types::element_id> rows(
        compass::org::types::element_id table,
        std::string_view header) const;
};

}

#endif
