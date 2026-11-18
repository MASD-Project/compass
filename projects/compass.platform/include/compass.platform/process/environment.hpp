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
#ifndef COMPASS_PLATFORM_PROCESS_ENVIRONMENT_HPP
#define COMPASS_PLATFORM_PROCESS_ENVIRONMENT_HPP

#include <cstdint>
#include <filesystem>
#include <string>

namespace compass::platform::process {

/**
 * @brief The ID the system gives the running process.
 */
[[nodiscard]] std::uint64_t current_process_id() noexcept;

/**
 * @brief Sets the environment variable @p name to @p value for this
 * process and the programs it starts.
 */
void set_environment_variable(const std::string& name,
    const std::string& value);

/**
 * @brief The user's home directory as git finds it: HOME, and on Windows
 * USERPROFILE when HOME is not set. Empty when neither is.
 */
[[nodiscard]] std::filesystem::path home_directory();

}

#endif
