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
#include "compass.platform/process/environment.hpp"

#include <cstdlib>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include "compass.platform/process/run_program.hpp"

namespace process = compass::platform::process;

TEST_CASE("a variable set is seen by this process and the programs it runs",
    "[process][environment]") {
    process::set_environment_variable("COMPASS_PLATFORM_TEST", "a value");
    CHECK(process::environment_variable("COMPASS_PLATFORM_TEST") ==
        "a value");

    const auto r = process::run_program({{"sh", "-c",
        "printf %s \"$COMPASS_PLATFORM_TEST\""}, {}, {}});
    CHECK(r.output == "a value");
}

TEST_CASE("the home directory is HOME when it is set",
    "[process][environment]") {
    const auto saved =
        process::environment_variable("HOME").value_or(std::string());
    process::set_environment_variable("HOME", "/compass/home");
    CHECK(process::home_directory() == "/compass/home");
    process::set_environment_variable("HOME", saved);
}
