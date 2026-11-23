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
 * Template: masd_cpp_http_service_http_response.hpp.mustache
 * To modify, update the template and regenerate.
 */
#ifndef COMPASS_SERVE_DOMAIN_HTTP_RESPONSE_HPP
#define COMPASS_SERVE_DOMAIN_HTTP_RESPONSE_HPP

#include "compass.serve/domain/awaitable_body_source.hpp"
#include "compass.serve/domain/body_source.hpp"
#include "compass.serve/domain/http_status.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace compass::serve::domain {

/**
 * @brief Represents an HTTP response to be sent to the client.
 */
struct http_response final {
    /**
     * @brief HTTP status code.
     */
    http_status status = http_status::ok;

    /**
     * @brief Response headers.
     */
    std::unordered_map<std::string, std::string> headers;

    /**
     * @brief Response body.
     *
     * Empty on a response that names a =body_stream=, because that body is read as
     * it is written.
     */
    std::string body;

    /**
     * @brief The body read as it is written, when the response has one.
     *
     * A shared pointer because the response outlives the handler that made it, and
     * because a source may be shared with whatever produced it.
     */
    std::shared_ptr<body_source> body_stream;

    /**
     * @brief The body read as it is written by a source that may have to wait.
     *
     * A response names one of this or =body_stream=, never both: the source that
     * waits is the one whose next piece is not ready, and the session takes the
     * loop that suits it.
     */
    std::shared_ptr<awaitable_body_source> body_awaitable;

    /**
     * @brief Content type header value.
     */
    std::string content_type = "application/json";

    /**
     * @brief Creates a successful JSON response.
     */
    static http_response json(const std::string& body, http_status status = http_status::ok);

    /**
     * @brief Creates an error response.
     */
    static http_response error(http_status status, const std::string& message);

    /**
     * @brief Creates an error response whose body names a code and a message.
     *
     * The browser reads a refusal as a code it can act on and a sentence it can
     * show, so a route the browser calls answers in this shape.
     */
    static http_response
    api_error(http_status status, const std::string& code, const std::string& message);

    /**
     * @brief Creates a 404 Not Found response.
     */
    static http_response not_found(const std::string& message = "Not Found");

    /**
     * @brief Refuses a range that starts past the end of what the body holds.
     */
    static http_response range_not_satisfiable(std::uint64_t total);

    /**
     * @brief Creates a 401 Unauthorized response.
     */
    static http_response unauthorized(const std::string& message = "Unauthorized");

    /**
     * @brief Creates a 403 Forbidden response.
     */
    static http_response forbidden(const std::string& message = "Forbidden");

    /**
     * @brief Creates a 400 Bad Request response.
     */
    static http_response bad_request(const std::string& message);

    /**
     * @brief Creates a 500 Internal Server Error response.
     */
    static http_response internal_error(const std::string& message);

    /**
     * @brief Sets a response header.
     */
    http_response& set_header(const std::string& name, const std::string& value);
};

}

#endif
