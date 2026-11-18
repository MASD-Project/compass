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
#include <exception>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include "compass.platform/filesystem/file_status.hpp"
#include "compass.store/io/git_files.hpp"
#include "compass.store/io/location.hpp"
#include "compass.store/io/pack_store.hpp"
#include "compass.store/ops/blob_hash.hpp"
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

using platform::filesystem::regular_file_status;

std::runtime_error unreadable(const std::filesystem::path& file,
    const std::exception& e) {
    return std::runtime_error("cannot read " + file.generic_string() + ": " +
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
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

double since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
}

bool remove_unchanged(const std::filesystem::path& file,
    std::uint64_t size, std::int64_t mtime) {
    const auto st = regular_file_status(file);
    if (!st || st->size != size || st->modified.count_ns() != mtime)
        return false;
    std::error_code error;
    return std::filesystem::remove(file, error);
}

/**
 * @brief Where an entry of the listing is: its pack and its row.
 */
struct place {
    std::size_t pack = 0;
    std::uint64_t row = 0;
    types::entry_kind kind = types::entry_kind::object;
};

/**
 * @brief The entries of a set of packs: those a later entry of the same
 * kind and key does not hide, as rows of stored with where each is, and
 * those it does.
 */
struct entry_listing {
    types::stored_objects stored;
    std::vector<place> places;
    std::vector<types::pack_entry> superseded;
};

std::string_view kind_name(types::entry_kind k) noexcept {
    switch (k) {
    case types::entry_kind::object: return "object/";
    case types::entry_kind::manifest: return "manifest/";
    case types::entry_kind::result: return "result/";
    }
    return "";
}

entry_listing list_entries(std::span<const open_pack> packs) {
    struct row {
        types::pack_entry entry;
        place at;
    };
    std::vector<row> rows;
    for (std::size_t p = 0; p < packs.size(); ++p) {
        for (std::uint64_t i = 0; i < packs[p].view.count; ++i) {
            const auto e = ops::entry_at(packs[p].view, i);
            rows.push_back({e, {p, i, e.kind}});
        }
    }
    std::ranges::sort(rows, [](const row& a, const row& b) {
        if (a.entry.key != b.entry.key)
            return a.entry.key < b.entry.key;
        if (a.entry.kind != b.entry.kind)
            return a.entry.kind < b.entry.kind;
        return a.entry.written > b.entry.written;
    });
    entry_listing out;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto& e = rows[i].entry;
        if (i != 0 && rows[i - 1].entry.key == e.key &&
            rows[i - 1].entry.kind == e.kind) {
            out.superseded.push_back(e);
            continue;
        }
        out.stored.names.push_back(std::string(kind_name(e.kind)) +
            ops::to_hex(e.key));
        out.stored.ids.push_back(e.key);
        out.stored.temporary.push_back(0);
        out.stored.sizes.push_back(e.size);
        out.stored.mtimes.push_back(e.written);
        out.places.push_back(rows[i].at);
    }
    return out;
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
            objects.sizes.push_back(st->size);
            objects.mtimes.push_back(st->modified.count_ns());
        }
    }
    return objects;
}

types::collection_result collect(const std::filesystem::path& checkout,
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
    const auto dir = packs_dir(common);
    std::optional<pack_lock> lock;
    if (!options.dry_run)
        lock.emplace(dir, true);
    std::vector<std::filesystem::path> rejected;
    const auto packs = open_packs(dir, &rejected);
    r.packs = packs.size();
    auto listed = list_entries(packs);
    auto& stored = listed.stored;
    for (std::size_t row = 0; row < listed.superseded.size(); ++row)
        r.superseded.add(listed.superseded[row].size);
    const auto entry_rows = stored.size();
    for (std::size_t row = 0; row < entry_rows; ++row) {
        if (listed.places[row].kind == types::entry_kind::object)
            continue;
        r.actions.add(stored.sizes[row]);
        if (std::ranges::binary_search(keys, stored.ids[row]))
            r.live_actions.add(stored.sizes[row]);
    }
    std::vector<std::filesystem::path> files;
    std::error_code ignored;
    for (const auto& entry : std::filesystem::directory_iterator(dir,
             ignored)) {
        const auto name = entry.path().filename().string();
        if (name.starts_with(temporary_prefix) ||
            std::ranges::contains(rejected, entry.path()))
            files.push_back(entry.path());
    }
    for (const auto& file : files) {
        const auto st = regular_file_status(file);
        if (!st)
            continue;
        stored.names.push_back(file.filename().string());
        stored.ids.push_back({});
        stored.temporary.push_back(1);
        stored.sizes.push_back(st->size);
        stored.mtimes.push_back(st->modified.count_ns());
    }
    std::vector<types::stored_objects> legacy;
    for (const auto& d : legacy_store_dirs(common)) {
        legacy.push_back(list_objects(d));
        for (const auto size : legacy.back().sizes)
            r.legacy.add(size);
    }
    r.list_ms = since(start);

    start = std::chrono::steady_clock::now();
    const auto grace = std::chrono::duration_cast<std::chrono::nanoseconds>(
        options.grace).count();
    r.plan = ops::plan_collection(stored, live, options.cap, now() - grace);
    for (const auto row : r.plan.victim_rows)
        r.victims.push_back(stored.names[row]);
    r.plan_ms = since(start);

    start = std::chrono::steady_clock::now();
    r.packs_after = r.packs;
    if (!options.dry_run) {
        std::vector<std::vector<std::uint8_t>> keep;
        for (const auto& p : packs)
            keep.emplace_back(p.view.count, std::uint8_t{0});
        for (std::size_t row = 0; row < entry_rows; ++row) {
            const auto& at = listed.places[row];
            keep[at.pack][at.row] = 1;
        }
        bool dropped = !listed.superseded.empty();
        for (const auto row : r.plan.victim_rows) {
            if (row < entry_rows) {
                const auto& at = listed.places[row];
                keep[at.pack][at.row] = 0;
                dropped = true;
            } else if (remove_unchanged(dir / stored.names[row],
                           stored.sizes[row], stored.mtimes[row])) {
                r.removed.add(stored.sizes[row]);
            }
        }
        if (dropped || packs.size() > 1) {
            const auto written = rewrite_packs(dir, packs, keep);
            r.packs_after = written ? 1 : 0;
            r.removed.objects += r.superseded.objects;
            r.removed.bytes += r.superseded.bytes;
            for (const auto row : r.plan.victim_rows) {
                if (row < entry_rows)
                    r.removed.add(stored.sizes[row]);
            }
        }
        for (const auto& d : legacy_store_dirs(common))
            std::filesystem::remove_all(d, ignored);
    }
    r.remove_ms = since(start);
    return r;
}

}
