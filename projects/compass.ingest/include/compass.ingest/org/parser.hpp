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
#ifndef COMPASS_INGEST_ORG_PARSER_HPP
#define COMPASS_INGEST_ORG_PARSER_HPP

#include <cstdint>
#include <string_view>
#include "compass.org/types/tables.hpp"

namespace compass::ingest::org {

/**
 * @brief The version of what parse puts in the tables.
 *
 * Bumped by hand whenever some source would now give tables that differ
 * from the ones the previous version gave it: a fixed position, a new
 * construct, a changed name. A store keeps the tables of a source under
 * this number, so tables an older parser wrote are parsed again and never
 * read as current. A change that leaves every table as it was, such as a
 * faster scan, keeps the number and the store.
 */
inline constexpr std::uint32_t parser_version = 1;

/**
 * @brief Parses one org-mode document into the compass.org tables.
 *
 * The tables are cleared first and keep their capacity, so one tables value
 * can be reused for a run of documents. Elements, objects and positions
 * follow org-element as Org 9.7 ships it: where the Org Syntax
 * specification and org-element disagree, org-element wins. Every range in
 * the tables is a byte range into @p source, which the caller keeps alive.
 *
 * The TODO keywords of the document come from its #+todo:, #+seq_todo: and
 * #+typ_todo: keywords, or are TODO and DONE when it has none. A link type
 * is one of the types org registers out of the box; a bracket link written
 * as scheme:target with any other scheme keeps that scheme as its type.
 *
 * Citations and radio links are not recognised.
 *
 * @throws std::length_error when the source is 4 GiB or larger.
 */
void parse(std::string_view source, compass::org::types::tables& out);

}

#endif
