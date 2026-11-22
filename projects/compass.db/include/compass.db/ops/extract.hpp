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
#ifndef COMPASS_DB_OPS_EXTRACT_HPP
#define COMPASS_DB_OPS_EXTRACT_HPP

#include <string>
#include <string_view>
#include "compass.db/types/corpus_document.hpp"
#include "compass.org/types/tables.hpp"
#include "compass.store/types/blob_id.hpp"

namespace compass::db::ops {

/**
 * @brief The corpus rows of the document @p source, whose blob ID is
 * @p hash and whose parse is @p t: compass's parse as the store holds it,
 * so the sync never parses a document twice. The rows point into
 * @p source, which the caller keeps alive.
 */
[[nodiscard]] types::corpus_document extract(
    const store::types::blob_id& hash, std::string_view source,
    const org::types::tables_view& t);

/**
 * @brief @p text as an anchor: lower-case ASCII letters and digits, each
 * run of anything else one hyphen, with none at either end; "section"
 * when nothing is left.
 */
[[nodiscard]] std::string slug(std::string_view text);

}

#endif
