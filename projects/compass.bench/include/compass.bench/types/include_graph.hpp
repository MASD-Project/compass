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
#ifndef COMPASS_BENCH_TYPES_INCLUDE_GRAPH_HPP
#define COMPASS_BENCH_TYPES_INCLUDE_GRAPH_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace compass::bench::types {

/**
 * @brief Include edges between files of the checkout, as compressed rows.
 *
 * The files file id f includes are targets[offsets[f], offsets[f + 1]),
 * ascending and without duplicates. offsets has one entry per file plus
 * one. External and unresolved includes are not edges.
 */
struct include_graph {
    std::vector<std::uint32_t> offsets;
    std::vector<std::uint32_t> targets;

    [[nodiscard]] std::size_t files() const noexcept {
        return offsets.empty() ? 0 : offsets.size() - 1;
    }

    [[nodiscard]] std::span<const std::uint32_t> includes(
        std::uint32_t file) const noexcept {
        return std::span(targets).subspan(offsets[file],
            offsets[file + 1] - offsets[file]);
    }
};

}

#endif
