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
 * Template: masd_cpp_http_service_caller.hpp.mustache
 * To modify, update the template and regenerate.
 */
#ifndef COMPASS_SERVE_DOMAIN_CALLER_HPP
#define COMPASS_SERVE_DOMAIN_CALLER_HPP

#include <optional>
#include <string>

namespace compass::serve::domain {

/**
 * @brief Who calls, as the gateway names them: the view they read through,
 * the person and, when an agent acts for the person, the agent.
 *
 * The server builds it from the request's headers, never from its body.
 * subject is empty on a read whose gateway named no one.
 */
struct caller final {
    std::string view;
    std::string subject;
    std::optional<std::string> actor;
};

}

#endif
