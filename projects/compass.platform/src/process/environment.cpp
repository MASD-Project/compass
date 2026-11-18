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

std::filesystem::path variable_path(const std::string& name) {
    const auto value = environment_variable(name);
    return value ? std::filesystem::path(*value) : std::filesystem::path{};
}

}

std::uint64_t current_process_id() noexcept {
#if defined(_WIN32)
    return ::GetCurrentProcessId();
#else
    return static_cast<std::uint64_t>(::getpid());
#endif
}

std::optional<std::string> environment_variable(const std::string& name) {
#if defined(_WIN32)
    // The C runtime's copy, which set_environment_variable keeps; getenv
    // reads it too, but MSVC deprecates getenv for _dupenv_s.
    char* value = nullptr;
    std::size_t size = 0;
    if (::_dupenv_s(&value, &size, name.c_str()) != 0 || value == nullptr)
        return std::nullopt;
    std::string out(value);
    std::free(value);
    return out;
#else
    const char* value = std::getenv(name.c_str());
    if (value == nullptr)
        return std::nullopt;
    return std::string(value);
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
