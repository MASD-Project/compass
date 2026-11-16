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
#include "compass.store/io/location.hpp"

#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include "compass.store/ops/blob_hash.hpp"

namespace compass::store::io {

namespace {

std::string first_line(const std::filesystem::path& file) {
    std::ifstream is(file);
    std::string line;
    std::getline(is, line);
    while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
        line.pop_back();
    return line;
}

std::filesystem::path resolve(const std::filesystem::path& base,
    const std::filesystem::path& named) {
    const auto path =
        (named.is_absolute() ? named : base / named).lexically_normal();
    return path.has_filename() ? path : path.parent_path();
}

}

std::filesystem::path git_common_dir(const std::filesystem::path& checkout) {
    constexpr std::string_view prefix = "gitdir: ";
    const auto dot_git = checkout / ".git";
    if (std::filesystem::is_directory(dot_git))
        return dot_git.lexically_normal();
    if (!std::filesystem::is_regular_file(dot_git))
        throw std::runtime_error("not a git checkout: " + checkout.string());

    const auto line = first_line(dot_git);
    if (!line.starts_with(prefix) || line.size() == prefix.size())
        throw std::runtime_error("no gitdir in " + dot_git.string());
    const auto git_dir = resolve(checkout, line.substr(prefix.size()));

    const auto common = first_line(git_dir / "commondir");
    return common.empty() ? git_dir : resolve(git_dir, common);
}

std::filesystem::path objects_dir(const std::filesystem::path& git_common_dir) {
    return git_common_dir / "compass" / "objects";
}

std::filesystem::path object_path(const std::filesystem::path& objects_dir,
    const types::blob_id& id) {
    const auto hex = ops::to_hex(id);
    return objects_dir / hex.substr(0, 2) / hex.substr(2);
}

}
