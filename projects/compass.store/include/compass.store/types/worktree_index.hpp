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
#ifndef COMPASS_STORE_TYPES_WORKTREE_INDEX_HPP
#define COMPASS_STORE_TYPES_WORKTREE_INDEX_HPP

#include <cstddef>
#include <string>
#include <vector>
#include "compass.store/types/blob_id.hpp"
#include "compass.store/types/git_index.hpp"

namespace compass::store::types {

/**
 * @brief What compass learnt by hashing files of one worktree: for each
 * path, the stat data the file had and the blob ID of its bytes, as
 * parallel columns sorted by path.
 *
 * written is the time the run that hashed the files started, taken before
 * it looked at any of them. An entry whose file was modified in that second
 * or later is racily clean and is not trusted.
 */
struct worktree_index {
    index_time written;
    std::vector<std::string> paths;
    std::vector<stat_data> stats;
    std::vector<blob_id> ids;

    [[nodiscard]] std::size_t size() const noexcept { return paths.size(); }
};

}

#endif
