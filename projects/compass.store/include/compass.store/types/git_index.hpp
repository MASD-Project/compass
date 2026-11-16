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
#ifndef COMPASS_STORE_TYPES_GIT_INDEX_HPP
#define COMPASS_STORE_TYPES_GIT_INDEX_HPP

#include <compare>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "compass.store/types/blob_id.hpp"

namespace compass::store::types {

/**
 * @brief A time as git's index stores it: seconds and nanoseconds, each cut
 * to 32 bits.
 */
struct index_time {
    std::uint32_t sec = 0;
    std::uint32_t nsec = 0;

    friend constexpr auto operator<=>(const index_time&,
        const index_time&) noexcept = default;
};

/**
 * @brief The stat data git records for a file, each field cut to 32 bits as
 * git cuts it. Git compares a file with its entry through these fields to
 * decide that the file is clean without reading it.
 */
struct stat_data {
    index_time ctime;
    index_time mtime;
    std::uint32_t dev = 0;
    std::uint32_t ino = 0;
    std::uint32_t uid = 0;
    std::uint32_t gid = 0;
    std::uint32_t size = 0;

    friend constexpr bool operator==(const stat_data&,
        const stat_data&) noexcept = default;
};

/**
 * @brief The entries of a git index file, as parallel columns indexed by
 * entry: one row per path and stage, in the order of the file.
 *
 * stage is 0 for a merged path and 1 to 3 for the sides of a conflict.
 * intent_to_add is 1 for a path added with `git add -N`, whose ID is the
 * empty blob rather than the file's. written is the modification time of
 * the index file, which decides whether an entry is racily clean; a parse
 * of bytes alone leaves it zero.
 */
struct git_index {
    std::uint32_t version = 0;
    index_time written;
    std::vector<std::string> paths;
    std::vector<std::uint32_t> modes;
    std::vector<stat_data> stats;
    std::vector<blob_id> ids;
    std::vector<std::uint8_t> stages;
    std::vector<std::uint8_t> intent_to_add;

    [[nodiscard]] std::size_t size() const noexcept { return paths.size(); }
};

}

#endif
