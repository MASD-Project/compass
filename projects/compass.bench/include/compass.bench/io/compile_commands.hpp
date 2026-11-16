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
#ifndef COMPASS_BENCH_IO_COMPILE_COMMANDS_HPP
#define COMPASS_BENCH_IO_COMPILE_COMMANDS_HPP

#include <string>
#include <string_view>
#include <vector>
#include "compass.bench/types/compile_command.hpp"

namespace compass::bench::io {

/**
 * @brief Splits a shell command line into words.
 *
 * Follows the POSIX shell quoting that compile_commands.json uses: blanks
 * separate words, single quotes are literal, and a backslash escapes the next
 * character outside single quotes.
 */
[[nodiscard]] std::vector<std::string> split_command(std::string_view command);

/**
 * @brief Parses the text of a compile_commands.json database.
 *
 * Accepts any valid JSON but only reads the top-level array of objects and
 * their "directory", "file", "arguments" and "command" members. Throws
 * std::runtime_error naming the byte offset of the first syntax error, or
 * naming the entry that lacks a directory, a file or a command.
 */
[[nodiscard]] std::vector<types::compile_command> parse_compile_commands(
    std::string_view json);

}

#endif
