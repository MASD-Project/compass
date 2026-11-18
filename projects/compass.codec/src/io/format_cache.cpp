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
#include "compass.codec/io/format_cache.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>
#include "compass.codec/ops/format_cache.hpp"
#include "compass.codec/ops/parallel_for.hpp"
#include "compass.codec/ops/registry.hpp"
#include "compass.platform/process/environment.hpp"
#include "compass.platform/process/run_program.hpp"
#include "compass.store/io/location.hpp"
#include "compass.store/io/pack_store.hpp"
#include "compass.store/ops/blob_hash.hpp"

namespace compass::codec::io {

namespace {

using compass::store::types::blob_id;

constexpr auto result = compass::store::types::action_kind::result;

struct key_hash {
    std::size_t operator()(const blob_id& id) const noexcept {
        std::size_t h = 0;
        std::memcpy(&h, id.bytes.data(), sizeof h);
        return h;
    }
};

std::string read_file(const std::filesystem::path& path) {
    std::ifstream is(path, std::ios::binary);
    if (!is)
        throw std::runtime_error("cannot read " + path.string());
    return {std::istreambuf_iterator<char>(is), {}};
}

void write_file(const std::filesystem::path& path, std::string_view text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream os(path, std::ios::binary);
    os << text;
    if (!os)
        throw std::runtime_error("cannot write " + path.string());
}

/**
 * @brief A directory of its own under the system's temporary directory,
 * removed with everything in it when this goes.
 */
class scratch_dir final {
public:
    scratch_dir() {
        static std::atomic<unsigned> counter{0};
        path_ = std::filesystem::temp_directory_path() /
            ("compass-format." +
                std::to_string(platform::process::current_process_id()) + "." +
                std::to_string(counter++));
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }
    ~scratch_dir() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    scratch_dir(const scratch_dir&) = delete;
    scratch_dir& operator=(const scratch_dir&) = delete;

    const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

/**
 * @brief The version @p program reports, read from the store when this
 * very program file reported it before, else by running it.
 */
std::string program_version(const store::io::pack_reader& store,
    store::io::pack_writer& writes, const std::filesystem::path& program,
    std::atomic<std::uint64_t>& runs) {
    const auto resolved = std::filesystem::canonical(program);
    const auto mtime = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::filesystem::last_write_time(resolved).time_since_epoch()).count();
    const auto key = ops::program_key(resolved.string(),
        std::filesystem::file_size(resolved), mtime);
    if (auto version = store.read_action(result, key))
        return std::move(*version);
    ++runs;
    auto r = platform::process::run_program(
        {{program.string(), "--version"}, {}, {}});
    if (r.exit_code != 0)
        throw std::runtime_error(program.string() + " --version exited with " +
            std::to_string(r.exit_code));
    writes.add_action(result, key, r.output);
    return std::move(r.output);
}

blob_id formatter_key(const store::io::pack_reader& store,
    store::io::pack_writer& writes, const types::formatter& f,
    std::atomic<std::uint64_t>& runs) {
    const auto style = f.style.empty()
        ? store::ops::blob_id_of({}) : store::ops::blob_id_of(read_file(f.style));
    return ops::formatter_key(f, program_version(store, writes, f.program,
        runs), style);
}

/**
 * @brief Formats the rows @p rows of @p raw with @p f, puts each formatted
 * form in @p texts and in the store under its key in @p keys. Rows that
 * share a path go under directories of their own, numbered by occurrence,
 * so each keeps its file name and none overwrites another.
 */
void format_misses(store::io::pack_writer& writes,
    const types::formatter& f, const types::file_table& raw,
    const std::vector<std::size_t>& rows, const std::vector<blob_id>& keys,
    std::vector<std::string>& texts, unsigned threads,
    std::atomic<std::uint64_t>& runs) {
    const scratch_dir dir;
    std::unordered_map<std::string_view, std::size_t> occurrences;
    std::vector<std::string> files;
    files.reserve(rows.size());
    for (const auto row : rows) {
        const types::file_id id{static_cast<std::uint32_t>(row)};
        const auto path = raw.paths[id];
        files.push_back(std::to_string(occurrences[path]++) + "/" +
            std::string(path));
        write_file(dir.path() / files.back(), raw.contents[id]);
    }
    const auto batches = (rows.size() + format_batch - 1) / format_batch;
    ops::parallel_for(batches, threads, [&](std::size_t b) {
        const auto first = b * format_batch;
        const auto last = std::min(rows.size(), first + format_batch);
        const std::span<const std::string> batch(files.data() + first,
            last - first);
        const auto command = ops::format_command(f, batch);
        for (unsigned pass = 0; pass < f.passes; ++pass) {
            ++runs;
            const auto r = platform::process::run_program(
                {command, dir.path(), {}});
            if (r.exit_code != 0)
                throw std::runtime_error(f.name + " exited with " +
                    std::to_string(r.exit_code));
        }
        for (auto i = first; i < last; ++i) {
            const auto row = rows[i];
            texts[row] = read_file(dir.path() / files[i]);
            writes.add_action(result, keys[row], texts[row]);
        }
    });
}

}

formatted_files format_files(const std::filesystem::path& git_common_dir,
    std::span<const types::formatter> formatters,
    const types::file_table& raw, unsigned threads) {
    const auto start = std::chrono::steady_clock::now();
    const auto packs = store::io::packs_dir(git_common_dir);
    const store::io::pack_reader store(packs);
    store::io::pack_writer writes(packs);
    std::atomic<std::uint64_t> runs{0};

    const auto n = raw.size();
    std::vector<std::optional<std::size_t>> chosen(n);
    std::vector<std::optional<blob_id>> formatter_keys(formatters.size());
    for (std::size_t i = 0; i < n; ++i) {
        chosen[i] = ops::formatter_of(formatters,
            raw.paths[types::file_id{static_cast<std::uint32_t>(i)}]);
        if (chosen[i] && !formatter_keys[*chosen[i]])
            formatter_keys[*chosen[i]] = formatter_key(store, writes,
                formatters[*chosen[i]], runs);
    }

    std::vector<blob_id> keys(n);
    std::vector<std::string> texts(n);
    std::vector<std::uint8_t> found(n, 0);
    ops::parallel_for(n, threads, [&](std::size_t i) {
        const types::file_id id{static_cast<std::uint32_t>(i)};
        if (!chosen[i])
            return;
        keys[i] = ops::formatted_key(*formatter_keys[*chosen[i]],
            raw.paths[id], store::ops::blob_id_of(raw.contents[id]));
        if (auto body = store.read_action(result, keys[i])) {
            texts[i] = std::move(*body);
            found[i] = 1;
        }
    });

    formatted_files out;
    std::vector<std::vector<std::size_t>> misses(formatters.size());
    std::unordered_map<blob_id, std::size_t, key_hash> first_miss;
    std::vector<std::pair<std::size_t, std::size_t>> copies;
    for (std::size_t i = 0; i < n; ++i) {
        if (!chosen[i]) {
            ++out.stats.unformatted;
        } else if (found[i] != 0) {
            ++out.stats.hits;
        } else {
            ++out.stats.formatted;
            const auto [it, added] = first_miss.try_emplace(keys[i], i);
            if (added)
                misses[*chosen[i]].push_back(i);
            else
                copies.emplace_back(i, it->second);
        }
    }
    for (std::size_t f = 0; f < formatters.size(); ++f) {
        if (!misses[f].empty())
            format_misses(writes, formatters[f], raw, misses[f], keys, texts,
                threads, runs);
    }
    for (const auto& [row, source] : copies)
        texts[row] = texts[source];

    for (std::size_t i = 0; i < n; ++i) {
        const types::file_id id{static_cast<std::uint32_t>(i)};
        out.files.add(raw.paths[id],
            chosen[i] ? std::string_view(texts[i]) : raw.contents[id]);
    }
    out.ids.resize(n);
    ops::parallel_for(n, threads, [&](std::size_t i) {
        out.ids[i] = store::ops::blob_id_of(
            out.files.contents[types::file_id{static_cast<std::uint32_t>(i)}]);
    });
    writes.publish();
    out.stats.runs = runs;
    out.stats.format_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    return out;
}

std::vector<std::optional<format_stats>> format_outputs(
    const types::registry& r, types::run_result& run,
    const std::filesystem::path& checkout,
    const std::filesystem::path& git_common_dir, unsigned threads) {
    std::vector<std::optional<format_stats>> out(r.size());
    run.formatted_ids.resize(r.size());
    for (std::uint32_t c = 0; c < r.size(); ++c) {
        const auto& d = r.codecs[c];
        if (r.enabled[c] == 0 || d.formatters == nullptr)
            continue;
        const auto formatters = d.formatters(
            {d.options, ops::option_values(r, types::codec_id{c})}, checkout);
        auto formatted = format_files(git_common_dir, formatters,
            run.files[c], threads);
        run.files[c] = std::move(formatted.files);
        run.formatted_ids[c] = std::move(formatted.ids);
        out[c] = formatted.stats;
    }
    return out;
}

}
