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
 * Template: masd_cpp_http_service_catalogue_rules_impl.cpp.mustache
 * To modify, update the template and regenerate.
 */
#include "compass.serve/domain/catalogue_rules.hpp"
#include <algorithm>
#include <array>

namespace compass::serve::domain {

namespace {

/// The published areas, as the service's component overview names them.
constexpr auto published_areas = std::to_array<std::string_view>({"agile",
                                                                  "documents",
                                                                  "qa"});

}

bool is_published_area(std::string_view area) {
    return std::ranges::find(published_areas, area) != published_areas.end();
}

bool may_declare_read(const route_catalogue& catalogue) {
    return catalogue.tool.starts_with("get_many_") || operation_only_reads(catalogue.tool);
}

bool operation_only_reads(std::string_view name) {
    if (name.find("_or_") != std::string_view::npos)
        return false;
    for (const std::string_view verb : {"list_", "get_", "read_", "find_", "search_", "lookup_"})
        if (name.starts_with(verb))
            return true;
    return false;
}

}
