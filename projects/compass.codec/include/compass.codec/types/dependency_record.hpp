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
#ifndef COMPASS_CODEC_TYPES_DEPENDENCY_RECORD_HPP
#define COMPASS_CODEC_TYPES_DEPENDENCY_RECORD_HPP

#include <cstddef>
#include <cstdint>
#include "compass.codec/types/file_table.hpp"
#include "compass.codec/types/text_column.hpp"
#include "compass.org/types/column.hpp"
#include "compass.org/types/handle.hpp"
#include "compass.store/types/blob_id.hpp"

namespace compass::codec::types {

using record_unit_id = compass::org::types::handle<struct record_unit_tag>;
using input_id = compass::org::types::handle<struct input_tag>;

/**
 * @brief What one recorded input asked, and so what its value is.
 * path: the document at the path the key holds; the value is its content
 * ID. id: the document whose ID property the key holds; the value is the
 * blob ID of its path, so the answer changes when another document takes
 * the ID. type_set: every document the codec may read; the value is the
 * SHA-1 of their paths and content IDs in path order. type_registry: the
 * type registry; the value is the SHA-1 of its tables. file: the file of
 * the checkout at the path the key holds, document or not; the value is
 * its content ID. file_exists: whether the checkout has a file at the path
 * the key holds; the value is all zeros. listing: the paths of the files of
 * the checkout that match the pattern the key holds, a prefix and a suffix
 * joined by '*'; the value is the SHA-1 of those paths in byte order.
 * fact: the fact the key names among those the prepare step of the unit's
 * staged codec published; the value is the hash it published for it.
 * Only documents of the codec's declared types answer a path or ID lookup,
 * so a document of another type is a miss. File inputs see every file of
 * the checkout.
 */
enum class input_kind : std::uint8_t {
    path, id, type_set, type_registry, file, file_exists, listing, fact
};

/**
 * @brief The last input kind, which bounds what a record or a manifest may
 * hold.
 */
inline constexpr input_kind last_input_kind = input_kind::fact;

/**
 * @brief What the codecs of a run read and produced, one row per unit: one
 * call of a document codec, for one document, or the call of a corpus
 * codec.
 * A unit is named by its codec and, for a document codec, the path of its
 * document; documents is empty for a corpus codec and for the prepare step
 * of a staged codec, and holds the part's key for a part of one. versions and
 * configurations hold the codec's version and the SHA-1 of its code hash
 * and option values when the unit ran. The inputs of unit u are the rows from
 * input_firsts[u] up to input_firsts of the next unit, or the end; found is
 * 0 for a lookup no document answered, whose value is all zeros. The files
 * of u are the rows of files from file_firsts[u] in the same way, with the
 * blob ID of each content in file_hashes. diagnostics holds the findings of
 * u encoded with no corpus row, each naming its document by path, and
 * diagnostic_hashes the SHA-1 of those bytes. manifests and results hold
 * the keys of the unit's action objects in the result cache, which keep
 * them through collection while this record names them.
 */
struct dependency_record {
    text_column<record_unit_id> codecs;
    compass::org::types::column<record_unit_id, std::uint32_t> versions;
    compass::org::types::column<record_unit_id,
        compass::store::types::blob_id> configurations;
    text_column<record_unit_id> documents;
    compass::org::types::column<record_unit_id, input_id> input_firsts;
    compass::org::types::column<record_unit_id, file_id> file_firsts;
    compass::org::types::column<record_unit_id, std::uint32_t>
        diagnostic_counts;
    compass::org::types::column<record_unit_id,
        compass::store::types::blob_id> diagnostic_hashes;
    text_column<record_unit_id, std::uint64_t> diagnostics;
    compass::org::types::column<record_unit_id,
        compass::store::types::blob_id> manifests;
    compass::org::types::column<record_unit_id,
        compass::store::types::blob_id> results;

    compass::org::types::column<input_id, input_kind> input_kinds;
    text_column<input_id> input_keys;
    compass::org::types::column<input_id, std::uint8_t> input_found;
    compass::org::types::column<input_id, compass::store::types::blob_id>
        input_values;

    file_table files;
    compass::org::types::column<file_id, compass::store::types::blob_id>
        file_hashes;

    [[nodiscard]] std::size_t size() const noexcept { return codecs.size(); }

    [[nodiscard]] std::size_t input_count() const noexcept {
        return input_kinds.size();
    }
};

}

#endif
