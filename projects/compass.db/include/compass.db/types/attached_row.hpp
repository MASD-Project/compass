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
 * Template: masd_cpp_row.hpp.mustache
 * To modify, update the template and regenerate.
 */
#ifndef COMPASS_DB_TYPES_ATTACHED_ROW_HPP
#define COMPASS_DB_TYPES_ATTACHED_ROW_HPP

#include <cstdint>
#include <optional>
#include <string>
#include "compass.store/types/blob_id.hpp"

namespace compass::db::types {

/**
 * @brief An attachment attached to a run or a note.
 *
 * A row of activity.attached.
 */
struct attached_row {
    store::types::blob_id attachment;
    std::optional<std::int64_t> run;
    std::optional<std::int64_t> note;
    std::optional<std::int32_t> step_ord;
    std::optional<std::string> caption;
};

}

#endif
