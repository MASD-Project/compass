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
#ifndef COMPASS_ORG_TYPES_DOCUMENT_TABLE_HPP
#define COMPASS_ORG_TYPES_DOCUMENT_TABLE_HPP

#include <cstddef>
#include <cstdint>
#include "compass.org/types/column.hpp"
#include "compass.org/types/handle.hpp"

namespace compass::org::types {

/**
 * @brief Where each document starts in the tables of a corpus.
 *
 * A corpus holds the rows of its documents end to end, in document order.
 * The rows of document d in a table run from that table's column here at d
 * up to the same column at the next document, or up to the end of the table
 * for the last document. Handles stay local: a handle indexes the slice of
 * its document, never the corpus column.
 */
struct document_table {
    column<document_id, std::uint64_t> source_firsts;
    column<document_id, std::uint32_t> name_byte_firsts;
    column<document_id, std::uint32_t> name_firsts;
    column<document_id, std::uint32_t> element_firsts;
    column<document_id, std::uint32_t> object_firsts;
    column<document_id, std::uint32_t> heading_firsts;
    column<document_id, std::uint32_t> heading_tag_firsts;
    column<document_id, std::uint32_t> keyword_firsts;
    column<document_id, std::uint32_t> property_firsts;
    column<document_id, std::uint32_t> affiliated_firsts;
    column<document_id, std::uint32_t> block_firsts;
    column<document_id, std::uint32_t> list_firsts;
    column<document_id, std::uint32_t> item_firsts;
    column<document_id, std::uint32_t> link_firsts;

    [[nodiscard]] std::size_t size() const noexcept {
        return source_firsts.size();
    }
};

inline constexpr std::size_t document_row_bytes = row_bytes(
    &document_table::source_firsts, &document_table::name_byte_firsts,
    &document_table::name_firsts, &document_table::element_firsts,
    &document_table::object_firsts, &document_table::heading_firsts,
    &document_table::heading_tag_firsts, &document_table::keyword_firsts,
    &document_table::property_firsts, &document_table::affiliated_firsts,
    &document_table::block_firsts, &document_table::list_firsts,
    &document_table::item_firsts, &document_table::link_firsts);

static_assert(document_row_bytes == 60);

}

#endif
