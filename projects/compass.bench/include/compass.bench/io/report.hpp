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
#ifndef COMPASS_BENCH_IO_REPORT_HPP
#define COMPASS_BENCH_IO_REPORT_HPP

#include <cstddef>
#include <filesystem>
#include <ostream>
#include "compass.bench/types/file_table.hpp"
#include "compass.bench/types/ids_result.hpp"
#include "compass.bench/types/includes_result.hpp"
#include "compass.bench/types/org_result.hpp"
#include "compass.bench/types/parse_result.hpp"
#include "compass.bench/types/stage_result.hpp"
#include "compass.bench/types/store_result.hpp"
#include "compass.bench/types/types_result.hpp"
#include "compass.bench/types/warm_result.hpp"

namespace compass::bench::io {

/**
 * @brief The setup that runs once, before any stage.
 */
struct setup_timing {
    unsigned threads = 0;
    double listing_ms = 0;
    double table_ms = 0;
};

/**
 * @brief True when the stage read nothing or hit any error.
 */
[[nodiscard]] bool failed(const types::stage_result& stage);

/**
 * @brief Writes the stage table, the setup lines and the extension table.
 *
 * A failed stage gets a FAILED banner naming the reason.
 */
void write_report(std::ostream& os, const types::stage_result& stage,
    const setup_timing& setup, const types::file_table& table);

/**
 * @brief The setup the includes stage runs once, before any repetition.
 */
struct includes_setup {
    setup_timing common;
    double compile_commands_ms = 0;
    std::size_t commands = 0;
    std::size_t directories = 0;
    std::size_t contexts = 0;
    std::size_t implicit_system = 0;
};

/**
 * @brief True when the includes stage scanned nothing or hit any error.
 */
[[nodiscard]] bool failed(const types::includes_result& stage);

/**
 * @brief Writes the step timings, the include counts and the top tables.
 *
 * The tables are the most frequent unresolved names, and the files with
 * the highest direct and transitive fan-in. A failed stage gets a FAILED
 * banner naming the reason.
 */
void write_includes_report(std::ostream& os,
    const types::includes_result& stage, const includes_setup& setup,
    const types::file_table& table);

/**
 * @brief True when any language parsed no files or hit any read error.
 *
 * Parse errors inside a tree are data, not failure.
 */
[[nodiscard]] bool failed(const types::parse_result& stage);

/**
 * @brief Writes the per-language stage table and the parse error tables.
 *
 * For each language: the files with parse errors and the total error
 * nodes, then the ten files with the most error nodes and the ten slowest
 * to parse. A failed language gets a FAILED banner naming the reason.
 */
void write_parse_report(std::ostream& os, const types::parse_result& stage,
    const setup_timing& setup, const types::file_table& table);

/**
 * @brief True when the org stage parsed no file or hit any read error.
 */
[[nodiscard]] bool failed(const types::org_result& stage);

/**
 * @brief Writes the stage table, the rows the tables took and the ten
 * files that were slowest to parse.
 *
 * A failed stage gets a FAILED banner naming the reason.
 */
void write_org_report(std::ostream& os, const types::org_result& stage,
    const setup_timing& setup, const types::file_table& table);

/**
 * @brief True when the types stage found no contract page, parsed no file
 * or hit any read error.
 *
 * Unresolved types are data, not failure.
 */
[[nodiscard]] bool failed(const types::types_result& stage);

/**
 * @brief Writes the stage table, the timings, how each contract page fitted
 * the extraction rule, the documents per type, the documents with no
 * #+type: and every unresolved type with up to five of its files.
 *
 * A failed stage gets a FAILED banner naming the reason.
 */
void write_types_report(std::ostream& os, const types::types_result& stage,
    const setup_timing& setup, const types::file_table& table);

/**
 * @brief True when the store stage stored no file, hit any error, rejected
 * an object it had written, or loaded an object that dumps differently from
 * its parse.
 */
[[nodiscard]] bool failed(const types::store_result& stage);

/**
 * @brief Writes the cold stage table, the wall times of the cold and the
 * two warm runs, and the object bytes against the source bytes.
 *
 * A failed stage gets a FAILED banner naming the reason.
 */
void write_store_report(std::ostream& os, const types::store_result& stage,
    const setup_timing& setup, const std::filesystem::path& objects);

/**
 * @brief True when the ids stage resolved no file, failed to hash any, or
 * gave any ID that differs from git's.
 */
[[nodiscard]] bool failed(const types::ids_result& stage);

/**
 * @brief Writes the index the resolver read, the files by source of their
 * ID, the wall times of reading, stat-ing and hashing, and the result of
 * the check against git hash-object.
 *
 * A failed stage gets a FAILED banner naming the reason.
 */
void write_ids_report(std::ostream& os, const types::ids_result& stage,
    const setup_timing& setup);

/**
 * @brief The time a warm run of compass has, in milliseconds.
 */
inline constexpr double warm_budget_ms = 1000;

/**
 * @brief True when the warm stage found no org file, hit any error, or
 * hashed or wrote anything in a repetition after the first, which finds
 * the store and the worktree index full when nothing changed.
 */
[[nodiscard]] bool failed(const types::warm_result& stage);

/**
 * @brief Writes one row per repetition with the time of each phase and the
 * counts by ID source, loaded and written, then the medians of the warm
 * repetitions, every one after the first, against warm_budget_ms.
 *
 * A failed stage gets a FAILED banner naming the reason.
 */
void write_warm_report(std::ostream& os, const types::warm_result& stage,
    const setup_timing& setup, const std::filesystem::path& objects);

}

#endif
