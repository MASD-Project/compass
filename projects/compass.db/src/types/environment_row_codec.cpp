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
 * Template: masd_cpp_json.cpp.mustache
 * To modify, update the template and regenerate.
 */
#include "compass.db/types/environment_row_codec.hpp"

#include "compass.db/codec/struct_codec.hpp"

namespace compass::db::codec {

std::string wire_traits<compass::db::types::environment>::write_json(const compass::db::types::environment& v) {
    return object::write_json(v);
}

compass::db::types::environment wire_traits<compass::db::types::environment>::read_json(std::string_view text) {
    return object::read_json<compass::db::types::environment>(text);
}

}
