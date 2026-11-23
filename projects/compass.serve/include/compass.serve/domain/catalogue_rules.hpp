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
 * Template: masd_cpp_http_service_catalogue_rules.hpp.mustache
 * To modify, update the template and regenerate.
 */
#ifndef COMPASS_SERVE_DOMAIN_CATALOGUE_RULES_HPP
#define COMPASS_SERVE_DOMAIN_CATALOGUE_RULES_HPP

#include "compass.serve/domain/http_request.hpp"
#include "compass.serve/domain/route.hpp"
#include <string>
#include <string_view>

namespace compass::serve::domain {

/**
 * @brief Whether an area is one of the published areas.
 */
bool is_published_area(std::string_view area);

/**
 * @brief Whether a route that changes something by its method may declare
 * that it does not write: only a bulk get, or an operation named for reading.
 */
bool may_declare_read(const route_catalogue& catalogue);

/**
 * @brief Whether an operation only reads, by its name: one that lists, gets,
 * reads, finds, searches or looks up changes nothing, unless it also makes, as
 * get_or_create does.
 */
bool operation_only_reads(std::string_view name);

}

#endif
