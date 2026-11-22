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
#include "compass.cli/ops/attribution.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <unordered_set>

namespace compass::cli::ops {

namespace {

namespace ct = compass::codec::types;

std::uint32_t end_of(const ct::dependency_record& record,
    std::uint32_t unit) {
    return unit + 1 < record.size()
        ? record.input_firsts[ct::record_unit_id{unit + 1}].value
        : static_cast<std::uint32_t>(record.input_count());
}

std::uint32_t files_end(const ct::dependency_record& record,
    std::uint32_t unit) {
    return unit + 1 < record.size()
        ? record.file_firsts[ct::record_unit_id{unit + 1}].value
        : static_cast<std::uint32_t>(record.files.size());
}

}

std::unordered_map<std::string_view, producer> producers_of(
    const ct::dependency_record& record) {
    std::unordered_map<std::string_view, producer> out;
    for (std::uint32_t u = 0; u < record.size(); ++u) {
        const ct::record_unit_id unit{u};
        for (auto f = record.file_firsts[unit].value; f < files_end(record, u);
             ++f) {
            const ct::file_id file{f};
            out.insert_or_assign(record.files.paths[file], producer{u, file});
        }
    }
    return out;
}

bool matches_listing(std::string_view pattern,
    std::string_view path) noexcept {
    const auto star = pattern.find('*');
    const auto prefix = pattern.substr(0, star);
    const auto suffix = star == std::string_view::npos
        ? std::string_view{} : pattern.substr(star + 1);
    return path.size() >= prefix.size() + suffix.size() &&
        path.starts_with(prefix) && path.ends_with(suffix);
}

std::vector<std::uint8_t> units_reading(const ct::dependency_record& record,
    std::span<const std::string> paths) {
    std::vector<std::uint8_t> out(record.size(), 0);
    if (paths.empty())
        return out;
    const std::unordered_set<std::string_view> wanted(paths.begin(),
        paths.end());
    for (std::uint32_t u = 0; u < record.size(); ++u) {
        if (wanted.contains(record.documents[ct::record_unit_id{u}])) {
            out[u] = 1;
            continue;
        }
        const auto first = record.input_firsts[ct::record_unit_id{u}].value;
        for (auto i = first; i < end_of(record, u) && out[u] == 0; ++i) {
            const ct::input_id in{i};
            const auto key = record.input_keys[in];
            switch (record.input_kinds[in]) {
            case ct::input_kind::path:
            case ct::input_kind::file:
            case ct::input_kind::file_exists:
                out[u] = wanted.contains(key) ? 1 : 0;
                break;
            case ct::input_kind::listing:
                out[u] = std::ranges::any_of(paths,
                    [&](const std::string& p) {
                        return matches_listing(key, p);
                    }) ? 1 : 0;
                break;
            case ct::input_kind::id:
            case ct::input_kind::type_set:
            case ct::input_kind::type_registry:
            case ct::input_kind::fact:
                break;
            }
        }
    }
    return out;
}

types::change_cause cause_of(
    const std::optional<store::types::blob_id>& generated,
    const std::optional<store::types::blob_id>& at_head,
    const std::optional<store::types::blob_id>& committed) {
    if (generated != at_head)
        return types::change_cause::yours;
    if (!generated || committed != at_head)
        return types::change_cause::stale;
    return types::change_cause::yours;
}

void narrow(ct::output_set& outputs,
    const std::function<bool(std::size_t row)>& keep) {
    ct::output_set kept;
    std::set<std::string_view> dropped;
    for (std::size_t row = 0; row < outputs.size(); ++row) {
        const ct::file_id f{static_cast<std::uint32_t>(row)};
        if (keep(row)) {
            kept.add(outputs.codecs[f], outputs.files.paths[f],
                outputs.files.contents[f], outputs.hashes[f]);
            continue;
        }
        dropped.insert(outputs.codecs[f]);
    }
    if (dropped.empty())
        return;
    for (auto& codec : outputs.complete) {
        if (!dropped.contains(codec))
            kept.complete.push_back(std::move(codec));
    }
    outputs = std::move(kept);
}

std::string producer_label(const types::attributed_file& f) {
    auto out = f.codec;
    for (const auto* part : {&f.unit, &f.origin}) {
        if (!part->empty())
            out += ", " + *part;
    }
    return out;
}

std::vector<std::pair<std::string, std::vector<std::size_t>>> groups_of(
    std::span<const types::attributed_file> files, types::change_cause cause,
    std::size_t most) {
    const auto group = [&](auto key_of) {
        std::map<std::string, std::vector<std::size_t>> by;
        for (std::size_t i = 0; i < files.size(); ++i) {
            if (files[i].cause == cause)
                by[key_of(files[i])].push_back(i);
        }
        return by;
    };
    auto by = group([](const types::attributed_file& f) {
        return f.origin.empty() ? producer_label(f) : f.codec + ", " + f.origin;
    });
    if (by.size() > most)
        by = group([](const types::attributed_file& f) { return f.codec; });
    std::vector<std::pair<std::string, std::vector<std::size_t>>> out(
        std::make_move_iterator(by.begin()), std::make_move_iterator(by.end()));
    std::ranges::stable_sort(out, std::ranges::greater{},
        [](const auto& g) { return g.second.size(); });
    return out;
}

}
