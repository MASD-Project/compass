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

#ifndef COMPASS_SERVE_MESSAGES_ERROR_CODE_ENUM
/**
 * @brief The values :values: lists for error_code.
 */
enum class error_code { invalid, no_view, no_subject, forbidden, not_found, unknown_environment, unknown_sprint, conflict, too_large, internal };

/**
 * @brief Each value of error_code as the wire names it.
 */
[[nodiscard]] constexpr std::string_view to_string(error_code v) {
    switch (v) {
    case error_code::invalid: return "invalid";
    case error_code::no_view: return "no_view";
    case error_code::no_subject: return "no_subject";
    case error_code::forbidden: return "forbidden";
    case error_code::not_found: return "not_found";
    case error_code::unknown_environment: return "unknown_environment";
    case error_code::unknown_sprint: return "unknown_sprint";
    case error_code::conflict: return "conflict";
    case error_code::too_large: return "too_large";
    case error_code::internal: return "internal";
    }
    throw std::invalid_argument("Not a error_code: the value is not one of invalid, no_view, no_subject, forbidden, not_found, unknown_environment, unknown_sprint, conflict, too_large, internal");
}

/**
 * @brief The error_code @p text names, or none for any other text.
 */
[[nodiscard]] constexpr std::optional<error_code> error_code_of(
    std::string_view text) noexcept {
    if (text == "invalid")
        return error_code::invalid;
    if (text == "no_view")
        return error_code::no_view;
    if (text == "no_subject")
        return error_code::no_subject;
    if (text == "forbidden")
        return error_code::forbidden;
    if (text == "not_found")
        return error_code::not_found;
    if (text == "unknown_environment")
        return error_code::unknown_environment;
    if (text == "unknown_sprint")
        return error_code::unknown_sprint;
    if (text == "conflict")
        return error_code::conflict;
    if (text == "too_large")
        return error_code::too_large;
    if (text == "internal")
        return error_code::internal;
    return std::nullopt;
}
#endif

struct error_response {
    error_code code{};
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

#ifndef COMPASS_SERVE_MESSAGES_ERROR_CODE_ENUM
#define COMPASS_SERVE_MESSAGES_ERROR_CODE_ENUM
/**
 * @brief error_code travels as the name of its value.
 */
template <>
struct text_form<compass::serve::messages::error_code> {
    static constexpr std::string_view wire_name = "a error_code";

    static constexpr std::string_view names[] = { "invalid", "no_view", "no_subject", "forbidden", "not_found", "unknown_environment", "unknown_sprint", "conflict", "too_large", "internal" };

    static constexpr compass::serve::messages::error_code values[] = { compass::serve::messages::error_code::invalid, compass::serve::messages::error_code::no_view, compass::serve::messages::error_code::no_subject, compass::serve::messages::error_code::forbidden, compass::serve::messages::error_code::not_found, compass::serve::messages::error_code::unknown_environment, compass::serve::messages::error_code::unknown_sprint, compass::serve::messages::error_code::conflict, compass::serve::messages::error_code::too_large, compass::serve::messages::error_code::internal };

    static std::string to_text(compass::serve::messages::error_code v) {
        return std::string(to_string(v));
    }

    static compass::serve::messages::error_code from_text(std::string_view text) {
        if (const auto v = compass::serve::messages::error_code_of(text))
            return *v;
        throw std::invalid_argument("Not " + std::string(wire_name) + ": '" +
                                    std::string(text) + "' is not one of invalid, no_view, no_subject, forbidden, not_found, unknown_environment, unknown_sprint, conflict, too_large, internal");
    }
};
#endif

template <>
struct wire_traits<compass::serve::messages::error_response> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::error_response& v);

    static compass::serve::messages::error_response read_json(std::string_view text);
};

}

#endif
