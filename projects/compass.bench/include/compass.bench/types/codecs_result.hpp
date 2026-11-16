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
#ifndef COMPASS_BENCH_TYPES_CODECS_RESULT_HPP
#define COMPASS_BENCH_TYPES_CODECS_RESULT_HPP

#include <cstdint>
#include <string>
#include <vector>
#include "compass.codec/io/output_sink.hpp"
#include "compass.codec/types/registry.hpp"
#include "compass.codec/types/run_result.hpp"

namespace compass::bench::types {

/**
 * @brief A text and how many findings share it.
 */
struct counted_text {
    std::string text;
    std::uint64_t count = 0;
};

/**
 * @brief The findings of one codec, summed up for the report.
 *
 * documents counts the documents with at least one finding. rules and
 * messages hold the most frequent rule names and messages, most frequent
 * first. first holds the first findings in document order, one line each.
 */
struct findings_summary {
    std::uint64_t documents = 0;
    std::vector<counted_text> rules;
    std::vector<counted_text> messages;
    std::vector<std::string> first;
};

/**
 * @brief What loading the corpus and running the codecs over it did.
 *
 * files counts the org files of the checkout and documents those that
 * loaded. loaded counts the objects found in the store and written those
 * parsed and written instead; errors counts files that could not be
 * hashed, read, parsed or stored, which are left out of the corpus.
 * load_ms covers resolving every ID, mapping every source and object and
 * writing the worktree index.
 *
 * The dependency record of the checkout is read once, before the first
 * repetition, and each repetition starts from it, so every repetition
 * calls the same units. record_problem says why a record that was there
 * was not used, record_bytes is the size of the file read and
 * record_units its units; record_written is true when the record the last
 * repetition made differed from the file and replaced it.
 *
 * sink is what writing the outputs did when an output root was given, and
 * sink_ms its time; outputs_problem says why a written-outputs record that
 * was there was not used, and outputs_written is true when the record was
 * rewritten.
 *
 * registry holds the codecs. ms has one row per codec and one time per
 * repetition, summed over threads, and wall_ms one wall time per
 * repetition. last is what the last repetition produced, and findings
 * sums up its diagnostics, one row per codec.
 */
struct codecs_result {
    std::uint64_t files = 0;
    std::uint64_t documents = 0;
    std::uint64_t loaded = 0;
    std::uint64_t written = 0;
    std::uint64_t errors = 0;
    std::string first_error;
    std::uint64_t registered_types = 0;
    double registry_ms = 0;
    double load_ms = 0;
    std::string record_problem;
    std::uint64_t record_bytes = 0;
    std::uint64_t record_units = 0;
    double record_read_ms = 0;
    bool record_written = false;
    double record_write_ms = 0;
    bool wrote_outputs = false;
    codec::io::sink_result sink;
    double sink_ms = 0;
    std::string outputs_problem;
    bool outputs_written = false;
    codec::types::registry registry;
    std::vector<std::vector<double>> ms;
    std::vector<double> wall_ms;
    codec::types::run_result last;
    std::vector<findings_summary> findings;
};

}

#endif
