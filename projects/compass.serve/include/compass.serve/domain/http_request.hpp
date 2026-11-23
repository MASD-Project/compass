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
 * Template: masd_cpp_http_service_http_request.hpp.mustache
 * To modify, update the template and regenerate.
 */
#ifndef COMPASS_SERVE_DOMAIN_HTTP_REQUEST_HPP
#define COMPASS_SERVE_DOMAIN_HTTP_REQUEST_HPP

#include "compass.serve/domain/body_sink.hpp"
#include "compass.serve/domain/http_method.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace compass::serve::domain {

/**
 * @brief Represents an incoming HTTP request.
 */
struct http_request final {
    /**
     * @brief HTTP method (GET, POST, etc.)
     */
    http_method method = http_method::get;

    /**
     * @brief Request target/path.
     */
    std::string target;

    /**
     * @brief Request headers.
     */
    std::unordered_map<std::string, std::string> headers;

    /**
     * @brief Query parameters parsed from URL.
     */
    std::unordered_map<std::string, std::string> query_params;

    /**
     * @brief Path parameters extracted from route pattern.
     */
    std::unordered_map<std::string, std::string> path_params;

    /**
     * @brief Request body.
     *
     * Empty on a route that streamed its body, because the bytes went to the
     * sink instead.
     */
    std::string body;

    /**
     * @brief The number of bytes the body carried, buffered or streamed.
     */
    std::uint64_t body_bytes = 0;

    /**
     * @brief The sink the body was streamed into, when the route asked for one.
     *
     * Null on a buffered route. The session owns the sink and it outlives the
     * handler call, so the handler may commit or abort it.
     */
    body_sink* sink = nullptr;

    /**
     * @brief Remote endpoint address.
     */
    std::string remote_address;

    /**
     * @brief HTTP version major number (e.g., 1 for HTTP/1.1).
     */
    std::uint16_t http_version_major = 1;

    /**
     * @brief HTTP version minor number (e.g., 1 for HTTP/1.1).
     */
    std::uint16_t http_version_minor = 1;

    /**
     * @brief Returns header value or empty string if not found.
     */
    std::string get_header(const std::string& name) const;

    /**
     * @brief Returns query parameter value or empty string if not found.
     */
    std::string get_query_param(const std::string& name) const;

    /**
     * @brief Returns path parameter value or empty string if not found.
     */
    std::string get_path_param(const std::string& name) const;
};

}

#endif
