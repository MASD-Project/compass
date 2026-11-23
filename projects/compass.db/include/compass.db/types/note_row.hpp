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
#ifndef COMPASS_DB_TYPES_NOTE_ROW_HPP
#define COMPASS_DB_TYPES_NOTE_ROW_HPP

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace compass::db::types {

/**
 * @brief The values of activity.note.state.
 */
enum class note_state : std::uint8_t { open, addressed };

/**
 * @brief Each value of note_state as activity.note stores it.
 */
[[nodiscard]] constexpr std::string_view to_string(note_state v) noexcept {
    switch (v) {
    case note_state::open: return "open";
    case note_state::addressed: return "addressed";
    }
    return "open";
}

/**
 * @brief The note_state @p text names, or none for any other text.
 */
[[nodiscard]] constexpr std::optional<note_state> note_state_of(
    std::string_view text) noexcept {
    if (text == "open")
        return note_state::open;
    if (text == "addressed")
        return note_state::addressed;
    return std::nullopt;
}

/**
 * @brief A tester's remark on a campaign, and how it was addressed.
 *
 * A row of activity.note.
 */
struct note_row {
    std::int64_t campaign = 0;
    std::optional<std::string> scenario_id;
    std::optional<std::int32_t> step_ord;
    std::string kind;
    std::string text;
    std::string author;
    note_state state = note_state::open;
    std::optional<std::string> reply;
    std::optional<std::string> addressed_by;
    std::optional<std::chrono::sys_time<std::chrono::microseconds>> addressed_at;

    bool operator==(const note_row&) const = default;
};

}

#endif
