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
#include "compass.platform/process/executable.hpp"

#include <cstdlib>
#include <string>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace compass::platform::process {

namespace {

#if defined(_WIN32)

constexpr char path_list_separator = ';';

bool has_separator(std::string_view name) {
    return name.find_first_of("/\\") != std::string_view::npos;
}

bool is_executable(const std::filesystem::path& p) {
    std::error_code ignored;
    return std::filesystem::is_regular_file(p, ignored);
}

/**
 * @brief @p p itself, else @p p with the first extension of PATHEXT that
 * names an executable, when @p p has no extension of its own.
 */
std::optional<std::filesystem::path> executable_named(
    const std::filesystem::path& p) {
    if (is_executable(p))
        return p;
    if (p.has_extension())
        return std::nullopt;
    const char* listed = std::getenv("PATHEXT");
    std::string_view extensions =
        listed == nullptr ? ".COM;.EXE;.BAT;.CMD" : listed;
    while (!extensions.empty()) {
        const auto end = extensions.find(path_list_separator);
        const auto extension = extensions.substr(0, end);
        auto candidate = p;
        candidate += std::string(extension);
        if (!extension.empty() && is_executable(candidate))
            return candidate;
        if (end == std::string_view::npos)
            break;
        extensions.remove_prefix(end + 1);
    }
    return std::nullopt;
}

#else

constexpr char path_list_separator = ':';

bool has_separator(std::string_view name) {
    return name.find('/') != std::string_view::npos;
}

std::optional<std::filesystem::path> executable_named(
    const std::filesystem::path& p) {
    std::error_code ignored;
    if (std::filesystem::is_regular_file(p, ignored) &&
        ::access(p.c_str(), X_OK) == 0)
        return p;
    return std::nullopt;
}

#endif

}

std::filesystem::path executable_path() {
#if defined(_WIN32)
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const auto size = static_cast<DWORD>(buffer.size());
        const DWORD n = ::GetModuleFileNameW(nullptr, buffer.data(), size);
        if (n == 0)
            return {};
        if (n < size) {
            buffer.resize(n);
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    std::error_code ec;
    auto path = std::filesystem::canonical(buffer, ec);
    return ec ? std::filesystem::path(buffer) : path;
#else
    std::error_code ec;
    auto path = std::filesystem::read_symlink("/proc/self/exe", ec);
    return ec ? std::filesystem::path() : path;
#endif
}

std::optional<std::filesystem::path> find_program(std::string_view name) {
    if (name.empty())
        return std::nullopt;
    if (has_separator(name))
        return executable_named(std::filesystem::path(name));
    const char* path = std::getenv("PATH");
    std::string_view dirs = path == nullptr ? "" : path;
    while (true) {
        const auto end = dirs.find(path_list_separator);
        const auto dir = dirs.substr(0, end);
        if (auto found = executable_named(
                std::filesystem::path(dir.empty() ? "." : dir) / name))
            return found;
        if (end == std::string_view::npos)
            return std::nullopt;
        dirs.remove_prefix(end + 1);
    }
}

}
