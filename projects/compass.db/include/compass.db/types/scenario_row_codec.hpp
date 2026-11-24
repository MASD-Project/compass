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
 * Template: masd_cpp_json.hpp.mustache
 * To modify, update the template and regenerate.
 */
#ifndef COMPASS_DB_TYPES_SCENARIO_ROW_CODEC_HPP
#define COMPASS_DB_TYPES_SCENARIO_ROW_CODEC_HPP

#include "compass.db/codec/text_form.hpp"
#include "compass.db/codec/wire_fields.hpp"
#include "compass.db/codec/wire_traits_fwd.hpp"
#include "compass.db/types/scenario_row.hpp"
#include "compass.db/codec/blob_id_form.hpp"
#include <string>
#include <string_view>
#include <type_traits>

namespace compass::db::types {

/**
 * @brief The members of scenario_row, named as the wire names them and in the
 * order the type declares them.
 *
 * This is the one statement of the row's shape. The carriers find it by
 * argument-dependent lookup, so no caller passes a list, and a message whose
 * member is this type encodes it as its own object without knowing the type.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<scenario_row>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"org_id", &scenario_row::org_id},
        ::compass::db::codec::field{"title", &scenario_row::title},
        ::compass::db::codec::field{"task_id", &scenario_row::task_id},
        ::compass::db::codec::field{"story_id", &scenario_row::story_id},
        ::compass::db::codec::field{"target", &scenario_row::target},
        ::compass::db::codec::field{"environment", &scenario_row::environment},
        ::compass::db::codec::field{"definition", &scenario_row::definition},
    };
}

}

namespace compass::db::codec {

/**
 * @brief The generated codec: what the wire codec resolves to for this type.
 *
 * The functions are declared here and defined in this type's own translation
 * unit, so a translation unit that only sends the type compiles a declaration
 * rather than a second copy of the encoder.
 */
template <>
struct wire_traits<compass::db::types::scenario_row> {
    /**
     * @brief States that this is a generated codec.
     *
     * A test asserts this rather than the encoding, because the encoding is
     * what the type's own case already round-trips: what it cannot see is
     * whether the seam resolved to this specialisation or fell through to the
     * primary template, and a marker that only this specialisation declares
     * is that answer at compile time.
     */
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::db::types::scenario_row& v);

    static compass::db::types::scenario_row read_json(std::string_view text);
};

}

#endif
