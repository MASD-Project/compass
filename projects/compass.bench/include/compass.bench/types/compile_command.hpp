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
#ifndef COMPASS_BENCH_TYPES_COMPILE_COMMAND_HPP
#define COMPASS_BENCH_TYPES_COMPILE_COMMAND_HPP

#include <string>
#include <vector>

namespace compass::bench::types {

/**
 * @brief One entry of a compile_commands.json database.
 *
 * The file is absolute, resolved against the directory when the database
 * gave it relative. The arguments are the compiler command line split into
 * words, whether the database stored it as "arguments" or as "command".
 */
struct compile_command {
    std::string directory;
    std::string file;
    std::vector<std::string> arguments;
};

}

#endif
