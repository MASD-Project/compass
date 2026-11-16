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
#ifndef COMPASS_STORE_OPS_COLLECTION_HPP
#define COMPASS_STORE_OPS_COLLECTION_HPP

#include <cstdint>
#include <span>
#include "compass.store/types/blob_id.hpp"
#include "compass.store/types/collection.hpp"

namespace compass::store::ops {

/**
 * @brief Chooses the files of @p objects a collection removes.
 *
 * An object whose ID is in @p live is never chosen. Of the rest, only files
 * modified before @p cutoff, in nanoseconds since the epoch, are candidates.
 * Candidates are taken oldest first, ties broken by name. A temporary file
 * is always taken, since nothing reads it; an object is taken while the
 * store still holds more than @p cap bytes.
 *
 * @pre @p live is sorted and holds no ID twice.
 */
[[nodiscard]] types::collection_plan plan_collection(
    const types::stored_objects& objects, std::span<const types::blob_id> live,
    std::uint64_t cap, std::int64_t cutoff);

}

#endif
