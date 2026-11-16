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
#include <vector>

namespace compass::cli::io {

/**
 * @brief The org files of @p checkout, relative to it, in git's order:
 * every tracked path and every untracked path no ignore rule covers, as
 * `git ls-files -z --cached --others --exclude-standard` lists them,
 * whose name ends in .org and which is a regular file on disk. A tracked
 * file deleted from the worktree is left out, so the corpus is what is on
 * disk.
 *
 * @throws std::system_error when git cannot be started, and
 * std::runtime_error when it fails.
 */
[[nodiscard]] std::vector<std::string> list_org_files(
    const std::filesystem::path& checkout);

}

#endif
