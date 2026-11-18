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
#ifndef COMPASS_PLATFORM_PROCESS_EXECUTABLE_HPP
#define COMPASS_PLATFORM_PROCESS_EXECUTABLE_HPP

#include <filesystem>
#include <optional>
#include <string_view>

namespace compass::platform::process {

/**
 * @brief The absolute path of the running executable with every symbolic
 * link resolved, so a program started through a link finds the files that
 * sit beside its real binary. Empty when the system cannot say.
 */
[[nodiscard]] std::filesystem::path executable_path();

/**
 * @brief The program @p name as a shell finds it: @p name itself when it
 * holds a directory separator, else the first executable regular file of
 * that name in a directory of PATH. On Windows a name without an extension
 * also matches that name with each extension in PATHEXT. Nothing when there
 * is none. Runs nothing.
 */
[[nodiscard]] std::optional<std::filesystem::path> find_program(
    std::string_view name);

}

#endif
