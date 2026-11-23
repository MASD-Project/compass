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
#ifndef COMPASS_CLI_APP_TEST_EXPORT_HPP
#define COMPASS_CLI_APP_TEST_EXPORT_HPP

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include "compass.db/io/connection.hpp"

namespace compass::cli::app {

/**
 * @brief What an export did: the campaigns it exported, the paths it
 * wrote because their bytes changed, how many files already held theirs,
 * how many screenshots are not PNG and were written as they are, and the
 * :ID: of each story whose campaign was not exported because the
 * environment no longer holds the story's page.
 */
struct export_report {
    std::size_t campaigns = 0;
    std::vector<std::string> written;
    std::size_t unchanged = 0;
    std::size_t not_png = 0;
    std::vector<std::string> skipped;
};

/**
 * @brief Writes each campaign of @p environment, or only that of
 * @p story, into the checkout @p checkout as ops::render_campaign renders
 * it, through platform::filesystem::update_file, so a file that already
 * holds its bytes is not touched, then sets the exported campaigns'
 * exported_at.
 * @throws db::query::unknown_environment when the environment does not
 * exist, db::io::error when the database fails and std::system_error when
 * a file cannot be written.
 */
export_report export_campaigns(db::io::connection& c,
    const std::filesystem::path& checkout, const std::string& environment,
    const std::optional<std::string>& story);

}

#endif
