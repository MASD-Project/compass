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
#include "compass.store/io/collection.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "compass.store/io/git_files.hpp"
#include "compass.store/io/location.hpp"
#include "compass.store/ops/collection.hpp"
#include "compass.store/ops/worktree_index.hpp"

namespace compass::store::io {

namespace {

constexpr std::string_view temporary_prefix = "tmp_";

std::optional<types::blob_id> parse_hex(std::string_view hex) {
    constexpr auto digit = [](char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        return -1;
    };
    types::blob_id id;
    if (hex.size() != 2 * id.bytes.size())
        return std::nullopt;
    for (std::size_t i = 0; i < id.bytes.size(); ++i) {
        const int high = digit(hex[2 * i]);
        const int low = digit(hex[2 * i + 1]);
        if (high < 0 || low < 0)
            return std::nullopt;
        id.bytes[i] = static_cast<std::uint8_t>(high * 16 + low);
    }
    return id;
}

std::int64_t to_ns(const struct timespec& t) noexcept {
    return std::int64_t{t.tv_sec} * 1'000'000'000 + t.tv_nsec;
}

std::optional<struct stat> regular_file_status(
    const std::filesystem::path& file) {
    struct stat st{};
    if (::lstat(file.c_str(), &st) == -1 || !S_ISREG(st.st_mode))
        return std::nullopt;
    return st;
}

std::runtime_error unreadable(const std::filesystem::path& file,
    const std::exception& e) {
    return std::runtime_error("cannot read " + file.string() + ": " +
        e.what() + "; nothing collected");
}

void add_git_index(const std::filesystem::path& file,
    std::vector<types::blob_id>& ids) {
    if (!std::filesystem::exists(file))
        return;
    try {
        const auto index = read_git_index(file);
        ids.insert(ids.end(), index.ids.begin(), index.ids.end());
    } catch (const std::exception& e) {
        throw unreadable(file, e);
    }
}

void add_worktree_index(const std::filesystem::path& file,
    std::vector<types::blob_id>& ids) {
    std::ifstream is(file, std::ios::binary);
    if (!is)
        return;
    try {
        const std::string bytes(std::istreambuf_iterator<char>(is), {});
        const auto index = ops::decode_worktree_index(bytes);
        ids.insert(ids.end(), index.ids.begin(), index.ids.end());
    } catch (const std::exception& e) {
        throw unreadable(file, e);
    }
}

std::int64_t now() noexcept {
    struct timespec t{};
    ::clock_gettime(CLOCK_REALTIME, &t);
    return to_ns(t);
}

double since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
}

bool remove_unchanged(const std::filesystem::path& file,
    std::uint64_t size, std::int64_t mtime) {
    const auto st = regular_file_status(file);
    if (!st || static_cast<std::uint64_t>(st->st_size) != size ||
        to_ns(st->st_mtim) != mtime)
        return false;
    return ::unlink(file.c_str()) == 0;
}

void append(types::stored_objects& to, const types::stored_objects& from,
    std::string_view prefix) {
    for (std::size_t row = 0; row < from.size(); ++row) {
        to.names.push_back(std::string(prefix) + from.names[row]);
        to.ids.push_back(from.ids[row]);
        to.temporary.push_back(from.temporary[row]);
        to.sizes.push_back(from.sizes[row]);
        to.mtimes.push_back(from.mtimes[row]);
    }
}

}

std::vector<std::filesystem::path> worktree_git_dirs(
    const std::filesystem::path& git_common_dir) {
    std::vector<std::filesystem::path> dirs{git_common_dir};
    std::error_code ignored;
    for (const auto& entry : std::filesystem::directory_iterator(
             git_common_dir / "worktrees", ignored)) {
        const auto& dir = entry.path();
        if (std::filesystem::exists(dir / "index") ||
            std::filesystem::exists(worktree_index_path(dir)))
            dirs.push_back(dir);
    }
    std::sort(dirs.begin() + 1, dirs.end());
    return dirs;
}

std::vector<types::blob_id> live_ids(
    std::span<const std::filesystem::path> git_dirs) {
    std::vector<types::blob_id> ids;
    for (const auto& dir : git_dirs) {
        add_git_index(dir / "index", ids);
        add_worktree_index(worktree_index_path(dir), ids);
    }
    std::ranges::sort(ids);
    const auto [first, last] = std::ranges::unique(ids);
    ids.erase(first, last);
    return ids;
}

types::stored_objects list_objects(const std::filesystem::path& objects_dir) {
    types::stored_objects objects;
    std::error_code ignored;
    for (const auto& fan : std::filesystem::directory_iterator(objects_dir,
             ignored)) {
        const auto prefix = fan.path().filename().string();
        if (prefix.size() != 2 || !fan.is_directory(ignored))
            continue;
        for (const auto& entry : std::filesystem::directory_iterator(
                 fan.path(), ignored)) {
            const auto name = entry.path().filename().string();
            const bool temporary = name.starts_with(temporary_prefix);
            const auto id = parse_hex(prefix + name);
            if (!temporary && !id)
                continue;
            const auto st = regular_file_status(entry.path());
            if (!st)
                continue;
            objects.names.push_back(prefix + "/" + name);
            objects.ids.push_back(id.value_or(types::blob_id{}));
            objects.temporary.push_back(temporary ? 1 : 0);
            objects.sizes.push_back(static_cast<std::uint64_t>(st->st_size));
            objects.mtimes.push_back(to_ns(st->st_mtim));
        }
    }
    return objects;
}

types::collection_result collect(const std::filesystem::path& checkout,
    const std::filesystem::path& objects_dir,
    const types::collection_options& options, const action_roots& roots) {
    types::collection_result r;

    auto start = std::chrono::steady_clock::now();
    const auto common = git_common_dir(checkout);
    r.worktrees = worktree_git_dirs(common);
    r.worktrees_ms = since(start);

    start = std::chrono::steady_clock::now();
    auto live = live_ids(r.worktrees);
    r.live_ids = live.size();
    std::vector<types::blob_id> keys;
    for (const auto& dir : r.worktrees) {
        const auto named = roots(dir);
        keys.insert(keys.end(), named.begin(), named.end());
    }
    std::ranges::sort(keys);
    keys.erase(std::ranges::unique(keys).begin(), keys.end());
    r.live_keys = keys.size();
    live.insert(live.end(), keys.begin(), keys.end());
    std::ranges::sort(live);
    live.erase(std::ranges::unique(live).begin(), live.end());
    r.live_ms = since(start);

    start = std::chrono::steady_clock::now();
    auto objects = list_objects(objects_dir);
    const auto object_rows = objects.size();
    const auto compass_dir = common / "compass";
    for (const auto kind : {types::action_kind::manifest,
             types::action_kind::result}) {
        const auto dir = actions_dir(common, kind);
        const auto prefix = dir.filename().string() + "/";
        append(objects, list_objects(dir), prefix);
    }
    for (auto row = object_rows; row < objects.size(); ++row) {
        r.actions.add(objects.sizes[row]);
        if (objects.temporary[row] == 0 &&
            std::ranges::binary_search(keys, objects.ids[row]))
            r.live_actions.add(objects.sizes[row]);
    }
    r.list_ms = since(start);

    start = std::chrono::steady_clock::now();
    const auto grace = std::chrono::duration_cast<std::chrono::nanoseconds>(
        options.grace).count();
    r.plan = ops::plan_collection(objects, live, options.cap, now() - grace);
    for (const auto row : r.plan.victim_rows)
        r.victims.push_back(objects.names[row]);
    r.plan_ms = since(start);

    start = std::chrono::steady_clock::now();
    if (!options.dry_run) {
        for (const auto row : r.plan.victim_rows) {
            const auto& base = row < object_rows ? objects_dir : compass_dir;
            if (remove_unchanged(base / objects.names[row],
                    objects.sizes[row], objects.mtimes[row]))
                r.removed.add(objects.sizes[row]);
        }
    }
    r.remove_ms = since(start);
    return r;
}

}
