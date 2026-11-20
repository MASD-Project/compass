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
#ifndef COMPASS_CODEC_TYPES_ACTION_MANIFEST_HPP
#define COMPASS_CODEC_TYPES_ACTION_MANIFEST_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include "compass.codec/types/dependency_record.hpp"
#include "compass.codec/types/text_column.hpp"
#include "compass.org/types/column.hpp"
#include "compass.org/types/handle.hpp"
#include "compass.store/types/action_kind.hpp"
#include "compass.store/types/blob_id.hpp"

namespace compass::codec::types {

using input_set_id = compass::org::types::handle<struct input_set_tag>;
using chunk_ref_id = compass::org::types::handle<struct chunk_ref_tag>;

/**
 * @brief Inputs of a unit, each its kind, its key, whether it was found and
 * its value: the inputs of one chunk, or of one input set once its chunks
 * are read.
 */
struct input_list {
    compass::org::types::column<input_id, input_kind> input_kinds;
    text_column<input_id> input_keys;
    compass::org::types::column<input_id, std::uint8_t> input_found;
    compass::org::types::column<input_id, compass::store::types::blob_id>
        input_values;

    [[nodiscard]] std::size_t input_count() const noexcept {
        return input_kinds.size();
    }
};

/**
 * @brief The input sets seen for one manifest key, newest first, each with
 * the key of the result it produced and the keys of the chunks that hold
 * its inputs.
 * The chunks of set s are the rows from chunk_firsts[s] up to chunk_firsts
 * of the next set, or the end. A chunk is a result object keyed by its
 * bytes, so sets that differ in a few inputs share all other chunks, and
 * a set that changes writes only the chunks that hold its changes.
 */
struct action_manifest {
    compass::org::types::column<input_set_id,
        compass::store::types::blob_id> results;
    compass::org::types::column<input_set_id, chunk_ref_id> chunk_firsts;
    compass::org::types::column<chunk_ref_id,
        compass::store::types::blob_id> chunks;

    [[nodiscard]] std::size_t size() const noexcept {
        return results.size();
    }

    [[nodiscard]] std::size_t chunk_count() const noexcept {
        return chunks.size();
    }
};

/**
 * @brief Reads the body of the action object of one kind and key, or
 * nothing when there is none or it is damaged. Called from several threads
 * at once. An empty source caches nothing.
 */
using action_source = std::function<std::optional<std::string>(
    compass::store::types::action_kind,
    const compass::store::types::blob_id&)>;

}

#endif
