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
#include "compass.store/io/id_resolver.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <set>
#include <unordered_set>
#include "compass.store/io/git_files.hpp"
#include "compass.store/io/location.hpp"
#include "compass.store/ops/git_index.hpp"
#include "compass.store/ops/git_settings.hpp"

namespace compass::store::io {

namespace {

constexpr int max_include_depth = 10;

std::optional<std::string> read_text(const std::filesystem::path& file) {
    std::ifstream is(file, std::ios::binary);
    if (!is)
        return std::nullopt;
    return std::string(std::istreambuf_iterator<char>(is), {});
}

std::filesystem::path environment(const char* name) {
    const char* value = std::getenv(name);
    return value == nullptr ? std::filesystem::path{} : value;
}

/**
 * @brief A path as git's configuration names it: "~/" is the home
 * directory, and a relative path is relative to @p directory.
 */
std::filesystem::path expand(std::string_view named,
    const std::filesystem::path& directory) {
    if (named.starts_with("~/"))
        return environment("HOME") / named.substr(2);
    const std::filesystem::path p(named);
    return p.is_absolute() ? p : directory / p;
}

std::filesystem::path xdg_git_dir() {
    const auto xdg = environment("XDG_CONFIG_HOME");
    if (!xdg.empty())
        return xdg / "git";
    const auto home = environment("HOME");
    return home.empty() ? home : home / ".config" / "git";
}

void read_config_file(const std::filesystem::path& file,
    types::git_config& config, int depth) {
    const auto text = read_text(file);
    if (!text || depth > max_include_depth)
        return;
    types::git_config local;
    ops::read_git_config(*text, local);
    config.autocrlf = config.autocrlf || local.autocrlf;
    config.sha1 = config.sha1 && local.sha1;
    for (const auto& named : local.attributes_files)
        config.attributes_files.push_back(
            expand(named, file.parent_path()).string());
    for (const auto& named : local.includes) {
        config.includes.push_back(named);
        read_config_file(expand(named, file.parent_path()), config,
            depth + 1);
    }
}

/**
 * @brief Every configuration file git may read for this checkout, read
 * whether or not git would, since reading more can only add distrust.
 */
types::git_config read_config(const std::filesystem::path& git_dir,
    const std::filesystem::path& common_dir) {
    std::vector<std::filesystem::path> files;
    if (std::getenv("GIT_CONFIG_NOSYSTEM") == nullptr)
        files.emplace_back("/etc/gitconfig");
    if (const auto xdg = xdg_git_dir(); !xdg.empty())
        files.push_back(xdg / "config");
    if (const auto home = environment("HOME"); !home.empty())
        files.push_back(home / ".gitconfig");
    files.push_back(common_dir / "config");
    files.push_back(git_dir / "config.worktree");

    types::git_config config;
    for (const auto& file : files)
        read_config_file(file, config, 0);
    return config;
}

void read_attributes_file(const std::filesystem::path& file,
    std::string_view base, types::attribute_rules& rules) {
    if (const auto text = read_text(file))
        ops::read_attributes(*text, base, rules);
}

types::attribute_rules read_attribute_rules(
    const std::filesystem::path& checkout,
    const std::filesystem::path& common_dir, const types::git_config& config,
    const types::git_index& index) {
    types::attribute_rules rules;
    read_attributes_file("/etc/gitattributes", "", rules);
    if (const auto xdg = xdg_git_dir(); !xdg.empty())
        read_attributes_file(xdg / "attributes", "", rules);
    for (const auto& file : config.attributes_files)
        read_attributes_file(file, "", rules);
    read_attributes_file(checkout / ".gitattributes", "", rules);
    read_attributes_file(common_dir / "info" / "attributes", "", rules);

    std::set<std::string_view> directories;
    for (const auto& path : index.paths) {
        for (auto slash = path.find('/'); slash != std::string::npos;
            slash = path.find('/', slash + 1))
            directories.insert(std::string_view(path).substr(0, slash + 1));
    }
    for (const auto directory : directories)
        read_attributes_file(checkout / directory / ".gitattributes",
            directory, rules);
    return rules;
}

}

id_resolver::id_resolver(const std::filesystem::path& checkout)
    : checkout_(checkout) {
    const auto dir = git_dir(checkout);
    const auto common_dir = git_common_dir(checkout);
    config_ = read_config(dir, common_dir);
    if (const auto file = dir / "index"; std::filesystem::exists(file))
        index_ = read_git_index(file);
    if (config_.autocrlf || !config_.sha1)
        return;

    attributes_ = read_attribute_rules(checkout, common_dir, config_, index_);
    std::unordered_set<std::string_view> conflicted;
    for (std::uint32_t row = 0; row < index_.size(); ++row) {
        if (index_.stages[row] != 0)
            conflicted.insert(index_.paths[row]);
    }
    for (std::uint32_t row = 0; row < index_.size(); ++row) {
        if (index_.stages[row] == 0 && index_.intent_to_add[row] == 0 &&
            ops::is_regular_file(index_.modes[row]) &&
            !conflicted.contains(index_.paths[row]) &&
            !ops::converts(attributes_, index_.paths[row]))
            trusted_.push_back(row);
    }
    std::ranges::sort(trusted_, {},
        [this](std::uint32_t row) -> const std::string& {
            return index_.paths[row];
        });
}

std::optional<types::blob_id> id_resolver::index_id(
    std::string_view path) const {
    const auto found = std::ranges::lower_bound(trusted_, path, {},
        [this](std::uint32_t row) -> std::string_view {
            return index_.paths[row];
        });
    if (found == trusted_.end() || index_.paths[*found] != path)
        return std::nullopt;
    const auto file = stat_of(checkout_ / path);
    if (!file || !ops::is_clean(index_.stats[*found], *file, index_.written))
        return std::nullopt;
    return index_.ids[*found];
}

resolved_id id_resolver::resolve(std::string_view path) const {
    if (const auto id = index_id(path))
        return {*id, id_source::index};
    return {hash_file(checkout_ / path), id_source::hashed};
}

}
