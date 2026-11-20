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
#include "compass.bench/ops/store_stage.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>
#include "compass.bench/ops/partition.hpp"
#include "compass.ingest/org/parser.hpp"
#include "compass.org/io/dump.hpp"
#include "compass.org/io/object.hpp"
#include "compass.org/types/tables.hpp"
#include "compass.platform/filesystem/mapped_file.hpp"
#include "compass.store/io/location.hpp"
#include "compass.store/io/pack_store.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "compass.store/types/blob_id.hpp"

namespace compass::bench::ops {

namespace {

using platform::filesystem::mapped_file;
namespace ot = compass::org::types;

enum class warm { map, verify };

/**
 * @brief The org files of a checkout and, once the cold run has stored
 * them, the object of each. A row is one file; stored is false for a row
 * the cold run failed on.
 */
struct corpus {
    const std::filesystem::path& checkout;
    const types::file_table& table;
    const std::filesystem::path& packs;
    std::vector<std::uint32_t> ids;
    std::vector<store::types::blob_id> blobs;
    std::vector<std::uint64_t> object_sizes;
    std::vector<std::uint8_t> stored;

    [[nodiscard]] std::filesystem::path source(std::size_t row) const {
        return checkout / table.paths[ids[row]];
    }

    [[nodiscard]] compass::org::io::object_key key(std::size_t row) const {
        return {blobs[row].bytes, ingest::org::parser_version};
    }
};

struct tally {
    std::uint64_t files = 0;
    std::uint64_t bytes = 0;
    std::uint64_t errors = 0;
    std::uint64_t rejected = 0;
    std::uint64_t mismatches = 0;
    std::string first_error;
    std::string first_mismatch;

    void error(const std::system_error& e) {
        if (errors++ == 0)
            first_error = e.what();
    }
};

tally store_range(corpus& c, store::io::pack_writer& writes,
    types::file_range range) {
    tally t;
    ot::tables tables;
    std::vector<std::byte> bytes;
    for (std::size_t row = range.begin; row < range.end; ++row) {
        c.stored[row] = 0;
        try {
            const mapped_file file(c.source(row));
            c.blobs[row] = store::ops::blob_id_of(file.text());
            ingest::org::parse(file.text(), tables);
            compass::org::io::encode(ot::view(tables), c.key(row), bytes);
            writes.add(store::types::entry_kind::object, c.blobs[row], bytes);
            c.object_sizes[row] = bytes.size();
            c.stored[row] = 1;
            t.bytes += file.size();
            ++t.files;
        } catch (const std::system_error& e) {
            t.error(e);
        }
    }
    return t;
}

std::span<const std::byte> object_of(const store::io::pack_reader& store,
    const corpus& c, std::size_t row) {
    const auto bytes = store.find(store::types::entry_kind::object,
        c.blobs[row]);
    if (!bytes)
        throw std::system_error(
            std::make_error_code(std::errc::no_such_file_or_directory),
            "no object for " + c.table.paths[c.ids[row]]);
    return *bytes;
}

tally load_range(const store::io::pack_reader& store, const corpus& c,
    types::file_range range, warm how) {
    tally t;
    for (std::size_t row = range.begin; row < range.end; ++row) {
        if (c.stored[row] == 0)
            continue;
        try {
            const auto object = object_of(store, c, row);
            const auto loaded = compass::org::io::decode(object, c.key(row));
            const bool good = loaded.has_value() && (how == warm::map ||
                compass::org::io::checksum_matches(object));
            t.rejected += good ? 0 : 1;
            t.bytes += object.size();
            ++t.files;
        } catch (const std::system_error& e) {
            t.error(e);
            ++t.rejected;
        }
    }
    return t;
}

tally compare_range(const store::io::pack_reader& store, const corpus& c,
    types::file_range range) {
    tally t;
    ot::tables tables;
    for (std::size_t row = range.begin; row < range.end; ++row) {
        if (c.stored[row] == 0)
            continue;
        bool same = false;
        try {
            const mapped_file source(c.source(row));
            const auto object = object_of(store, c, row);
            ingest::org::parse(source.text(), tables);
            std::ostringstream parsed;
            compass::org::io::dump(parsed, source.text(), ot::view(tables));
            const auto loaded = compass::org::io::decode(object, c.key(row));
            if (loaded) {
                std::ostringstream mapped;
                compass::org::io::dump(mapped, source.text(), *loaded);
                same = parsed.view() == mapped.view();
            }
        } catch (const std::system_error& e) {
            t.error(e);
        }
        if (!same && t.mismatches++ == 0)
            t.first_mismatch = c.table.paths[c.ids[row]];
    }
    return t;
}

void count_objects(const corpus& c, types::store_result& r) {
    std::vector<std::pair<store::types::blob_id, std::uint64_t>> objects;
    for (std::uint32_t row = 0; row < c.ids.size(); ++row) {
        if (c.stored[row] != 0)
            objects.emplace_back(c.blobs[row], c.object_sizes[row]);
    }
    std::ranges::sort(objects);
    const auto repeated = std::ranges::unique(objects, {},
        [](const auto& o) { return o.first; });
    objects.erase(repeated.begin(), repeated.end());
    r.objects = objects.size();
    for (const auto& [id, size] : objects)
        r.object_bytes += size;
}

}

types::store_result run_store_stage(const std::filesystem::path& checkout,
    const types::file_table& table, const std::filesystem::path& packs,
    unsigned threads, unsigned repetitions) {
    types::store_result r;
    r.cold.name = "cold";
    corpus c{checkout, table, packs, {}, {}, {}, {}};
    std::vector<std::uint64_t> sizes;
    for (std::uint32_t id = 0; id < table.size(); ++id) {
        if (table.extensions[table.extension_ids[id]] == ".org") {
            c.ids.push_back(id);
            sizes.push_back(table.sizes[id]);
        }
    }
    c.blobs.resize(c.ids.size());
    c.object_sizes.assign(c.ids.size(), 0);
    c.stored.assign(c.ids.size(), 0);
    const auto ranges = balance_ranges(sizes, threads);

    std::optional<std::filesystem::path> written;
    for (unsigned rep = 0; rep < repetitions; ++rep) {
        if (written)
            std::filesystem::remove(*written);
        store::io::pack_writer writes(packs);
        const auto [ms, tallies] = run_ranges(ranges,
            [&](types::file_range range) {
                return store_range(c, writes, range);
            });
        const auto start = std::chrono::steady_clock::now();
        written = writes.publish_only();
        r.cold.wall_ms.push_back(ms + std::chrono::duration<double,
            std::milli>(std::chrono::steady_clock::now() - start).count());
        r.cold.files = r.cold.bytes = r.cold.errors = 0;
        r.cold.first_error.clear();
        for (const auto& t : tallies) {
            r.cold.files += t.files;
            r.cold.bytes += t.bytes;
            r.cold.errors += t.errors;
            if (r.cold.first_error.empty())
                r.cold.first_error = t.first_error;
        }
    }
    count_objects(c, r);

    for (const auto how : {warm::map, warm::verify}) {
        for (unsigned rep = 0; rep < repetitions; ++rep) {
            const auto start = std::chrono::steady_clock::now();
            const store::io::pack_reader store(packs);
            const auto opened = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start).count();
            const auto [ms, tallies] = run_ranges(ranges,
                [&](types::file_range range) {
                    return load_range(store, c, range, how);
                });
            (how == warm::map ? r.map_ms : r.verify_ms).push_back(
                opened + ms);
            r.rejected = 0;
            for (const auto& t : tallies)
                r.rejected += t.rejected;
        }
    }

    const store::io::pack_reader store(packs);
    const auto [ms, tallies] = run_ranges(ranges,
        [&](types::file_range range) {
            return compare_range(store, c, range);
        });
    for (const auto& t : tallies) {
        r.mismatches += t.mismatches;
        if (r.first_mismatch.empty())
            r.first_mismatch = t.first_mismatch;
    }
    return r;
}

}
