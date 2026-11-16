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
#include "compass.store/ops/blob_hash.hpp"

#include <cstddef>
#include <string>
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace compass::store;

/**
 * The bytes the expected IDs were taken from: `git hash-object --stdin`
 * over byte i = (7 * i + 3) mod 251.
 */
std::string pattern(std::size_t size) {
    std::string s(size, '\0');
    for (std::size_t i = 0; i < size; ++i)
        s[i] = static_cast<char>((i * 7 + 3) % 251);
    return s;
}

std::string id_of(std::string_view content) {
    return ops::to_hex(ops::blob_id_of(content));
}

}

TEST_CASE("a blob ID is the one git hash-object prints", "[ops][blob_hash]") {
    CHECK(id_of("") == "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391");
    CHECK(id_of("hello\n") == "ce013625030ba8dba906f756967f9e9ca394464a");
    CHECK(id_of("#+title: T\n* H\n") ==
        "6dbe83622a80e8c490397de35b6ebf5d0d0492a8");
}

TEST_CASE("a blob ID is git's at every length around a SHA-1 block",
    "[ops][blob_hash]") {
    CHECK(id_of(pattern(55)) == "038186c00ebb93085677731203cbb88513d5e02b");
    CHECK(id_of(pattern(56)) == "c013b05b59e9caf36e73e191bfd526a03485a23d");
    CHECK(id_of(pattern(63)) == "51d532d8ca26803b6bf2675511a31db230b19c31");
    CHECK(id_of(pattern(64)) == "35cb9b06ff2d0481a8904db781ce9101997acee9");
    CHECK(id_of(pattern(65)) == "1090134147ca2b67e90797f415ed684a3cac45d5");
    CHECK(id_of(pattern(119)) == "230e404e6bfee782cff93264b5c53980464c4737");
    CHECK(id_of(pattern(120)) == "8aca049e1a4cae4ed35d1a9071fdb46ea77c2bd5");
    CHECK(id_of(pattern(1000)) == "d71af1ca89db590b23da93119eac26d725adf093");
}

TEST_CASE("equal content has equal IDs and different content does not",
    "[ops][blob_hash]") {
    CHECK(ops::blob_id_of("a") == ops::blob_id_of("a"));
    CHECK(ops::blob_id_of("a") != ops::blob_id_of("b"));
}
