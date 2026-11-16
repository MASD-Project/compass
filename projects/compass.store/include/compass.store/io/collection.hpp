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
#ifndef COMPASS_STORE_IO_COLLECTION_HPP
#define COMPASS_STORE_IO_COLLECTION_HPP

#include <filesystem>
#include <span>
#include <vector>
#include "compass.store/types/blob_id.hpp"
#include "compass.store/types/collection.hpp"

namespace compass::store::io {

/**
 * @brief The git directory of every worktree of a repository: @p
 * git_common_dir itself, which is the main worktree's, then each directory
 * under its worktrees/ that holds a git index or a worktree index, in name
 * order.
 *
 * The checkout a linked worktree's gitdir file names is not looked at, so a
 * worktree whose checkout is gone counts until git prunes its directory.
 */
[[nodiscard]] std::vector<std::filesystem::path> worktree_git_dirs(
    const std::filesystem::path& git_common_dir);

/**
 * @brief Every ID the git index or the worktree index of any of @p git_dirs
 * names, sorted and distinct.
 *
 * Every entry of git's index counts, whatever its stage, mode or flags. A
 * directory with neither file names nothing.
 *
 * @throws std::runtime_error naming the file when an index exists and cannot
 * be read or decoded, since what it names cannot then be kept.
 */
[[nodiscard]] std::vector<types::blob_id> live_ids(
    std::span<const std::filesystem::path> git_dirs);

/**
 * @brief The objects and temporary files under @p objects_dir, with their
 * size and modification time, in no particular order. A missing directory
 * holds none.
 */
[[nodiscard]] types::stored_objects list_objects(
    const std::filesystem::path& objects_dir);

/**
 * @brief Collects the store of the repository @p checkout belongs to.
 *
 * Reads the indexes of every worktree, lists @p objects_dir, chooses the
 * victims with ops::plan_collection, the cutoff being now less the grace
 * period, and unlinks each unless this is a dry run. A victim whose size
 * or modification time changed since it was listed is left alone, as is
 * one already gone. A process that has an object mapped keeps its mapping,
 * and a run that looks up a removed object parses its source again.
 *
 * @throws std::runtime_error when @p checkout is not a git checkout or an
 * index cannot be read, before anything is removed.
 */
[[nodiscard]] types::collection_result collect(
    const std::filesystem::path& checkout,
    const std::filesystem::path& objects_dir,
    const types::collection_options& options);

}

#endif
