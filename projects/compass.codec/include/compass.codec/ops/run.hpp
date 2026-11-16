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
#ifndef COMPASS_CODEC_OPS_RUN_HPP
#define COMPASS_CODEC_OPS_RUN_HPP

#include <string_view>
#include "compass.codec/types/corpus.hpp"
#include "compass.codec/types/dependency_record.hpp"
#include "compass.codec/types/registry.hpp"
#include "compass.codec/types/run_result.hpp"
#include "compass.org/types/tables.hpp"
#include "compass.org/types/type_registry.hpp"

namespace compass::codec::ops {

/**
 * @brief The registered type of a parsed document: the type its first
 * #+type: keyword names, or none.
 */
[[nodiscard]] compass::org::types::type_id document_type(
    std::string_view source, const compass::org::types::tables_view& tables,
    const compass::org::types::type_registry& types) noexcept;

/**
 * @brief Runs every enabled codec of @p r over @p c on up to @p threads
 * threads.
 *
 * Each codec reads through a view of the documents whose type it declared.
 * No codec reads what another wrote, so all of them run at once: a corpus
 * codec is one piece of work and a document codec is split into runs of
 * documents. Every piece writes to a table of its own, and the tables of a
 * codec are joined in document order, so the result does not depend on the
 * thread count. Nothing is written anywhere else.
 *
 * A unit of @p previous, the record of an earlier run, stands in for the
 * call when its codec's version and configuration hash are unchanged and
 * every input it recorded has the same answer in @p c: a path or ID lookup
 * finds the same document, or still finds none; the set of documents of a
 * corpus codec has the same paths and content IDs; the type registry has
 * the same tables. Its recorded outputs are then put in the result where
 * the call's would be, so the result is the one a run with no record
 * gives. Every other unit is called, through a view that records what it
 * reads.
 *
 * @pre @p r was built against @p types, and the types of @p c are rows of
 * @p types or none.
 * @throws whatever a codec throws; the first exception is rethrown once
 * every thread has stopped.
 */
[[nodiscard]] types::run_result run(const types::registry& r,
    const types::corpus& c, const compass::org::types::type_registry& types,
    unsigned threads, const types::dependency_record& previous = {});

}

#endif
