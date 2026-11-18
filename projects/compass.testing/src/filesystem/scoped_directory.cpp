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
#include "compass.testing/filesystem/scoped_directory.hpp"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <system_error>
#include <vector>
#include "compass.platform/process/environment.hpp"
#include "compass.platform/process/liveness.hpp"

namespace compass::testing::filesystem {

namespace {

namespace process = compass::platform::process;

bool all_digits(std::string_view s) {
    return !s.empty() &&
        std::ranges::all_of(s, [](char c) { return c >= '0' && c <= '9'; });
}

/**
 * @brief The process ID in @p name when it reads <prefix>.<process ID>.<number>.
 */
std::optional<std::uint64_t> owner_of(std::string_view name,
    std::string_view prefix) {
    if (!name.starts_with(prefix) || name.size() <= prefix.size() ||
        name[prefix.size()] != '.')
        return std::nullopt;
    const auto rest = name.substr(prefix.size() + 1);
    const auto dot = rest.find('.');
    if (dot == std::string_view::npos || !all_digits(rest.substr(0, dot)) ||
        !all_digits(rest.substr(dot + 1)))
        return std::nullopt;
    std::uint64_t id = 0;
    const auto [end, error] = std::from_chars(rest.data(), rest.data() + dot, id);
    if (error != std::errc{})
        return std::nullopt;
    return id;
}

void remove_orphans(const std::filesystem::path& temp,
    std::string_view prefix) {
    const auto self = process::current_process_id();
    std::vector<std::filesystem::path> orphans;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(temp, ec), end;
         !ec && it != end; it.increment(ec)) {
        const auto owner = owner_of(it->path().filename().string(), prefix);
        if (owner && *owner != self && !process::is_running(*owner))
            orphans.push_back(it->path());
    }
    // Another user's directory, or one a concurrent run removes first,
    // stays where it is; neither is this run's to clean.
    for (const auto& p : orphans)
        std::filesystem::remove_all(p, ec);
}

}

scoped_directory::scoped_directory(std::string_view prefix) {
    // Windows may name the temporary directory by its short 8.3 name, and
    // git prints the long one.
    const auto temp =
        std::filesystem::canonical(std::filesystem::temp_directory_path());
    remove_orphans(temp, prefix);

    static std::atomic<std::uint64_t> counter = 0;
    const auto stem = std::string(prefix) + "." +
        std::to_string(process::current_process_id()) + ".";
    do
        path_ = temp / (stem + std::to_string(counter++));
    while (!std::filesystem::create_directory(path_));
}

scoped_directory::~scoped_directory() {
    // A file still open on Windows cannot be removed; the next run removes
    // the directory once this process has exited.
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
}

}
