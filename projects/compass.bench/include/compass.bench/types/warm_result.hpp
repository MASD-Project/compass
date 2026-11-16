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
#ifndef COMPASS_BENCH_TYPES_WARM_RESULT_HPP
#define COMPASS_BENCH_TYPES_WARM_RESULT_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace compass::bench::types {

/**
 * @brief What one repetition of the warm stage did.
 *
 * The four times are the wall times of its phases: building the resolver
 * and resolving every ID, loading every object or parsing the file when
 * it was missing or rejected, writing the parsed objects, and writing the
 * worktree index.
 * The sources count the IDs from git's index, from the worktree index and
 * from hashing. loaded counts the objects that decoded and whose checksum
 * matched, written those parsed and written instead. errors counts files
 * that could not be hashed, parsed or written.
 */
struct warm_run {
    double resolve_ms = 0;
    double load_ms = 0;
    double write_ms = 0;
    double index_ms = 0;
    std::uint64_t from_git_index = 0;
    std::uint64_t from_worktree_index = 0;
    std::uint64_t hashed = 0;
    std::uint64_t loaded = 0;
    std::uint64_t written = 0;
    std::uint64_t errors = 0;

    [[nodiscard]] double total_ms() const noexcept {
        return resolve_ms + load_ms + write_ms + index_ms;
    }
};

/**
 * @brief What the warm stage did over every repetition.
 *
 * files counts the org files of the checkout. runs holds one row per
 * repetition; the first may fill the store and the worktree index, and the
 * rest find them full. index_entries is the size of the last worktree index
 * written, and the two problems say why the last run could not use git's
 * index or the worktree index, when it could not.
 */
struct warm_result {
    std::uint64_t files = 0;
    std::uint64_t index_entries = 0;
    std::string git_index_problem;
    std::string worktree_index_problem;
    std::string first_error;
    std::vector<warm_run> runs;
};

}

#endif
