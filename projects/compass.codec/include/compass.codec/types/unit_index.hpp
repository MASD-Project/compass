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
#ifndef COMPASS_CODEC_TYPES_UNIT_INDEX_HPP
#define COMPASS_CODEC_TYPES_UNIT_INDEX_HPP

#include <cstddef>
#include <cstdint>
#include <vector>
#include "compass.codec/types/dependency_record.hpp"
#include "compass.codec/types/text_column.hpp"
#include "compass.org/types/column.hpp"
#include "compass.org/types/handle.hpp"
#include "compass.store/types/blob_id.hpp"

namespace compass::codec::types {

using subject_id = compass::org::types::handle<struct subject_tag>;

/**
 * @brief What links a unit of the dependency record to a key, so that an
 * edit to one file finds the units it may change without reading the
 * others.
 * reads: a path, file or file_exists input of the unit has the path the
 * key holds. A document unit reads its own document so, and a lookup by ID
 * that found a document reads it so too. id: an id input looked up the ID
 * the key holds. listing: a listing input has the pattern the key holds.
 * type_set and type_registry: the unit has such an input; the key is
 * empty. output: the unit produced a file at the path the key holds.
 * finding: a finding of the unit names the path the key holds. fact: the
 * unit has a fact input; the key is its codec. stage: the unit has an
 * empty document and its codec is one whose units have fact inputs, so it
 * is the prepare step that published them; the key is its codec.
 */
enum class subject_kind : std::uint8_t {
    reads, id, listing, type_set, type_registry, output, finding, fact,
    stage
};

inline constexpr subject_kind last_subject_kind = subject_kind::stage;

/**
 * @brief Where each unit of a dependency record file is, and which units
 * each subject names, so one unit can be read and checked alone.
 * The head of unit u runs from offsets[u] up to offsets[u + 1], and
 * hashes[u] is the SHA-1 of its bytes; the contents of its files run from
 * content_offsets[u] up to content_offsets[u + 1]. Both offset lists have
 * one more row than there are units, the end of the last. Subjects are
 * sorted by kind, then by key in byte order, each once; the units of
 * subject s are units[unit_firsts[s]] up to unit_firsts of the next
 * subject, or the end, in ascending order.
 */
struct unit_index {
    std::vector<std::uint64_t> offsets;
    compass::org::types::column<record_unit_id,
        compass::store::types::blob_id> hashes;
    std::vector<std::uint64_t> content_offsets;
    compass::org::types::column<subject_id, subject_kind> kinds;
    text_column<subject_id> keys;
    compass::org::types::column<subject_id, std::uint32_t> unit_firsts;
    std::vector<record_unit_id> units;

    [[nodiscard]] std::size_t size() const noexcept { return hashes.size(); }

    [[nodiscard]] std::size_t subject_count() const noexcept {
        return kinds.size();
    }
};

}

#endif
