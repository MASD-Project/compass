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
#include "compass.cli/ops/ratchet.hpp"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include "compass.store/ops/blob_hash.hpp"

namespace compass::cli::ops {

namespace {

using key = std::tuple<std::string, std::string, std::string>;

key key_of(const types::finding& f, std::string_view path) {
    if (!f.drift)
        return {std::string(path), f.rule, f.message};
    const auto hex = [](const auto& id) {
        return id ? store::ops::to_hex(*id) : std::string("none");
    };
    return {std::string(path), f.rule,
        hex(f.drift->file) + " " + hex(f.drift->generated)};
}

}

std::vector<types::rename> exact_renames(
    const store::types::commit_files& base,
    const store::types::commit_files& head,
    std::span<const std::string_view> gone) {
    std::map<store::types::object_id, std::vector<std::string_view>> from;
    for (const auto path : gone) {
        if (const auto row = base.find(path))
            from[base.ids[*row]].push_back(path);
    }
    std::map<store::types::object_id, std::vector<std::string_view>> to;
    for (std::size_t row = 0; row < head.size(); ++row) {
        if (from.contains(head.ids[row]) && !base.find(head.paths[row]))
            to[head.ids[row]].push_back(head.paths[row]);
    }
    std::vector<types::rename> out;
    for (const auto& [id, paths] : from) {
        const auto& targets = to[id];
        for (std::size_t i = 0; i < paths.size() && i < targets.size(); ++i)
            out.push_back({std::string(paths[i]), std::string(targets[i])});
    }
    return out;
}

types::ratchet_split split_new(std::span<const types::finding> base,
    std::span<const types::finding> current,
    std::span<const types::rename> renames) {
    std::map<std::string_view, std::string_view> moved;
    for (const auto& r : renames)
        moved.emplace(r.from, r.to);
    std::map<key, std::multiset<std::uint32_t>> lines;
    for (const auto& f : base) {
        const auto it = moved.find(f.path);
        lines[key_of(f, it == moved.end() ? std::string_view(f.path)
                                          : it->second)].insert(f.line);
    }

    std::vector<decltype(lines)::iterator> groups;
    std::vector<bool> excused(current.size());
    for (std::size_t i = 0; i < current.size(); ++i) {
        const auto group = lines.find(key_of(current[i], current[i].path));
        groups.push_back(group);
        if (group == lines.end())
            continue;
        if (const auto line = group->second.find(current[i].line);
            line != group->second.end()) {
            group->second.erase(line);
            excused[i] = true;
        }
    }
    types::ratchet_split out;
    for (std::size_t i = 0; i < current.size(); ++i) {
        const auto group = groups[i];
        if (!excused[i] && group != lines.end() && !group->second.empty()) {
            group->second.erase(group->second.begin());
            excused[i] = true;
        }
        if (excused[i])
            ++out.excused;
        else
            out.fresh.push_back(current[i]);
    }
    return out;
}

}
