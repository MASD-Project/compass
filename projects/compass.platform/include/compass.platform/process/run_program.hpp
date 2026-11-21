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
#ifndef COMPASS_PLATFORM_PROCESS_RUN_PROGRAM_HPP
#define COMPASS_PLATFORM_PROCESS_RUN_PROGRAM_HPP

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace compass::platform::process {

/**
 * @brief Where a program's standard error goes: the caller's, nowhere, or
 * into the output it returns, interleaved with standard output.
 */
enum class error_stream : std::uint8_t { inherit, discard, merge };

/**
 * @brief One run of a program: its arguments, the first naming the program
 * and searched on PATH when it holds no slash, the directory it runs in,
 * the file it reads as standard input and where its standard error goes.
 * An empty directory or input is inherited from the caller.
 */
struct program_call {
    std::vector<std::string> arguments;
    std::filesystem::path directory;
    std::filesystem::path input;
    error_stream errors = error_stream::inherit;
};

/**
 * @brief How a program ended and what it wrote to standard output. A
 * program killed by a signal has exit code -1. Standard error goes where
 * the call says.
 */
struct program_result {
    int exit_code = 0;
    std::string output;
};

/**
 * @brief Runs @p call to completion.
 *
 * @throws std::system_error when the program cannot be started or its
 * output cannot be read.
 */
[[nodiscard]] program_result run_program(const program_call& call);

}

#endif
