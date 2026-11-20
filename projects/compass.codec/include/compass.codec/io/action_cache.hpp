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
#ifndef COMPASS_CODEC_IO_ACTION_CACHE_HPP
#define COMPASS_CODEC_IO_ACTION_CACHE_HPP

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include "compass.codec/types/action_manifest.hpp"
#include "compass.codec/types/dependency_record.hpp"
#include "compass.store/types/blob_id.hpp"

namespace compass::codec::io {

/**
 * @brief The result cache of the repository whose git common directory is
 * @p git_common_dir, as ops::run reads it: the action objects of the packs
 * under its compass/packs, as they are when the source is made.
 */
[[nodiscard]] types::action_source action_source_of(
    const std::filesystem::path& git_common_dir);

/**
 * @brief What store_actions wrote: result objects, manifests and the bytes
 * of all it wrote, with the manifests left alone because their set was
 * already first, the fact objects of the files scans mapped and the input
 * chunks the store did not hold.
 */
struct action_writes {
    std::uint64_t results = 0;
    std::uint64_t manifests = 0;
    std::uint64_t unchanged_manifests = 0;
    std::uint64_t bytes = 0;
    std::uint64_t facts = 0;
    std::uint64_t chunks = 0;
};

/**
 * @brief Puts the units @p fresh of @p record in the result cache of
 * @p git_common_dir, on up to @p threads threads: each one's result object,
 * and its input set at the head of its manifest. @p scans, when not
 * empty, holds the scan table of each of @p fresh, in order, which goes
 * into its result object, and @p facts the fact objects of the files the
 * scans mapped, by key, which are written as they are.
 *
 * A result object is always written, so one a damaged copy made the unit
 * run is replaced. A manifest is read, given the set and written back,
 * with those chunks of the set the store does not hold intact;
 * one that does not load is replaced by a manifest of the one set. Two
 * worktrees that write one manifest at once can lose one of their sets,
 * which costs one more run of that unit, never a wrong result. Everything
 * goes into one pack, published when all is written.
 *
 * @throws std::system_error when a write fails.
 */
action_writes store_actions(const std::filesystem::path& git_common_dir,
    const types::dependency_record& record,
    std::span<const types::record_unit_id> fresh, unsigned threads,
    std::span<const std::string> scans = {},
    std::span<const std::pair<compass::store::types::blob_id, std::string>>
        facts = {});

/**
 * @brief The retired file of one worktree: compass/retired under its own
 * @p git_dir, beside its dependency record.
 */
[[nodiscard]] std::filesystem::path retired_file_path(
    const std::filesystem::path& git_dir);

/**
 * @brief Records, in the retired file of @p git_dir, the keys the run that
 * replaced @p previous by @p current retired, as ops::retired_keys says,
 * before the runs already there, keeping ops::kept_runs - 1 runs. A
 * damaged file is replaced.
 * @return false when the run retired nothing, and the file is left alone.
 * @throws std::filesystem::filesystem_error when the write fails.
 */
bool retire_keys(const std::filesystem::path& git_dir,
    const types::dependency_record& previous,
    const types::dependency_record& current,
    std::span<const types::record_unit_id> fresh);

/**
 * @brief The keys of the action objects the worktree whose git directory
 * is @p git_dir needs, which collection keeps: those of each unit of its
 * dependency record, as ops::unit_keys says, and those of its retired
 * file, so the last ops::kept_runs runs of the worktree stay in the
 * cache. A missing or damaged record or retired file names none.
 */
[[nodiscard]] std::vector<compass::store::types::blob_id> action_keys(
    const std::filesystem::path& git_dir);

}

#endif
