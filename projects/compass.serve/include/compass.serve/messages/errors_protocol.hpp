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
 * Template: masd_cpp_protocol.hpp.mustache
 * To modify, update the template and regenerate.
 */
#ifndef COMPASS_SERVE_MESSAGES_ERRORS_PROTOCOL_HPP
#define COMPASS_SERVE_MESSAGES_ERRORS_PROTOCOL_HPP

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include "compass.db/codec/text_form.hpp"
#include "compass.db/codec/wire_fields.hpp"
#include "compass.db/codec/wire_traits_fwd.hpp"

namespace compass::serve::messages {

struct error_response {
    std::string code;
    std::string message;

    friend bool operator==(const error_response&, const error_response&) = default;
};

/**
 * @brief The members of error_response, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<error_response>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"code", &error_response::code},
        ::compass::db::codec::field{"message", &error_response::message},
    };
}

}

namespace compass::db::codec {

template <>
struct wire_traits<compass::serve::messages::error_response> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::error_response& v);

    static compass::serve::messages::error_response read_json(std::string_view text);
};

}

#endif
