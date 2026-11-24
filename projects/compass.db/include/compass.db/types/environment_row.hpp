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
#ifndef COMPASS_DB_TYPES_ENVIRONMENT_ROW_HPP
#define COMPASS_DB_TYPES_ENVIRONMENT_ROW_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace compass::db::types {

/**
 * @brief The values of corpus.environment.kind.
 */
enum class environment_kind : std::uint8_t { worktree, branch, release };

/**
 * @brief Each value of environment_kind as corpus.environment stores it.
 */
[[nodiscard]] constexpr std::string_view to_string(environment_kind v) noexcept {
    switch (v) {
    case environment_kind::worktree: return "worktree";
    case environment_kind::branch: return "branch";
    case environment_kind::release: return "release";
    }
    return "worktree";
}

/**
 * @brief The environment_kind @p text names, or none for any other text.
 */
[[nodiscard]] constexpr std::optional<environment_kind> environment_kind_of(
    std::string_view text) noexcept {
    if (text == "worktree")
        return environment_kind::worktree;
    if (text == "branch")
        return environment_kind::branch;
    if (text == "release")
        return environment_kind::release;
    return std::nullopt;
}

/**
 * @brief A worktree, branch or release of the repository.
 *
 * A row of corpus.environment.
 */
struct environment {
    std::string name;
    environment_kind kind = environment_kind::worktree;
    std::optional<std::string> root;
    std::optional<std::string> head;

    bool operator==(const environment&) const = default;
};

}

#endif
