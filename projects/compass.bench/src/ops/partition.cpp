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
#include "compass.bench/ops/partition.hpp"

#include <algorithm>
#include <numeric>

namespace compass::bench::ops {

std::vector<types::file_range> balance_ranges(
    std::span<const std::uint64_t> sizes, std::size_t count) {
    count = std::max<std::size_t>(count, 1);
    const auto total =
        std::accumulate(sizes.begin(), sizes.end(), std::uint64_t{0});
    const auto files = static_cast<std::uint32_t>(sizes.size());

    // Range i ends at the first file whose running total reaches i+1 shares
    // of the bytes, so no range can overshoot its share by more than the one
    // file that crossed the boundary.
    std::vector<types::file_range> r(count);
    std::uint32_t end = 0;
    std::uint64_t running = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const auto begin = end;
        if (i + 1 == count) {
            end = files;
        } else {
            const auto target = static_cast<std::uint64_t>(
                (static_cast<unsigned __int128>(total) * (i + 1)) / count);
            while (end < files && running < target)
                running += sizes[end++];
        }
        r[i] = {begin, end};
    }
    return r;
}

}
