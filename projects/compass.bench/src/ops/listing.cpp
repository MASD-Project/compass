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
#include "compass.bench/ops/listing.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include "compass.platform/filesystem/file_status.hpp"
#include "compass.platform/process/environment.hpp"
#include "compass.platform/process/run_program.hpp"

namespace compass::bench::ops {

namespace {

/**
 * @brief Runs git in @p checkout with @p args and returns its standard
 * output. Standard input is @p input when given, else inherited.
 */
std::string run_git(const std::filesystem::path& checkout,
    std::vector<std::string> args, const std::filesystem::path& input = {}) {
    const std::string dir = checkout.string();
    const auto command = args.front();
    args.insert(args.begin(), {"git", "-C", dir});
    auto r = platform::process::run_program({std::move(args), {}, input});
    if (r.exit_code != 0)
        throw std::runtime_error("git " + command + " failed for " + dir);
    return std::move(r.output);
}

}

std::string git_ls_files(const std::filesystem::path& checkout) {
    return run_git(checkout, {"ls-files", "-z"});
}

std::vector<std::string> git_hash_objects(
    const std::filesystem::path& checkout,
    std::span<const std::string_view> paths) {
    const auto input = std::filesystem::temp_directory_path() /
        ("compass-bench." +
            std::to_string(platform::process::current_process_id()) +
            ".paths");
    {
        std::ofstream os(input, std::ios::binary);
        for (const auto path : paths)
            os << path << '\n';
        if (!os)
            throw std::runtime_error("cannot write " + input.string());
    }
    std::string output;
    try {
        output = run_git(checkout,
            {"hash-object", "--no-filters", "--stdin-paths"}, input);
    } catch (...) {
        std::filesystem::remove(input);
        throw;
    }
    std::filesystem::remove(input);

    std::vector<std::string> ids;
    ids.reserve(paths.size());
    std::istringstream lines(output);
    for (std::string line; std::getline(lines, line);)
        ids.push_back(line);
    if (ids.size() != paths.size())
        throw std::runtime_error("git hash-object gave " +
            std::to_string(ids.size()) + " IDs for " +
            std::to_string(paths.size()) + " paths");
    return ids;
}

std::vector<std::string_view> parse_listing(std::string_view raw) {
    std::vector<std::string_view> r;
    while (!raw.empty()) {
        const auto nul = raw.find('\0');
        r.push_back(raw.substr(0, nul));
        if (nul == std::string_view::npos)
            break;
        raw.remove_prefix(nul + 1);
    }
    return r;
}

std::string_view extension_of(std::string_view path) {
    const auto slash = path.rfind('/');
    const auto name =
        slash == std::string_view::npos ? path : path.substr(slash + 1);
    const auto dot = name.rfind('.');
    if (dot == std::string_view::npos || dot == 0)
        return {};
    return name.substr(dot);
}

void add_file(types::file_table& table, std::string_view path,
    std::uint64_t size) {
    const auto extension = extension_of(path);
    auto it = table.extension_index.find(extension);
    if (it == table.extension_index.end()) {
        const auto id = static_cast<std::uint32_t>(table.extensions.size());
        table.extensions.emplace_back(extension);
        it = table.extension_index.emplace(table.extensions.back(), id).first;
    }
    table.paths.emplace_back(path);
    table.sizes.push_back(size);
    table.extension_ids.push_back(it->second);
}

types::file_table build_file_table(const std::filesystem::path& checkout,
    std::span<const std::string_view> entries) {
    types::file_table table;
    table.paths.reserve(entries.size());
    table.sizes.reserve(entries.size());
    table.extension_ids.reserve(entries.size());

    std::string full = checkout.string() + "/";
    const auto prefix = full.size();
    for (const auto entry : entries) {
        full.resize(prefix);
        full.append(entry);
        const auto status = platform::filesystem::regular_file_status(full);
        if (!status) {
            std::error_code error;
            if (!std::filesystem::exists(std::filesystem::symlink_status(
                    full, error)))
                add_file(table, entry, 0);
            continue;
        }
        add_file(table, entry, status->size);
    }
    return table;
}

}
