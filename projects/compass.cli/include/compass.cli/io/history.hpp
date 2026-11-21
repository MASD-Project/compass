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
#ifndef COMPASS_CLI_IO_HISTORY_HPP
#define COMPASS_CLI_IO_HISTORY_HPP

#include <expected>
#include <optional>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include "compass.cli/types/provenance.hpp"
#include "compass.store/types/git_object.hpp"

/**
 * What a branch's history and its pull request say, read through git and
 * gh, which are not content of the checkout.
 */
namespace compass::cli::io {

/**
 * @brief The commits of @p to that @p from lacks, oldest first, each with
 * its trailers.
 * @throws std::runtime_error when git cannot list them.
 */
[[nodiscard]] std::vector<types::branch_commit> branch_commits(
    const std::filesystem::path& checkout,
    const store::types::object_id& from, const store::types::object_id& to);

/**
 * @brief The branch HEAD of @p checkout is on, or, when HEAD is detached,
 * as in a pull request's CI run, the branch GITHUB_HEAD_REF names; empty
 * when neither names one.
 */
[[nodiscard]] std::string head_branch(const std::filesystem::path& checkout);

/**
 * @brief The pull request of the JSON gh pr view --json number,body,url
 * printed, or why @p json is not that.
 */
[[nodiscard]] std::expected<types::pull_request, std::string>
parse_pull_request(std::string_view json);

/**
 * @brief The pull request of @p branch, or of the branch gh finds when it
 * is empty, as the program @p gh reads it from the forge of the
 * repository of @p checkout.
 * @return why there is none: gh is off, missing or not signed in, or the
 * branch has no pull request, as one plain line.
 */
[[nodiscard]] std::expected<types::pull_request, std::string>
find_pull_request(const std::filesystem::path& checkout, std::string_view gh,
    std::string_view branch);

/**
 * @brief The lines git prints for @p args, run in @p checkout, without
 * their line ends; none when git fails, as it does for a path or a
 * revision it does not know.
 * @throws std::system_error when git cannot be started.
 */
[[nodiscard]] std::vector<std::string> git_lines(
    const std::filesystem::path& checkout,
    const std::vector<std::string>& args);

/**
 * @brief The commit @p id of @p checkout, a full ID or any revision: its
 * full ID, author, subject and trailers, as ops::closing_trailers reads
 * them; nothing when git knows no such commit.
 */
[[nodiscard]] std::optional<types::commit_detail> read_commit(
    const std::filesystem::path& checkout, std::string_view id);

/**
 * @brief Whether the commit @p ancestor is @p descendant or one of its
 * ancestors: git rev-list <descendant>..<ancestor> lists nothing. That
 * walks only the commits since the older one, where git merge-base
 * --is-ancestor, in a repository with no commit graph, walks the whole
 * history to answer no.
 */
[[nodiscard]] bool is_ancestor(const std::filesystem::path& checkout,
    std::string_view ancestor, std::string_view descendant);

}

#endif
