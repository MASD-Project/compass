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
#ifndef COMPASS_DB_TYPES_ENVIRONMENT_HPP
#define COMPASS_DB_TYPES_ENVIRONMENT_HPP

#include <cstdint>
#include <string>
#include "compass.db/types/environment_row.hpp"
#include "compass.db/types/placement_row.hpp"

namespace compass::db::types {

/**
 * @brief An environment as the database holds it, with how many paths it
 * places; synced_at is the server's text for the time, empty when never
 * synced.
 */
struct environment_summary {
    std::string name;
    std::string kind;
    std::string root;
    std::string head;
    std::string synced_at;
    std::uint64_t documents = 0;
};

}

#endif
