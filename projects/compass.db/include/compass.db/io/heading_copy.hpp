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
/**
 * AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Template: masd_cpp_copy.hpp.mustache
 * To modify, update the template and regenerate.
 */
#ifndef COMPASS_DB_IO_HEADING_COPY_HPP
#define COMPASS_DB_IO_HEADING_COPY_HPP

#include <cstddef>
#include <string_view>
#include "compass.db/ops/copy_text.hpp"
#include "compass.db/types/heading_row.hpp"
#include "compass.store/types/blob_id.hpp"

namespace compass::db::io {

/**
 * @brief The columns of corpus.heading that write_copy writes,
 * in its order, for the column list of a COPY ... FROM STDIN.
 */
inline constexpr std::string_view heading_copy_columns =
    "hash, ord, level, title, path, anchor, org_id, todo, priority, "
    "tags, properties";

/**
 * @brief Writes a row of corpus.heading to @p w, in the order of
 * heading_copy_columns.
 */
void write_copy(ops::copy_writer& w,
    const store::types::blob_id& hash,
    std::size_t ord,
    const types::heading_row& r);

}

#endif
