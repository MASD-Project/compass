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
#include <functional>
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
 * @brief The files of the first layout of the store under @p objects_dir,
 * a fan-out directory of objects or of action objects, with their size and
 * modification time, in no particular order. A missing directory holds
 * none.
 */
[[nodiscard]] types::stored_objects list_objects(
    const std::filesystem::path& objects_dir);

/**
 * @brief The keys of the action objects the record of one worktree names,
 * given its git directory. The store does not know the record; its writer
 * does.
 */
using action_roots = std::function<std::vector<types::blob_id>(
    const std::filesystem::path& git_dir)>;

/**
 * @brief Collects the store of the repository @p checkout belongs to.
 *
 * Reads the indexes of every worktree and the action keys @p roots gives
 * for each, and opens every pack. The entries the plan keeps go into one
 * new pack and the old packs are removed, unless this is a dry run or
 * there is one pack and nothing to drop. ops::plan_collection chooses the
 * victims over every entry, an object being live when an index names its
 * ID and an action object when a record names its key, so one cap and one
 * grace period cover both; the age of an entry is when it was first
 * written. Entries a later one hides are dropped whatever their age.
 * Temporary files and packs that do not parse are removed once older than
 * the grace period, and the directories of the first layout of the store
 * are removed whole. The lock of the packs directory is held throughout,
 * so no repack runs at the same time; writers are not held up, and a pack
 * published during the collection is left as it is.
 *
 * @throws std::runtime_error when @p checkout is not a git checkout or an
 * index cannot be read, before anything is removed.
 */
[[nodiscard]] types::collection_result collect(
    const std::filesystem::path& checkout,
    const types::collection_options& options, const action_roots& roots);

}

#endif
