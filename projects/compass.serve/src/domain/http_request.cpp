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
 * Template: masd_cpp_http_service_http_request_impl.cpp.mustache
 * To modify, update the template and regenerate.
 */
#include "compass.serve/domain/http_request.hpp"
#include <algorithm>

namespace compass::serve::domain {

std::string http_request::get_header(const std::string& name) const {
    // Headers are stored with lowercase keys (normalized on insertion)
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    auto it = headers.find(lower_name);
    if (it != headers.end()) {
        return it->second;
    }
    return "";
}

std::string http_request::get_query_param(const std::string& name) const {
    auto it = query_params.find(name);
    return it != query_params.end() ? it->second : "";
}

std::string http_request::get_path_param(const std::string& name) const {
    auto it = path_params.find(name);
    return it != path_params.end() ? it->second : "";
}

}
