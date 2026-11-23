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
 * Template: masd_cpp_http_service_endpoint_registry.hpp.mustache
 * To modify, update the template and regenerate.
 */
#ifndef COMPASS_SERVE_OPENAPI_ENDPOINT_REGISTRY_HPP
#define COMPASS_SERVE_OPENAPI_ENDPOINT_REGISTRY_HPP

#include "compass.serve/domain/route.hpp"
#include <string>
#include <vector>

namespace compass::serve::openapi {

/**
 * @brief API information for OpenAPI spec.
 */
struct api_info final {
    std::string title = "compass serve";
    std::string description = "compass's document service";
    std::string version = "1.0.0";
    std::string contact_name;
    std::string contact_email;
    std::string license_name = "GPL-3.0";
    std::string license_url;
};

/**
 * @brief Server information for OpenAPI spec.
 */
struct server_info final {
    std::string url;
    std::string description;
};

/**
 * @brief Registry for API endpoints that generates OpenAPI specification.
 */
class endpoint_registry final {
public:
    endpoint_registry();

    /**
     * @brief Sets the API information.
     */
    void set_info(const api_info& info);

    /**
     * @brief Adds a server to the spec.
     */
    void add_server(const server_info& server);

    /**
     * @brief Registers a route for documentation.
     */
    void register_route(const domain::route& route);

    /**
     * @brief Generates the OpenAPI 3.0 JSON specification.
     */
    std::string generate_openapi_json() const;

    /**
     * @brief Generates a minimal Swagger UI HTML page.
     */
    std::string generate_swagger_ui_html(const std::string& spec_url = "/openapi.json") const;

private:
    std::string method_to_string(domain::http_method method) const;

    api_info info_;
    std::vector<server_info> servers_;
    std::vector<domain::route> routes_;
};

}

#endif
