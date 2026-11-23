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
 * Template: masd_cpp_http_service_refusal.hpp.mustache
 * To modify, update the template and regenerate.
 */
#ifndef COMPASS_SERVE_DOMAIN_REFUSAL_HPP
#define COMPASS_SERVE_DOMAIN_REFUSAL_HPP

#include "compass.serve/domain/http_status.hpp"
#include <stdexcept>
#include <string>
#include <utility>

namespace compass::serve::domain {

/**
 * @brief A call the service refuses: the status it answers, a code from the
 * service's closed list a client can act on, and a sentence it can show.
 *
 * A route answers it as the body api_error writes, so nothing about the
 * refusal depends on which part of the call refused it.
 */
class refusal : public std::runtime_error {
public:
    refusal(http_status status, std::string code, const std::string& message)
        : std::runtime_error(message), status_(status), code_(std::move(code)) {}

    [[nodiscard]] http_status status() const noexcept {
        return status_;
    }

    [[nodiscard]] const std::string& code() const noexcept {
        return code_;
    }

    /// The request's input is wrong: a value, a parameter or a member.
    [[nodiscard]] static refusal invalid(const std::string& message) {
        return {http_status::bad_request, "invalid", message};
    }

    /// The thing the request names is not in the environment.
    [[nodiscard]] static refusal not_found(const std::string& message) {
        return {http_status::not_found, "not_found", message};
    }

private:
    http_status status_;
    std::string code_;
};

}

#endif
