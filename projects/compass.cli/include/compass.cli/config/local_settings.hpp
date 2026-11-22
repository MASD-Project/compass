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
#ifndef COMPASS_CLI_CONFIG_LOCAL_SETTINGS_HPP
#define COMPASS_CLI_CONFIG_LOCAL_SETTINGS_HPP

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace compass::cli::config {

/**
 * @brief The settings of one machine, never committed: a file
 * compass/local.org under the git directory every worktree of a repository
 * shares, and one under each worktree's own git directory, which wins
 * where both set a keyword. For a main checkout the two are one file.
 *
 * database is #+database:, the libpq connection string of the
 * repository's compass database, empty when none is configured;
 * environment is #+environment:, the name of this worktree's environment,
 * empty for the default. database_file is the file the database came
 * from.
 */
struct local_settings {
    std::string database;
    std::string environment;
    std::filesystem::path database_file;
};

/**
 * @brief The local settings of the repository @p checkout belongs to:
 * <git common dir>/compass/local.org.
 * @throws std::runtime_error when @p checkout is not a git checkout.
 */
[[nodiscard]] std::filesystem::path repository_settings_path(
    const std::filesystem::path& checkout);

/**
 * @brief The local settings of the worktree @p checkout: <git
 * dir>/compass/local.org.
 * @throws std::runtime_error when @p checkout is not a git checkout.
 */
[[nodiscard]] std::filesystem::path worktree_settings_path(
    const std::filesystem::path& checkout);

/**
 * @brief Reads the local settings of @p checkout. A checkout that is not
 * a git checkout, and a file that does not exist, set nothing.
 * @returns why a file that exists cannot be read.
 */
[[nodiscard]] std::expected<local_settings, std::string>
read_local_settings(const std::filesystem::path& checkout);

/**
 * @brief Sets #+<key>: @p value in @p file, replacing the line that set it
 * or adding one, and creating the file when it is missing.
 * @throws std::system_error when the file cannot be written.
 */
void write_local_setting(const std::filesystem::path& file,
    std::string_view key, std::string_view value);

/**
 * @brief The name of the environment of the worktree @p checkout:
 * @p given when not empty, else #+environment: of its local settings, else
 * the name of its directory.
 */
[[nodiscard]] std::string environment_name(
    const std::filesystem::path& checkout, const local_settings& s,
    std::string_view given);

}

#endif
