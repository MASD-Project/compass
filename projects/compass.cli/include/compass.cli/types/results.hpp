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
#ifndef COMPASS_CLI_TYPES_RESULTS_HPP
#define COMPASS_CLI_TYPES_RESULTS_HPP

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "compass.codec/io/format_cache.hpp"
#include "compass.codec/io/output_sink.hpp"
#include "compass.codec/types/diagnostic_table.hpp"
#include "compass.store/types/collection.hpp"

namespace compass::cli::types {

/**
 * @brief What loading the corpus and running the codecs did.
 *
 * files counts the org files of the checkout, documents those that
 * loaded, loaded those whose object came from the store and parsed those
 * parsed instead. errors holds one line per file that could not be read
 * or parsed, and problems one line per defect of the codec registry, such
 * as a --set nobody declared; either makes the command fail. codecs
 * names each codec of the run and ran counts the units of that codec that
 * were called rather than reused, skipped those the worktree's record
 * stood in for and cached those the result cache stood in for. formats
 * says, per codec, what formatting its files did, for a codec whose files
 * were formatted. steps names each step of the command with its wall
 * time, in the order they ran. ms is the wall time from listing the
 * checkout to the end of the codec run.
 */
struct run_summary {
    std::uint64_t files = 0;
    std::uint64_t documents = 0;
    std::uint64_t loaded = 0;
    std::uint64_t parsed = 0;
    std::vector<std::string> errors;
    std::vector<std::string> problems;
    std::vector<std::string> codecs;
    std::vector<std::uint64_t> ran;
    std::vector<std::uint64_t> skipped;
    std::vector<std::uint64_t> cached;
    std::vector<std::optional<codec::io::format_stats>> formats;
    std::vector<std::pair<std::string, double>> steps;
    double ms = 0;

    [[nodiscard]] bool failed() const noexcept {
        return !errors.empty() || !problems.empty();
    }
};

/**
 * @brief What compass generate did: sink counts what writing the outputs
 * under root did, and its problems why nothing could be written. ms is
 * the wall time of the whole command.
 */
struct generate_result {
    run_summary run;
    std::filesystem::path root;
    codec::io::sink_result sink;
    double ms = 0;
};

/**
 * @brief What compass diff found: planned counts what generate would do,
 * and diff is the unified diff of every file it would change.
 */
struct diff_result {
    run_summary run;
    std::filesystem::path root;
    codec::io::sink_result planned;
    std::string diff;
    double ms = 0;
};

/**
 * @brief One diagnostic, located: line and column are 1-based, the column
 * counted in bytes. rule is the codec and rule name as codec/rule.
 */
struct finding {
    std::string path;
    std::uint32_t line = 0;
    std::uint32_t column = 0;
    std::string rule;
    codec::types::severity level = codec::types::severity::error;
    std::string message;
    std::string fix;
};

/**
 * @brief What compass check found, sorted by path, line and column.
 */
struct check_result {
    run_summary run;
    std::vector<finding> findings;
    double ms = 0;
};

/**
 * @brief What compass gc found and removed in the store at objects.
 */
struct gc_result {
    std::filesystem::path objects;
    store::types::collection_options options;
    store::types::collection_result collection;
};

}

#endif
