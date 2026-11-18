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
#include "compass.platform/process/liveness.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include "compass.platform/process/environment.hpp"
#include "compass.platform/process/run_program.hpp"

namespace process = compass::platform::process;

TEST_CASE("this process is running", "[process][liveness]") {
    CHECK(process::is_running(process::current_process_id()));
}

TEST_CASE("an ID no process can have is not running",
    "[process][liveness]") {
    CHECK_FALSE(process::is_running(0));
    CHECK_FALSE(process::is_running(
        std::numeric_limits<std::uint64_t>::max()));
}

TEST_CASE("a program that has exited is not running",
    "[process][liveness][posix]") {
    const auto r = process::run_program({{"sh", "-c", "echo $$"}, {}, {}});
    REQUIRE(r.exit_code == 0);
    CHECK_FALSE(process::is_running(std::stoull(r.output)));
}

TEST_CASE("init is running, whether or not this user may signal it",
    "[process][liveness][posix]") {
    CHECK(process::is_running(1));
}
