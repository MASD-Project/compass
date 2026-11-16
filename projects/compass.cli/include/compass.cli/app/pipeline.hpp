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
#ifndef COMPASS_CLI_APP_PIPELINE_HPP
#define COMPASS_CLI_APP_PIPELINE_HPP

#include "compass.cli/types/options.hpp"
#include "compass.cli/types/results.hpp"

/**
 * The commands of the compass executable, one function each. Every
 * command but gc first runs the steps of app/steps.hpp: list the org files
 * of the checkout, build the type registry from the contract pages under
 * the meta directory, resolve their IDs, load each object from the store
 * or parse the file, and run the enabled codecs from the dependency record
 * of the checkout and the result cache of the repository. The store is
 * compass/objects under the git common directory. None of these functions
 * prints.
 */
namespace compass::cli::app {

/**
 * @brief Runs the pipeline and makes the files under the output root hold
 * what the codecs emit, touching only the files that differ.
 *
 * Stores the parsed objects, the worktree index, the result cache and the
 * dependency record, then the outputs and the written-outputs record.
 * Outputs are written only when every file loaded and the registry has no
 * problem.
 *
 * @throws std::system_error or std::filesystem::filesystem_error when the
 * checkout, the meta directory or the output root cannot be read or
 * written.
 */
[[nodiscard]] types::generate_result generate(const types::options& o);

/**
 * @brief Runs the pipeline and diffs what generate would write against
 * the files under the output root. Writes nothing anywhere.
 *
 * @throws as generate does.
 */
[[nodiscard]] types::diff_result diff(const types::options& o);

/**
 * @brief Runs the pipeline and returns the diagnostics of every codec.
 *
 * Stores the parsed objects, the worktree index, the result cache and the
 * dependency record, so the next command starts warm, and writes no
 * output.
 *
 * @throws as generate does.
 */
[[nodiscard]] types::check_result check(const types::options& o);

/**
 * @brief Collects the store with the collection options of @p o.
 *
 * @throws std::runtime_error when the checkout is not a git checkout or
 * an index cannot be read, before anything is removed.
 */
[[nodiscard]] types::gc_result gc(const types::options& o);

}

#endif
