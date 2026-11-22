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
#ifndef COMPASS_CLI_CONFIG_ARGUMENTS_HPP
#define COMPASS_CLI_CONFIG_ARGUMENTS_HPP

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include "compass.cli/types/options.hpp"

namespace compass::cli::config {

/**
 * @brief The command line asked for the help text.
 */
struct show_help {};

/**
 * @brief The command line asked for the version.
 */
struct show_version {};

/**
 * @brief The command line is not one compass understands, and why.
 */
struct usage_error {
    std::string message;
};

using parsed_arguments =
    std::variant<types::invocation, show_help, show_version, usage_error>;

/**
 * @brief The help text: the commands, their options and the exit
 * statuses.
 */
[[nodiscard]] std::string_view help_text() noexcept;

/**
 * @brief Parses the arguments after the program name.
 *
 * One argument is the command: generate, or its alias gen, diff, check,
 * gc or config. Options may come before or after it, each value as the
 * next argument. -C names the checkout, relative to @p working_directory
 * and the working directory itself by default, as git -C does. --meta,
 * --out and --set go into the command-line layer as written, with no
 * default; resolving the configuration makes the paths absolute. -j
 * defaults to @p default_threads.
 * --timings asks for the time of each step. --ratchet belongs to
 * check and takes the next argument as its base unless it starts with -
 * or names a command; --ratchet=<base> always does. --dry-run, --cap
 * and --grace belong to gc alone, and --scope, --skip-stale and --files
 * to generate and diff. --help or -h anywhere asks for help and
 * --version for the version, whatever else is there.
 */
[[nodiscard]] parsed_arguments parse_arguments(
    std::span<const std::string_view> args,
    const std::filesystem::path& working_directory,
    unsigned default_threads);

}

#endif
