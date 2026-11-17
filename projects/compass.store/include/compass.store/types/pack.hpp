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
#ifndef COMPASS_STORE_TYPES_PACK_HPP
#define COMPASS_STORE_TYPES_PACK_HPP

#include <cstdint>
#include "compass.store/types/action_kind.hpp"
#include "compass.store/types/blob_id.hpp"

namespace compass::store::types {

/**
 * @brief What an entry of a pack holds: a parsed object, keyed by the blob
 * ID of its source, or one of the two kinds of action object, keyed by the
 * key its writer computed.
 */
enum class entry_kind : std::uint8_t { object, manifest, result };

[[nodiscard]] constexpr entry_kind entry_kind_of(action_kind k) noexcept {
    return k == action_kind::manifest ? entry_kind::manifest
                                      : entry_kind::result;
}

/**
 * @brief One row of a pack's index: the kind and key of an entry, where
 * its bytes start in the pack file and how many there are, and when it was
 * first written, in nanoseconds since the epoch.
 *
 * A merge or a collection that copies an entry keeps its written time, so
 * collection reads the age of the entry and not of the pack, and of two
 * entries with one kind and key the later written is the one read.
 */
struct pack_entry {
    blob_id key;
    entry_kind kind = entry_kind::object;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    std::int64_t written = 0;
};

}

#endif
