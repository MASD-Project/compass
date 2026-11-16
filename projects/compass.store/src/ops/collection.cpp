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
#include "compass.store/ops/collection.hpp"

#include <algorithm>
#include <vector>

namespace compass::store::ops {

types::collection_plan plan_collection(const types::stored_objects& objects,
    std::span<const types::blob_id> live, std::uint64_t cap,
    std::int64_t cutoff) {
    types::collection_plan plan;
    std::vector<std::uint32_t> candidates;
    for (std::uint32_t row = 0; row < objects.size(); ++row) {
        const auto size = objects.sizes[row];
        const bool old = objects.mtimes[row] < cutoff;
        plan.stored.add(size);
        if (objects.temporary[row] != 0) {
            plan.temporaries.add(size);
        } else if (std::ranges::binary_search(live, objects.ids[row])) {
            plan.live.add(size);
            continue;
        } else {
            (old ? plan.old_unreferenced : plan.within_grace).add(size);
        }
        if (old)
            candidates.push_back(row);
    }

    std::ranges::sort(candidates, [&](std::uint32_t a, std::uint32_t b) {
        if (objects.mtimes[a] != objects.mtimes[b])
            return objects.mtimes[a] < objects.mtimes[b];
        return objects.names[a] < objects.names[b];
    });
    auto remaining = plan.stored.bytes;
    for (const auto row : candidates) {
        if (objects.temporary[row] == 0 && remaining <= cap)
            continue;
        plan.victim_rows.push_back(row);
        plan.victims.add(objects.sizes[row]);
        remaining -= objects.sizes[row];
    }
    return plan;
}

}
