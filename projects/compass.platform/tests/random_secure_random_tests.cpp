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
#include "compass.platform/random/secure_random.hpp"

#include <array>
#include <set>
#include <string>
#include <string_view>
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace compass::platform::random;

constexpr std::string_view alphanumeric =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

}

TEST_CASE("two draws of secure random bytes differ", "[random]") {
    std::array<std::byte, 300> a{};
    std::array<std::byte, 300> b{};
    fill_secure_random(a);
    fill_secure_random(b);
    CHECK(a != b);
}

TEST_CASE("a secure random string has the length asked for and every "
    "character of the alphabet, and no other", "[random]") {
    const auto s = secure_random_string(500, "abc");
    REQUIRE(s.size() == 500);
    const std::set<char> seen(s.begin(), s.end());
    CHECK(seen == std::set<char>{'a', 'b', 'c'});
}

TEST_CASE("two secure random strings differ", "[random]") {
    CHECK(secure_random_string(32, alphanumeric) !=
        secure_random_string(32, alphanumeric));
    CHECK(secure_random_string(0, alphanumeric).empty());
}
