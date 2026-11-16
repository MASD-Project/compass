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
#ifndef COMPASS_CLI_IO_LISTING_HPP
#define COMPASS_CLI_IO_LISTING_HPP

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include "compass.store/types/git_object.hpp"

namespace compass::cli::io {

/**
 * @brief The files of @p checkout, relative to it, in byte order, each
 * once: every tracked path and every untracked path no ignore rule
 * covers, as `git ls-files -z --cached --others --exclude-standard` lists
 * them. A tracked file deleted from the worktree is listed; reading it
 * fails.
 *
 * @throws std::system_error when git cannot be started, and
 * std::runtime_error when it fails.
 */
[[nodiscard]] std::vector<std::string> list_files(
    const std::filesystem::path& checkout);

/**
 * @brief The org files among @p files of @p checkout: those whose name
 * ends in .org and which are regular files on disk, so the corpus is what
 * is on disk.
 */
[[nodiscard]] std::vector<std::string_view> org_files(
    const std::filesystem::path& checkout,
    const std::vector<std::string>& files);

/**
 * @brief The ID of the commit @p revision names in the repository of
 * @p checkout, as `git rev-parse --verify <revision>^{commit}` resolves
 * it, so a branch, a tag, an abbreviated ID or HEAD~2 all work. This is
 * the one git process a check of a commit starts; its objects are read
 * without git.
 *
 * @throws std::system_error when git cannot be started, and
 * std::runtime_error when @p revision names no commit.
 */
[[nodiscard]] store::types::object_id resolve_commit(
    const std::filesystem::path& checkout, std::string_view revision);

}

#endif
