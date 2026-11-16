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
#include "compass.cpp/ops/report.hpp"

namespace compass::cpp::ops {

void report(compass::codec::types::diagnostic_table& out,
    std::string_view file, std::uint32_t offset, std::uint32_t rule,
    std::string_view message, std::string_view fix) {
    namespace ct = compass::codec::types;
    out.add({.document = compass::org::types::document_id::none(),
        .range = {offset, offset},
        .rule = ct::rule_id{rule},
        .level = ct::severity::error,
        .message = message,
        .fix = fix,
        .file = file});
}

}
