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
#ifndef COMPASS_STORE_IO_LOCATION_HPP
#define COMPASS_STORE_IO_LOCATION_HPP

#include <filesystem>
#include <vector>

namespace compass::store::io {

/**
 * @brief The git directory of one checkout, as `git rev-parse --git-dir`
 * names it: the .git directory of a main checkout, or the directory the
 * .git file of a linked worktree names. Its index file lives there.
 *
 * @throws std::runtime_error when @p checkout holds no .git or its .git
 * file cannot be understood.
 */
[[nodiscard]] std::filesystem::path git_dir(
    const std::filesystem::path& checkout);

/**
 * @brief The git directory every worktree of a repository shares, as
 * `git rev-parse --git-common-dir` names it.
 *
 * @p checkout is the root of a checkout: the directory that holds .git. In
 * the main checkout the answer is its .git directory. In a linked worktree
 * .git is a file that names the worktree's own git directory, whose
 * commondir file names the shared one.
 *
 * @throws std::runtime_error when @p checkout holds no .git or its .git
 * file cannot be understood.
 */
[[nodiscard]] std::filesystem::path git_common_dir(
    const std::filesystem::path& checkout);

/**
 * @brief Where the packs of a repository live: compass/packs under its git
 * common directory, so every worktree reads and writes the same store.
 */
[[nodiscard]] std::filesystem::path packs_dir(
    const std::filesystem::path& git_common_dir);

/**
 * @brief The directories of the first layout of the store, a file per
 * object, manifest and result: compass/objects, compass/manifests and
 * compass/results. Nothing reads them; collection removes them.
 */
[[nodiscard]] std::vector<std::filesystem::path> legacy_store_dirs(
    const std::filesystem::path& git_common_dir);

/**
 * @brief The worktree index of one checkout: compass/index under its own
 * @p git_dir, so each worktree keeps its own.
 */
[[nodiscard]] std::filesystem::path worktree_index_path(
    const std::filesystem::path& git_dir);

}

#endif
