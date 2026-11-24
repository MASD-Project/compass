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
 * Template: masd_cpp_row.hpp.mustache
 * To modify, update the template and regenerate.
 */
#ifndef COMPASS_DB_TYPES_SCENARIO_ROW_HPP
#define COMPASS_DB_TYPES_SCENARIO_ROW_HPP

#include <optional>
#include <string>
#include "compass.store/types/blob_id.hpp"

namespace compass::db::types {

/**
 * @brief The test definition of a test_scenario document.
 *
 * A row of test.scenario.
 */
struct scenario_row {
    std::optional<std::string> org_id;
    std::optional<std::string> title;
    std::optional<std::string> task_id;
    std::optional<std::string> story_id;
    std::optional<std::string> target;
    std::optional<std::string> environment;
    store::types::blob_id definition;

    bool operator==(const scenario_row&) const = default;
};

}

#endif
