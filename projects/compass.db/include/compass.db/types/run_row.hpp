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
/**
 * AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Template: masd_cpp_row.hpp.mustache
 * To modify, update the template and regenerate.
 */
#ifndef COMPASS_DB_TYPES_RUN_ROW_HPP
#define COMPASS_DB_TYPES_RUN_ROW_HPP

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include "compass.store/types/blob_id.hpp"

namespace compass::db::types {

/**
 * @brief The values of activity.run.outcome.
 */
enum class run_outcome : std::uint8_t { passed, failed, blocked };

/**
 * @brief Each value of run_outcome as activity.run stores it.
 */
[[nodiscard]] constexpr std::string_view to_string(run_outcome v) noexcept {
    switch (v) {
    case run_outcome::passed: return "passed";
    case run_outcome::failed: return "failed";
    case run_outcome::blocked: return "blocked";
    }
    return "passed";
}

/**
 * @brief The run_outcome @p text names, or none for any other text.
 */
[[nodiscard]] constexpr std::optional<run_outcome> run_outcome_of(
    std::string_view text) noexcept {
    if (text == "passed")
        return run_outcome::passed;
    if (text == "failed")
        return run_outcome::failed;
    if (text == "blocked")
        return run_outcome::blocked;
    return std::nullopt;
}

/**
 * @brief One run of a test scenario in a campaign.
 *
 * A row of activity.run.
 */
struct run_row {
    std::int64_t campaign = 0;
    std::string scenario_id;
    store::types::blob_id definition;
    store::types::blob_id scenario_hash;
    std::string tester;
    std::optional<std::chrono::sys_time<std::chrono::microseconds>> finished_at;
    std::optional<run_outcome> outcome;

    bool operator==(const run_row&) const = default;
};

}

#endif
