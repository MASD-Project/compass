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
#ifndef COMPASS_DB_TYPES_STEP_RESULT_ROW_HPP
#define COMPASS_DB_TYPES_STEP_RESULT_ROW_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace compass::db::types {

/**
 * @brief The values of activity.step_result.outcome.
 */
enum class step_outcome { pass, fail, blocked, skipped };

/**
 * @brief Each value of step_outcome as activity.step_result stores it.
 */
[[nodiscard]] constexpr std::string_view to_string(step_outcome v) noexcept {
    switch (v) {
    case step_outcome::pass: return "pass";
    case step_outcome::fail: return "fail";
    case step_outcome::blocked: return "blocked";
    case step_outcome::skipped: return "skipped";
    }
    return "pass";
}

/**
 * @brief The step_outcome @p text names, or none for any other text.
 */
[[nodiscard]] constexpr std::optional<step_outcome> step_outcome_of(
    std::string_view text) noexcept {
    if (text == "pass")
        return step_outcome::pass;
    if (text == "fail")
        return step_outcome::fail;
    if (text == "blocked")
        return step_outcome::blocked;
    if (text == "skipped")
        return step_outcome::skipped;
    return std::nullopt;
}

/**
 * @brief The outcome of one step of a run.
 *
 * A row of activity.step_result.
 */
struct step_result_row {
    std::int64_t run = 0;
    std::int32_t step_ord = 0;
    step_outcome outcome = step_outcome::pass;
    std::optional<std::string> notes;

    bool operator==(const step_result_row&) const = default;
};

}

#endif
