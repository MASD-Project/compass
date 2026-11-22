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
#ifndef COMPASS_DB_TYPES_ENVIRONMENT_HPP
#define COMPASS_DB_TYPES_ENVIRONMENT_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include "compass.store/types/blob_id.hpp"

namespace compass::db::types {

/**
 * @brief What an environment is a copy of: a worktree's files, a branch,
 * or a release synced from a commit at deploy.
 */
enum class environment_kind : std::uint8_t { worktree, branch, release };

[[nodiscard]] constexpr std::string_view to_string(
    environment_kind k) noexcept {
    switch (k) {
    case environment_kind::worktree: return "worktree";
    case environment_kind::branch: return "branch";
    case environment_kind::release: return "release";
    }
    return "worktree";
}

/**
 * @brief The environment a sync writes. root is the checkout of a
 * worktree, empty for a release; head is the commit its files are at, when
 * the sync knows it.
 */
struct environment {
    std::string name;
    environment_kind kind = environment_kind::worktree;
    std::string root;
    std::optional<std::string> head;
};

/**
 * @brief A path of an environment and the blob ID of what it holds.
 */
struct placement {
    std::string path;
    store::types::blob_id hash;
};

/**
 * @brief An environment as the database holds it, with how many paths it
 * places; synced_at is the server's text for the time, empty when never
 * synced.
 */
struct environment_summary {
    std::string name;
    std::string kind;
    std::string root;
    std::string head;
    std::string synced_at;
    std::uint64_t documents = 0;
};

}

#endif
