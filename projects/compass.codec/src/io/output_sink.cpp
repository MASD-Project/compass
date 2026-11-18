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
#include "compass.codec/io/output_sink.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include "compass.codec/ops/outputs.hpp"
#include "compass.codec/ops/unified_diff.hpp"
#include "compass.store/io/git_files.hpp"
#include "compass.store/io/object_file.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "compass.store/ops/git_index.hpp"

namespace compass::codec::io {

namespace {

namespace st = compass::store::types;

std::optional<std::string> read_file(const std::filesystem::path& path) {
    std::ifstream is(path, std::ios::binary);
    if (!is)
        return std::nullopt;
    return std::string(std::istreambuf_iterator<char>(is), {});
}

st::index_time now() noexcept {
    const auto since_epoch =
        std::chrono::system_clock::now().time_since_epoch();
    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(since_epoch);
    const auto nanoseconds =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            since_epoch - seconds);
    return {static_cast<std::uint32_t>(seconds.count()),
        static_cast<std::uint32_t>(nanoseconds.count())};
}

struct entry {
    std::string_view path;
    std::string_view codec;
    st::blob_id hash;
    st::stat_data stat;
};

/**
 * @brief Removes the directories from @p dir up to, not including,
 * @p base while each is empty.
 */
void remove_empty_directories(std::filesystem::path dir,
    const std::filesystem::path& base) {
    while (dir.native().size() > base.native().size()) {
        std::error_code error;
        if (!std::filesystem::is_empty(dir, error) || error)
            return;
        std::filesystem::remove(dir, error);
        if (error)
            return;
        dir = dir.parent_path();
    }
}

/**
 * @brief Whether a directory between @p base and the file named by @p path
 * is a symbolic link. Such a link could send a write or a removal outside
 * the output root, so no output may pass through one.
 */
bool passes_symlink(const std::filesystem::path& base, std::string_view path) {
    auto dir = base;
    for (std::size_t first = 0;;) {
        const auto slash = path.find('/', first);
        if (slash == std::string_view::npos)
            return false;
        dir /= std::string(path.substr(first, slash - first));
        std::error_code error;
        const auto type = std::filesystem::symlink_status(dir, error).type();
        if (error || type == std::filesystem::file_type::not_found)
            return false;
        if (type == std::filesystem::file_type::symlink)
            return true;
        first = slash + 1;
    }
}

bool same_entries(const std::vector<entry>& next,
    const types::written_outputs& previous) {
    if (next.size() != previous.size())
        return false;
    for (std::size_t i = 0; i < next.size(); ++i) {
        if (next[i].path != previous.paths[i] ||
            next[i].codec != previous.codecs[i] ||
            next[i].hash != previous.hashes[i] ||
            next[i].stat != previous.stats[i])
            return false;
    }
    return true;
}

}

std::filesystem::path written_outputs_path(
    const std::filesystem::path& git_dir) {
    return git_dir / "compass" / "outputs";
}

loaded_outputs read_written_outputs(const std::filesystem::path& path) {
    loaded_outputs r;
    const auto bytes = read_file(path);
    if (!bytes)
        return r;
    r.bytes = bytes->size();
    try {
        r.record = ops::decode_written_outputs(*bytes);
    } catch (const std::runtime_error& e) {
        r.problem = e.what();
    }
    return r;
}

bool write_written_outputs(const std::filesystem::path& path,
    const types::written_outputs& record) {
    const auto bytes = ops::encode_written_outputs(record);
    if (read_file(path) == bytes)
        return false;
    store::io::write_object(path, std::as_bytes(std::span(bytes)));
    return true;
}

output_plan plan_outputs(const std::filesystem::path& root,
    const types::output_set& outputs,
    const types::written_outputs& previous) {
    const auto started = now();
    output_plan p;
    p.problems = ops::check_outputs(outputs);
    if (!p.problems.empty()) {
        p.record = previous;
        return p;
    }

    // Joining a name and taking the parent drops a trailing separator, so
    // the base compares equal to the parent of every top-level output.
    const auto base = (root / "x").parent_path();
    for (std::uint32_t i = 0; i < outputs.size(); ++i) {
        const auto path = outputs.files.paths[types::file_id{i}];
        if (passes_symlink(base, path))
            p.problems.push_back("output path passes through a symbolic "
                                 "link: " + std::string(path));
    }
    if (!p.problems.empty()) {
        p.record = previous;
        return p;
    }
    const types::written_outputs none;
    const auto& prev = previous.root == base.string() ? previous : none;
    const auto recorded = [&](std::string_view path)
        -> std::optional<std::size_t> {
        const auto it = std::ranges::lower_bound(prev.paths, path);
        if (it == prev.paths.end() || *it != path)
            return std::nullopt;
        return static_cast<std::size_t>(it - prev.paths.begin());
    };
    const auto trusted = [&](std::size_t at,
        const std::optional<st::stat_data>& file) {
        return file && store::ops::is_unchanged(prev.stats[at], *file,
            prev.written);
    };
    const auto add = [&](std::string_view path, std::string_view codec,
        output_action action, std::string_view content) {
        p.paths.push_back(path);
        p.codecs.push_back(codec);
        p.actions.push_back(action);
        p.contents.push_back(content);
    };

    std::vector<entry> next;
    std::unordered_set<std::string_view> produced;
    for (std::uint32_t i = 0; i < outputs.size(); ++i) {
        const types::file_id f{i};
        const auto path = outputs.files.paths[f];
        const auto& hash = outputs.hashes[f];
        produced.insert(path);
        const auto target = base / path;
        const auto at = recorded(path);
        const auto file = store::io::stat_of(target);
        const bool holds = at && trusted(*at, file)
            ? prev.hashes[*at] == hash
            : file && store::io::hash_file(target) == hash;
        const auto action = holds ? output_action::unchanged
            : at && prev.hashes[*at] == hash ? output_action::restore
            : output_action::write;
        add(path, outputs.codecs[f], action, outputs.files.contents[f]);
        next.push_back({path, outputs.codecs[f], hash,
            file.value_or(st::stat_data{})});
    }

    const std::unordered_set<std::string_view> complete(
        outputs.complete.begin(), outputs.complete.end());
    for (std::size_t k = 0; k < prev.size(); ++k) {
        const auto& path = prev.paths[k];
        if (produced.contains(path))
            continue;
        const entry carried{path, prev.codecs[k], prev.hashes[k],
            prev.stats[k]};
        if (!complete.contains(prev.codecs[k])) {
            next.push_back(carried);
            continue;
        }
        const auto target = base / path;
        if (passes_symlink(base, path)) {
            next.push_back(carried);
            add(path, prev.codecs[k], output_action::keep, {});
            continue;
        }
        const auto file = store::io::stat_of(target);
        if (!file)
            continue;
        const bool edited = !trusted(k, file) &&
            store::io::hash_file(target) != prev.hashes[k];
        add(path, prev.codecs[k],
            edited ? output_action::keep : output_action::remove, {});
    }

    std::ranges::sort(next, {}, &entry::path);
    const bool racy = std::ranges::any_of(prev.stats,
        [&](const st::stat_data& s) { return s.mtime.sec >= prev.written.sec; });
    // A file written through a rename gets a new inode, so its entry
    // differs from the recorded one whatever the stat data was before.
    const bool writes = std::ranges::any_of(p.actions, [](output_action a) {
        return a == output_action::write || a == output_action::restore;
    });
    p.record.root = base.string();
    p.record.written = !writes && same_entries(next, prev) && !racy
        ? prev.written : started;
    for (const auto& e : next) {
        p.record.paths.emplace_back(e.path);
        p.record.codecs.emplace_back(e.codec);
        p.record.hashes.push_back(e.hash);
        p.record.stats.push_back(e.stat);
    }
    return p;
}

sink_result summarise(const output_plan& plan) {
    sink_result r;
    r.problems = plan.problems;
    for (const auto a : plan.actions) {
        switch (a) {
        case output_action::write: ++r.written; break;
        case output_action::restore: ++r.restored; break;
        case output_action::unchanged: ++r.unchanged; break;
        case output_action::remove: ++r.removed; break;
        case output_action::keep: ++r.kept; break;
        }
    }
    r.record = plan.record;
    return r;
}

sink_result apply_outputs(const output_plan& plan) {
    auto r = summarise(plan);
    const std::filesystem::path base = r.record.root;
    for (std::size_t i = 0; i < plan.size(); ++i) {
        const auto action = plan.actions[i];
        const auto target = base / plan.paths[i];
        if (action == output_action::remove) {
            std::filesystem::remove(target);
            remove_empty_directories(target.parent_path(), base);
            continue;
        }
        if (action != output_action::write &&
            action != output_action::restore)
            continue;
        store::io::write_object(target,
            std::as_bytes(std::span(plan.contents[i])));
        const auto file = store::io::stat_of(target);
        if (!file)
            throw std::filesystem::filesystem_error(
                "output is not a regular file after writing it", target,
                std::make_error_code(std::errc::io_error));
        const auto at = std::ranges::lower_bound(r.record.paths,
            plan.paths[i]);
        r.record.stats[static_cast<std::size_t>(
            at - r.record.paths.begin())] = *file;
    }
    return r;
}

sink_result write_outputs(const std::filesystem::path& root,
    const types::output_set& outputs,
    const types::written_outputs& previous) {
    return apply_outputs(plan_outputs(root, outputs, previous));
}

std::vector<hand_edit> find_hand_edits(const output_plan& plan,
    const types::written_outputs& previous) {
    std::vector<hand_edit> out;
    const std::filesystem::path base = plan.record.root;
    const bool same_root = previous.root == plan.record.root;
    for (std::size_t i = 0; i < plan.size(); ++i) {
        if (!changes_file(plan.actions[i]) ||
            plan.actions[i] == output_action::remove)
            continue;
        const auto target = base / plan.paths[i];
        if (!store::io::stat_of(target))
            continue;
        const auto disk = read_file(target);
        if (!disk)
            continue;
        const auto at = std::ranges::lower_bound(previous.paths,
            plan.paths[i]);
        if (same_root && at != previous.paths.end() && *at == plan.paths[i] &&
            previous.hashes[static_cast<std::size_t>(
                at - previous.paths.begin())] ==
                store::ops::blob_id_of(*disk))
            continue;
        if (auto block = ops::dropped_hand_lines(*disk, plan.contents[i]))
            out.push_back({std::string(plan.paths[i]), std::move(*block)});
    }
    return out;
}

std::string diff_outputs(const output_plan& plan) {
    const std::filesystem::path base = plan.record.root;
    std::string out;
    for (std::size_t i = 0; i < plan.size(); ++i) {
        const auto action = plan.actions[i];
        if (!changes_file(action))
            continue;
        const auto target = base / plan.paths[i];
        std::optional<std::string> before;
        if (store::io::stat_of(target)) {
            before = read_file(target);
            if (!before)
                throw std::filesystem::filesystem_error(
                    "cannot read output to compare it", target,
                    std::make_error_code(std::errc::io_error));
        }
        std::optional<std::string_view> after;
        if (action != output_action::remove)
            after = plan.contents[i];
        out += ops::unified_diff(plan.paths[i], before, after);
    }
    return out;
}

}
