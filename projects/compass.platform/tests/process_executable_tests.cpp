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
#include "compass.platform/process/executable.hpp"

#include <filesystem>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("executable_path names this test binary by its real path",
    "[process]") {
    const auto path = compass::platform::process::executable_path();
    CHECK(path.is_absolute());
    CHECK(path.filename() == "compass.platform.tests");
    CHECK(std::filesystem::is_regular_file(path));
    CHECK_FALSE(std::filesystem::is_symlink(path));
}
