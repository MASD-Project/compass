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
#include "compass.platform/text/decoding.hpp"

#include <catch2/catch_test_macros.hpp>

namespace pt = compass::platform::text;

TEST_CASE("strict UTF-8 refuses what Python's decoder refuses",
    "[text][decoding]") {
    CHECK(pt::is_utf8("caf\xc3\xa9 \xf0\x9f\x98\x80"));
    CHECK_FALSE(pt::is_utf8("\xc0\xaf"));
    CHECK_FALSE(pt::is_utf8("\xed\xa0\x80"));
    CHECK_FALSE(pt::is_utf8("\xf4\x90\x80\x80"));
    CHECK_FALSE(pt::is_utf8("a\xc3"));
    CHECK_FALSE(pt::is_utf8("\x80"));
}

TEST_CASE("an invalid byte is dropped or replaced", "[text][decoding]") {
    CHECK(pt::decode_ignoring("a\xff" "b\xc3\xa9") == "ab\xc3\xa9");
    CHECK(pt::decode_replacing("a\xff" "b") == "a\xef\xbf\xbd" "b");
}

TEST_CASE("line ends read as Python reads them", "[text][decoding]") {
    CHECK(pt::universal_newlines("a\r\nb\rc\n") == "a\nb\nc\n");
}
