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
#include "compass.platform/process/environment.hpp"

#include <cstdlib>
#include <stdexcept>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace compass::platform::process {

namespace {

std::filesystem::path variable_path(const char* name) {
    const char* value = std::getenv(name);
    return value == nullptr ? std::filesystem::path{} : value;
}

}

std::uint64_t current_process_id() noexcept {
#if defined(_WIN32)
    return ::GetCurrentProcessId();
#else
    return static_cast<std::uint64_t>(::getpid());
#endif
}

void set_environment_variable(const std::string& name,
    const std::string& value) {
#if defined(_WIN32)
    // The C runtime keeps its own copy of the environment, which getenv
    // reads; a started program inherits the system's.
    if (::_putenv_s(name.c_str(), value.c_str()) != 0 ||
        !::SetEnvironmentVariableA(name.c_str(), value.c_str()))
        throw std::runtime_error("cannot set " + name);
#else
    if (::setenv(name.c_str(), value.c_str(), 1) != 0)
        throw std::runtime_error("cannot set " + name);
#endif
}

std::filesystem::path home_directory() {
    auto home = variable_path("HOME");
#if defined(_WIN32)
    if (home.empty())
        home = variable_path("USERPROFILE");
#endif
    return home;
}

}
