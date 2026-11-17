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
#include "compass.codec/io/action_cache.hpp"

#include <atomic>
#include <memory>
#include "compass.codec/io/dependency_file.hpp"
#include "compass.codec/ops/action_cache.hpp"
#include "compass.codec/ops/parallel_for.hpp"
#include "compass.store/io/location.hpp"
#include "compass.store/io/pack_store.hpp"

namespace compass::codec::io {

namespace {

using compass::store::types::action_kind;

}

types::action_source action_source_of(
    const std::filesystem::path& git_common_dir) {
    return [store = std::make_shared<const store::io::pack_reader>(
                store::io::packs_dir(git_common_dir))](action_kind kind,
               const store::types::blob_id& key) {
        return store->read_action(kind, key);
    };
}

action_writes store_actions(const std::filesystem::path& git_common_dir,
    const types::dependency_record& record,
    std::span<const types::record_unit_id> fresh, unsigned threads,
    std::span<const std::string> scans) {
    if (fresh.empty())
        return {};
    const auto packs = store::io::packs_dir(git_common_dir);
    const store::io::pack_reader store(packs);
    store::io::pack_writer writes(packs);
    std::atomic<std::uint64_t> results_written{0};
    std::atomic<std::uint64_t> manifests_written{0};
    std::atomic<std::uint64_t> unchanged{0};
    std::atomic<std::uint64_t> bytes{0};
    ops::parallel_for(fresh.size(), threads, [&](std::size_t i) {
        const auto u = fresh[i];
        const auto result = ops::encode_result(record, u,
            scans.empty() ? std::string_view{} : std::string_view(scans[i]));
        writes.add_action(action_kind::result, record.results[u], result);
        ++results_written;
        bytes += result.size();

        const auto key = record.manifests[u];
        types::action_manifest manifest;
        if (const auto body = store.read_action(action_kind::manifest, key)) {
            if (auto decoded = ops::decode_manifest(*body))
                manifest = std::move(*decoded);
        }
        if (!ops::add_input_set(manifest, record, u)) {
            ++unchanged;
            return;
        }
        const auto encoded = ops::encode_manifest(manifest);
        writes.add_action(action_kind::manifest, key, encoded);
        ++manifests_written;
        bytes += encoded.size();
    });
    writes.publish();
    return {results_written, manifests_written, unchanged, bytes};
}

std::vector<store::types::blob_id> action_keys(
    const std::filesystem::path& git_dir) {
    const auto loaded = read_dependency_file(dependency_file_path(git_dir));
    const auto& record = loaded.record;
    std::vector<store::types::blob_id> keys;
    keys.reserve(2 * record.size());
    keys.insert(keys.end(), record.manifests.values.begin(),
        record.manifests.values.end());
    keys.insert(keys.end(), record.results.values.begin(),
        record.results.values.end());
    return keys;
}

}
