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
#include <time.h>
#include <unordered_set>
#include "compass.store/io/git_files.hpp"
#include "compass.store/io/location.hpp"
#include "compass.store/io/object_file.hpp"
#include "compass.store/ops/git_index.hpp"
#include "compass.store/ops/git_settings.hpp"
#include "compass.store/ops/worktree_index.hpp"

namespace compass::store::io {

namespace {

constexpr int max_include_depth = 10;

std::optional<std::string> read_text(const std::filesystem::path& file) {
    std::ifstream is(file, std::ios::binary);
    if (!is)
        return std::nullopt;
    return std::string(std::istreambuf_iterator<char>(is), {});
}

/**
 * @brief The time as the kernel stamps a file it modifies. Local file
 * systems take the coarse clock, so a file modified after this call has a
 * modification time no earlier than the answer.
 */
types::index_time now() noexcept {
    struct timespec t{};
    ::clock_gettime(CLOCK_REALTIME_COARSE, &t);
    return {static_cast<std::uint32_t>(t.tv_sec),
        static_cast<std::uint32_t>(t.tv_nsec)};
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
    std::string_view previous;
    for (const std::string_view path : index.paths) {
        const auto directory = path.substr(0, path.rfind('/') + 1);
        if (directory == previous)
            continue;
        previous = directory;
        for (auto slash = directory.find('/'); slash != std::string::npos;
            slash = directory.find('/', slash + 1))
            directories.insert(directory.substr(0, slash + 1));
    }
    for (const auto directory : directories)
        read_attributes_file(checkout / directory / ".gitattributes",
            directory, rules);
    return rules;
}

}

id_resolver::id_resolver(const std::filesystem::path& checkout)
    : checkout_(checkout), started_(now()) {
    const auto dir = git_dir(checkout);
    const auto common_dir = git_common_dir(checkout);
    worktree_file_ = worktree_index_path(dir);
    // Either index can be unusable, and that costs speed but not
    // correctness: its IDs are not used and the reason is kept.
    try {
        if (const auto bytes = read_text(worktree_file_))
            worktree_ = ops::decode_worktree_index(*bytes);
    } catch (const std::runtime_error& e) {
        worktree_problem_ = e.what();
    }
    config_ = read_config(dir, common_dir);
    if (const auto file = dir / "index"; std::filesystem::exists(file)) {
        try {
            index_ = read_git_index(file);
        } catch (const std::runtime_error& e) {
            index_problem_ = e.what();
        }
    }
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

std::optional<resolved_id> id_resolver::known_id(std::string_view path) const {
    const auto trusted = std::ranges::lower_bound(trusted_, path, {},
        [this](std::uint32_t row) -> std::string_view {
            return index_.paths[row];
        });
    const bool in_index =
        trusted != trusted_.end() && index_.paths[*trusted] == path;
    const auto entry = std::ranges::lower_bound(worktree_.paths, path);
    const bool in_worktree =
        entry != worktree_.paths.end() && *entry == path;
    if (!in_index && !in_worktree)
        return std::nullopt;

    const auto file = stat_of(checkout_ / path);
    if (!file)
        return std::nullopt;
    if (in_index && ops::is_clean(index_.stats[*trusted], *file, index_.written))
        return resolved_id{index_.ids[*trusted], id_source::git_index, file};
    const auto row = entry - worktree_.paths.begin();
    if (in_worktree &&
        ops::is_unchanged(worktree_.stats[row], *file, worktree_.written))
        return resolved_id{worktree_.ids[row], id_source::worktree_index, file};
    return std::nullopt;
}

resolved_id id_resolver::resolve(std::string_view path) const {
    if (auto known = known_id(path))
        return *known;
    const auto file = checkout_ / path;
    const auto stat = stat_of(file);
    return {hash_file(file), id_source::hashed, stat};
}

std::size_t id_resolver::write_worktree_index(
    std::span<const std::string_view> paths,
    std::span<const resolved_id> resolved) const {
    struct entry {
        std::string_view path;
        types::stat_data stat;
        types::blob_id id;
    };
    std::vector<entry> entries;
    std::unordered_set<std::string_view> seen;
    for (std::size_t row = 0; row < paths.size(); ++row) {
        seen.insert(paths[row]);
        const auto& r = resolved[row];
        if (r.source != id_source::git_index && r.stat)
            entries.push_back({paths[row], *r.stat, r.id});
    }
    for (std::size_t row = 0; row < worktree_.size(); ++row) {
        const auto& path = worktree_.paths[row];
        if (seen.contains(path))
            continue;
        const auto file = stat_of(checkout_ / path);
        if (file && ops::is_unchanged(worktree_.stats[row], *file,
                worktree_.written))
            entries.push_back({path, worktree_.stats[row], worktree_.ids[row]});
    }
    std::ranges::sort(entries, {}, &entry::path);

    types::worktree_index next;
    next.written = started_;
    for (const auto& e : entries) {
        next.paths.emplace_back(e.path);
        next.stats.push_back(e.stat);
        next.ids.push_back(e.id);
    }
    const auto bytes = ops::encode_worktree_index(next);
    write_object(worktree_file_, std::as_bytes(std::span(bytes)));
    return next.size();
}

}
