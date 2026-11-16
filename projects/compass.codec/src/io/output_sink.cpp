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
#include <cstddef>
#include <ctime>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include "compass.codec/ops/outputs.hpp"
#include "compass.store/io/git_files.hpp"
#include "compass.store/io/object_file.hpp"
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
    timespec t{};
    ::clock_gettime(CLOCK_REALTIME, &t);
    return {static_cast<std::uint32_t>(t.tv_sec),
        static_cast<std::uint32_t>(t.tv_nsec)};
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

sink_result write_outputs(const std::filesystem::path& root,
    const types::output_set& outputs,
    const types::written_outputs& previous) {
    const auto started = now();
    sink_result r;
    r.problems = ops::check_outputs(outputs);
    if (!r.problems.empty()) {
        r.record = previous;
        return r;
    }

    // Joining a name and taking the parent drops a trailing separator, so
    // the base compares equal to the parent of every top-level output.
    const auto base = (root / "x").parent_path();
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

    std::vector<entry> next;
    std::unordered_set<std::string_view> produced;
    for (std::uint32_t i = 0; i < outputs.size(); ++i) {
        const types::file_id f{i};
        const auto path = outputs.files.paths[f];
        const auto content = outputs.files.contents[f];
        const auto& hash = outputs.hashes[f];
        produced.insert(path);
        const auto target = base / path;
        const auto at = recorded(path);
        auto file = store::io::stat_of(target);
        const bool holds = at && trusted(*at, file)
            ? prev.hashes[*at] == hash
            : file && store::io::hash_file(target) == hash;
        if (holds) {
            ++r.unchanged;
        } else {
            store::io::write_object(target, std::as_bytes(std::span(content)));
            file = store::io::stat_of(target);
            if (!file)
                throw std::filesystem::filesystem_error(
                    "output is not a regular file after writing it", target,
                    std::make_error_code(std::errc::io_error));
            ++(at && prev.hashes[*at] == hash ? r.restored : r.written);
        }
        next.push_back({path, outputs.codecs[f], hash, *file});
    }

    const std::unordered_set<std::string_view> complete(
        outputs.complete.begin(), outputs.complete.end());
    for (std::size_t k = 0; k < prev.size(); ++k) {
        const auto& path = prev.paths[k];
        if (produced.contains(path))
            continue;
        if (!complete.contains(prev.codecs[k])) {
            next.push_back({path, prev.codecs[k], prev.hashes[k],
                prev.stats[k]});
            continue;
        }
        const auto target = base / path;
        const auto file = store::io::stat_of(target);
        if (!file)
            continue;
        if (!trusted(k, file) &&
            store::io::hash_file(target) != prev.hashes[k]) {
            ++r.kept;
            continue;
        }
        std::filesystem::remove(target);
        ++r.removed;
        remove_empty_directories(target.parent_path(), base);
    }

    std::ranges::sort(next, {}, &entry::path);
    const bool racy = std::ranges::any_of(prev.stats,
        [&](const st::stat_data& s) { return s.mtime.sec >= prev.written.sec; });
    r.record.root = base.string();
    r.record.written = same_entries(next, prev) && !racy
        ? prev.written : started;
    for (const auto& e : next) {
        r.record.paths.emplace_back(e.path);
        r.record.codecs.emplace_back(e.codec);
        r.record.hashes.push_back(e.hash);
        r.record.stats.push_back(e.stat);
    }
    return r;
}

}
