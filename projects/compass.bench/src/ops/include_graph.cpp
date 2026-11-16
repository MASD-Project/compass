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
#include "compass.bench/ops/include_graph.hpp"

#include <algorithm>

namespace compass::bench::ops {

types::include_graph build_include_graph(const types::include_table& includes,
    std::size_t files) {
    types::include_graph g;
    g.offsets.assign(files + 1, 0);
    for (std::size_t i = 0; i < includes.size(); ++i) {
        if (includes.targets[i] < files)
            ++g.offsets[includes.includers[i] + 1];
    }
    for (std::size_t f = 0; f < files; ++f)
        g.offsets[f + 1] += g.offsets[f];

    g.targets.resize(g.offsets[files]);
    auto cursor = g.offsets;
    for (std::size_t i = 0; i < includes.size(); ++i) {
        if (includes.targets[i] < files)
            g.targets[cursor[includes.includers[i]]++] = includes.targets[i];
    }

    std::uint32_t write = 0;
    for (std::size_t f = 0; f < files; ++f) {
        const auto first = g.targets.begin() + g.offsets[f];
        const auto last = g.targets.begin() + g.offsets[f + 1];
        std::sort(first, last);
        const auto unique_end = std::unique(first, last);
        g.offsets[f] = write;
        write = static_cast<std::uint32_t>(
            std::copy(first, unique_end, g.targets.begin() + write) -
            g.targets.begin());
    }
    g.offsets[files] = write;
    g.targets.resize(write);
    return g;
}

std::vector<std::uint32_t> direct_fan_in(const types::include_graph& graph) {
    std::vector<std::uint32_t> r(graph.files(), 0);
    for (const auto t : graph.targets)
        ++r[t];
    return r;
}

std::vector<std::uint32_t> transitive_fan_in(
    const types::include_graph& graph, std::span<const std::uint32_t> roots) {
    const auto n = graph.files();
    std::vector<std::uint32_t> r(n, 0);
    std::vector<std::uint32_t> seen(n, 0);
    std::vector<std::uint32_t> stack;
    std::uint32_t stamp = 0;
    for (const auto root : roots) {
        ++stamp;
        seen[root] = stamp;
        stack.assign(1, root);
        while (!stack.empty()) {
            const auto f = stack.back();
            stack.pop_back();
            for (const auto t : graph.includes(f)) {
                if (seen[t] == stamp)
                    continue;
                seen[t] = stamp;
                ++r[t];
                stack.push_back(t);
            }
        }
    }
    return r;
}

}
