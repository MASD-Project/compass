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
#ifndef COMPASS_CLI_APP_APPLICATION_HPP
#define COMPASS_CLI_APP_APPLICATION_HPP

#include <filesystem>
#include <ostream>
#include <span>
#include <string_view>

namespace compass::cli::app {

/**
 * @brief The exit statuses of the compass executable: success when
 * nothing would change and no diagnostic fires, findings when diff finds
 * a change or check a diagnostic, failure on an error or bad usage.
 */
inline constexpr int exit_success = 0;
inline constexpr int exit_findings = 1;
inline constexpr int exit_failure = 2;

/**
 * @brief Runs the command line @p args, the arguments after the program
 * name, from @p working_directory, writing to @p out and @p err, and
 * returns the exit status.
 *
 * Every command reads the configuration file of the checkout and applies
 * the codec defaults, the file and the command line in that order. Bad
 * usage prints the reason and the help text to @p err; an error in the
 * configuration prints its file, line and fix, and is a failure. An
 * exception
 * a command throws is printed to @p err as "compass: <what>" and is a
 * failure.
 */
[[nodiscard]] int run(std::span<const std::string_view> args,
    const std::filesystem::path& working_directory, std::ostream& out,
    std::ostream& err);

}

#endif
