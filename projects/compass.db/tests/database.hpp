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
#ifndef COMPASS_DB_TESTS_DATABASE_HPP
#define COMPASS_DB_TESTS_DATABASE_HPP

#include <string>
#include <catch2/catch_test_macros.hpp>
#include "compass.testing/database/scoped_cluster.hpp"

namespace compass::db::tests {

/**
 * @brief The connection string of a new, empty database in the suite's
 * throwaway cluster. Skips the test when the machine has no PostgreSQL
 * server programs.
 */
inline std::string fresh_database() {
    std::string why;
    auto* cluster = compass::testing::database::shared_cluster(
        "compass.db.tests", why);
    if (cluster == nullptr)
        SKIP(why);
    return cluster->create_database();
}

}

#endif
