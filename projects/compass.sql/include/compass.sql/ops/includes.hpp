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
#ifndef COMPASS_SQL_OPS_INCLUDES_HPP
#define COMPASS_SQL_OPS_INCLUDES_HPP

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "compass.codec/types/fact.hpp"
#include "compass.sql/ops/sql_model.hpp"

/**
 * @brief The include walk of ORE Studio's schema scripts: which files psql
 * reaches from the entry points, and in what order it runs what they say.
 */
namespace compass::sql::ops {

/**
 * @brief The root of the schema scripts, and the entry points besides the
 * scripts directly under it, relative to it.
 */
inline constexpr std::string_view sql_root = "projects/ores.sql";

inline constexpr std::array<std::string_view, 4> extra_entry_points{
    "drop/drop.sql", "drop/drop_database.sql", "drop/drop_test_databases.sql",
    "populate/ip2country/ip2country_import.sql"};

void write_includes(compass::codec::types::fact_writer& fact,
    std::span<const sql_include> includes);

[[nodiscard]] std::vector<sql_include> read_includes(
    compass::codec::types::fact_reader& fact);

/**
 * @brief An include whose target does not exist.
 */
struct missing_include {
    std::string file;
    std::uint32_t offset = 0;
    std::string names;
};

/**
 * @brief Where psql runs a byte of a reached file: the run of the file it
 * belongs to, in execution order, and its offset. Comparing two gives
 * which runs first.
 */
using run_point = std::pair<std::uint32_t, std::uint32_t>;

/**
 * @brief The walk of the includes from the entry points.
 *
 * A file runs from its start up to its first include, then the included
 * file runs, then the file runs on to its next include, and so on; each
 * of these stretches is a run, numbered in the order psql executes them.
 * A file psql reaches twice runs where it is first reached. Both branches
 * of a \if count as reached.
 */
struct include_walk {
    std::vector<std::string> missing_entry_points;
    std::vector<missing_include> missing_includes;
    std::map<std::string, std::vector<std::pair<std::uint32_t,
        std::uint32_t>>, std::less<>> runs;

    [[nodiscard]] bool reached(std::string_view path) const {
        return runs.contains(path);
    }

    /**
     * @brief When psql runs @p offset of @p path, or nothing when no entry
     * point reaches it.
     */
    [[nodiscard]] std::optional<run_point> when(std::string_view path,
        std::uint32_t offset) const;
};

/**
 * @brief The walk from the scripts directly under the root, in pathlib's
 * order, then the extra entry points, through @p includes_of, which gives
 * the includes of a file, and @p exists, which says whether a file exists.
 */
[[nodiscard]] include_walk walk_includes(
    std::span<const std::string_view> roots,
    const std::function<std::vector<sql_include>(std::string_view)>&
        includes_of,
    const std::function<bool(std::string_view)>& exists);

}

#endif
