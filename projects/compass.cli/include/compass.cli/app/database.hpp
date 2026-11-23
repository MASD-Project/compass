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
#ifndef COMPASS_CLI_APP_DATABASE_HPP
#define COMPASS_CLI_APP_DATABASE_HPP

#include <filesystem>
#include <ostream>
#include <string_view>
#include "compass.cli/types/options.hpp"

namespace compass::cli::app {

/**
 * @brief compass db: init, migrate, status, hooks install and prune, over
 * the database the local settings of the checkout of @p i name.
 * @returns the exit status.
 */
int run_db(const types::invocation& i, std::ostream& out, std::ostream& err);

/**
 * @brief compass sync: syncs the checkout's environment, or with --commit
 * a release environment, into the database the local settings name.
 * @returns the exit status.
 */
int run_sync(const types::invocation& i, std::ostream& out,
    std::ostream& err);

/**
 * @brief compass test export: writes the test campaigns of an environment
 * into their stories' folders in the checkout, as export_campaigns does.
 * @returns the exit status.
 */
int run_test(const types::invocation& i, std::ostream& out,
    std::ostream& err);

/**
 * @brief The sync at the end of a run over the worktree @p checkout. Does
 * nothing, and reads nothing but the local settings, when no database is
 * configured. A sync that fails is reported on @p err as a warning and
 * fails nothing; one that succeeds prints its summary only with
 * @p timings.
 */
void sync_after_run(const std::filesystem::path& checkout, unsigned threads,
    bool timings, std::ostream& err);

/**
 * @brief The sync of the edit hook: the one document @p path, relative to
 * the worktree @p checkout, when a database is configured and @p path is
 * a document. A failure is reported on @p err and fails nothing; a sync
 * that succeeds prints its summary only with @p timings.
 */
void sync_after_edit(const std::filesystem::path& checkout,
    std::string_view path, bool timings, std::ostream& err);

}

#endif
