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
#include "compass.store/ops/git_index.hpp"

#include <stdexcept>
#include <string>
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace compass::store;

types::stat_data entry() {
    types::stat_data s;
    s.ctime = {1000, 5};
    s.mtime = {1000, 7};
    s.dev = 1;
    s.ino = 2;
    s.uid = 3;
    s.gid = 4;
    s.size = 5;
    return s;
}

}

TEST_CASE("an entry is clean only when every stat field matches",
    "[ops][git_index]") {
    const types::index_time later{1001, 0};
    CHECK(ops::is_clean(entry(), entry(), later));

    const auto differs = [&](auto change) {
        auto file = entry();
        change(file);
        return !ops::is_clean(entry(), file, later);
    };
    CHECK(differs([](auto& s) { s.ctime.sec++; }));
    CHECK(differs([](auto& s) { s.ctime.nsec++; }));
    CHECK(differs([](auto& s) { s.mtime.sec--; }));
    CHECK(differs([](auto& s) { s.mtime.nsec++; }));
    CHECK(differs([](auto& s) { s.dev++; }));
    CHECK(differs([](auto& s) { s.ino++; }));
    CHECK(differs([](auto& s) { s.size++; }));
}

TEST_CASE("an entry modified in the second the index was written is racily "
    "clean", "[ops][git_index]") {
    CHECK_FALSE(ops::is_clean(entry(), entry(), {1000, 0}));
    CHECK_FALSE(ops::is_clean(entry(), entry(), {1000, 999999999}));
    CHECK_FALSE(ops::is_clean(entry(), entry(), {999, 0}));
    CHECK(ops::is_clean(entry(), entry(), {1001, 0}));
}

TEST_CASE("an entry whose size git zeroed is never clean",
    "[ops][git_index]") {
    auto zeroed = entry();
    zeroed.size = 0;
    CHECK_FALSE(ops::is_clean(zeroed, zeroed, {2000, 0}));
}

TEST_CASE("bytes that are not an index are rejected", "[ops][git_index]") {
    CHECK_THROWS_AS(ops::parse_git_index(""), std::runtime_error);
    CHECK_THROWS_AS(ops::parse_git_index(std::string(40, 'x')),
        std::runtime_error);

    std::string header("DIRC\0\0\0\x05\0\0\0\0", 12);
    CHECK_THROWS_AS(ops::parse_git_index(header + std::string(20, '\0')),
        std::runtime_error);
    header[7] = 2;
    CHECK(ops::parse_git_index(header + std::string(20, '\0')).size() == 0);
    header[11] = 1;
    CHECK_THROWS_AS(ops::parse_git_index(header + std::string(20, '\0')),
        std::runtime_error);
}
