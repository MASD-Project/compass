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
#ifndef COMPASS_CLI_APP_SYNC_HPP
#define COMPASS_CLI_APP_SYNC_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "compass.db/io/connection.hpp"
#include "compass.db/io/corpus.hpp"
#include "compass.store/types/git_object.hpp"

namespace compass::cli::app {

/**
 * @brief What to sync: the environment of the checkout @p checkout, named
 * environment, or, when commit is set, the release environment of that
 * commit, read from git's objects with no checkout of it.
 */
struct sync_request {
    std::filesystem::path checkout;
    std::string environment;
    std::optional<store::types::object_id> commit;
    unsigned threads = 1;
};

/**
 * @brief What a sync did: the paths it placed, the documents the corpus
 * lacked and how many of those were parsed rather than taken from the
 * store, what writing the environment and collecting did, the files that
 * could not be read, and the wall time of each step.
 */
struct sync_report {
    std::string environment;
    std::size_t placed = 0;
    std::size_t loaded = 0;
    std::size_t parsed = 0;
    db::io::write_result written;
    db::io::collection_result collection;
    std::vector<std::string> errors;
    std::vector<std::pair<std::string, double>> steps;
    double ms = 0;
};

/**
 * @brief Syncs a whole environment: lists its org documents and their
 * blob IDs as the pipeline does, takes compass's parse of each document
 * the corpus lacks from the store, parsing it when the store has none,
 * writes those documents and the environment's placements in one
 * transaction, and collects the corpus when it has grown enough.
 * @throws db::io::error when the database fails, and as the pipeline's
 * listing does.
 */
sync_report sync_environment(db::io::connection& c, const sync_request& r);

/**
 * @brief Syncs one document of the worktree @p checkout, @p path relative
 * to it, into the environment @p environment: its placement follows the
 * file, and leaves the environment when the file is gone. Every other
 * path of the environment stays as it is.
 * @throws db::io::error when the database fails.
 */
sync_report sync_file(db::io::connection& c,
    const std::filesystem::path& checkout, const std::string& environment,
    std::string_view path);

/**
 * @brief Whether @p path, relative to the checkout @p checkout, is a
 * document a sync loads: an org file other than the repository file.
 */
[[nodiscard]] bool is_synced_document(const std::filesystem::path& checkout,
    std::string_view path);

}

#endif
