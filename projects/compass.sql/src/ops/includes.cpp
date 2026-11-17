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
#include "compass.sql/ops/includes.hpp"

#include <algorithm>
#include <iterator>

namespace compass::sql::ops {

namespace {

namespace ct = compass::codec::types;

/**
 * @brief @p name from directory @p base, with "." and empty components
 * dropped and ".." taken lexically, as resolve() leaves a path with no
 * symbolic link; nothing when it climbs out of the checkout.
 */
std::optional<std::string> join(std::string_view base, std::string_view name) {
    std::vector<std::string_view> parts;
    const auto add = [&](std::string_view path) {
        for (std::size_t start = 0; start <= path.size();) {
            auto end = path.find('/', start);
            if (end == std::string_view::npos)
                end = path.size();
            const auto part = path.substr(start, end - start);
            start = end + 1;
            if (part.empty() || part == ".")
                continue;
            if (part == "..") {
                if (parts.empty())
                    return false;
                parts.pop_back();
                continue;
            }
            parts.push_back(part);
        }
        return true;
    };
    if (name.starts_with('/') || !add(base) || !add(name))
        return std::nullopt;
    std::string out;
    for (const auto p : parts) {
        if (!out.empty())
            out += '/';
        out += p;
    }
    return out;
}

struct walker {
    const std::function<std::vector<sql_include>(std::string_view)>&
        includes_of;
    const std::function<bool(std::string_view)>& exists;
    include_walk& out;
    std::uint32_t next_run = 0;

    void visit(const std::string& path) {
        if (out.runs.contains(path))
            return;
        auto& runs = out.runs[path];
        runs.emplace_back(0, next_run++);
        const auto dir = std::string_view(path).substr(0, path.rfind('/'));
        for (const auto& in : includes_of(path)) {
            const auto target = join(in.relative ? dir : sql_root, in.path);
            if (!target || !exists(*target)) {
                out.missing_includes.push_back({path, in.offset, in.path});
                continue;
            }
            visit(*target);
            out.runs[path].emplace_back(in.offset + 1, next_run++);
        }
    }
};

}

void write_includes(ct::fact_writer& fact,
    std::span<const sql_include> includes) {
    fact.number(static_cast<std::uint32_t>(includes.size()));
    for (const auto& in : includes)
        fact.number(in.relative ? 1 : 0).text(in.path).number(in.offset);
}

std::vector<sql_include> read_includes(ct::fact_reader& fact) {
    std::vector<sql_include> out(fact.number());
    for (auto& in : out) {
        in.relative = fact.number() == 1;
        in.path = fact.text();
        in.offset = fact.number();
    }
    return out;
}

std::optional<run_point> include_walk::when(std::string_view path,
    std::uint32_t offset) const {
    const auto it = runs.find(path);
    if (it == runs.end())
        return std::nullopt;
    const auto& r = it->second;
    const auto after = std::ranges::upper_bound(r, offset, {},
        [](const auto& run) { return run.first; });
    return run_point{std::prev(after)->second, offset};
}

include_walk walk_includes(std::span<const std::string_view> roots,
    const std::function<std::vector<sql_include>(std::string_view)>&
        includes_of,
    const std::function<bool(std::string_view)>& exists) {
    include_walk out;
    walker w{includes_of, exists, out};
    for (const auto root : roots)
        w.visit(std::string(root));
    for (const auto name : extra_entry_points) {
        const auto path = std::string(sql_root) + "/" + std::string(name);
        if (exists(path))
            w.visit(path);
        else
            out.missing_entry_points.emplace_back(name);
    }
    return out;
}

}
