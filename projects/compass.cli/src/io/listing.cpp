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
#include "compass.cli/io/listing.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <sys/stat.h>
#include "compass.platform/process/run_program.hpp"
#include "compass.store/ops/git_object.hpp"

namespace compass::cli::io {

namespace {

/**
 * @brief Runs git in @p checkout with @p args and returns its standard
 * output.
 */
std::string run_git(const std::filesystem::path& checkout,
    std::vector<std::string> args) {
    const std::string dir = checkout.string();
    const auto command = args.front();
    args.insert(args.begin(), {"git", "-C", dir});
    auto r = platform::process::run_program({std::move(args), {}, {}});
    if (r.exit_code != 0)
        throw std::runtime_error("git " + command + " failed for " + dir);
    return std::move(r.output);
}

bool is_plain_file(const std::filesystem::path& path) {
    struct stat st {};
    return ::lstat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

}

std::vector<std::string> list_files(const std::filesystem::path& checkout) {
    const auto raw = run_git(checkout,
        {"ls-files", "-z", "--cached", "--others", "--exclude-standard"});
    std::vector<std::string> out;
    std::string_view rest = raw;
    while (!rest.empty()) {
        const auto end = rest.find('\0');
        const auto path = rest.substr(0, end);
        rest = end == std::string_view::npos ? std::string_view{}
                                             : rest.substr(end + 1);
        out.emplace_back(path);
    }
    // git lists a path with conflicts once per stage.
    std::ranges::sort(out);
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

store::types::object_id resolve_commit(const std::filesystem::path& checkout,
    std::string_view revision) {
    std::string out;
    try {
        out = run_git(checkout, {"rev-parse", "--verify", "--quiet",
            "--end-of-options", std::string(revision) + "^{commit}"});
    } catch (const std::runtime_error&) {
        throw std::runtime_error("no commit is named " +
            std::string(revision) + " in " + checkout.string());
    }
    while (!out.empty() && out.back() == '\n')
        out.pop_back();
    const auto id = store::ops::from_hex(out);
    if (!id)
        throw std::runtime_error("git rev-parse printed no commit ID for " +
            std::string(revision));
    return *id;
}

std::vector<std::string_view> org_files(const std::filesystem::path& checkout,
    const std::vector<std::string>& files) {
    std::vector<std::string_view> out;
    for (const auto& path : files) {
        if (path.ends_with(".org") && is_plain_file(checkout / path))
            out.emplace_back(path);
    }
    return out;
}

}
