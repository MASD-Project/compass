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
#include "compass.db/ops/run_outcome.hpp"

#include <algorithm>

namespace compass::db::ops {

std::expected<types::run_outcome, std::vector<std::int32_t>>
derive_run_outcome(std::span<const std::optional<types::step_outcome>> steps) {
    using types::step_outcome;
    const auto any = [&](step_outcome o) {
        return std::ranges::contains(steps, std::optional(o));
    };
    if (any(step_outcome::fail))
        return types::run_outcome::failed;
    if (any(step_outcome::blocked))
        return types::run_outcome::blocked;
    std::vector<std::int32_t> unrecorded;
    for (std::size_t ord = 0; ord < steps.size(); ++ord)
        if (!steps[ord])
            unrecorded.push_back(static_cast<std::int32_t>(ord));
    if (!unrecorded.empty())
        return std::unexpected(std::move(unrecorded));
    return types::run_outcome::passed;
}

}
