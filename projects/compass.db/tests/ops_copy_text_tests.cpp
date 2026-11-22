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
#include "compass.db/ops/copy_text.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <catch2/catch_test_macros.hpp>

namespace {

using compass::db::ops::copy_writer;

}

TEST_CASE("COPY text escapes separators and backslashes", "[ops][copy]") {
    std::string out;
    copy_writer w(out);
    w.text("a\tb\nc\\d\re");
    w.number(42);
    w.end();
    CHECK(out == "a\\tb\\nc\\\\d\\re\t42\n");
}

TEST_CASE("COPY text writes NULL, bytea and arrays", "[ops][copy]") {
    std::string out;
    copy_writer w(out);
    w.null();
    w.text(std::optional<std::string>{});
    const std::array<std::uint8_t, 3> bytes{0x00, 0xab, 0x10};
    w.bytes(bytes);
    const std::array<std::string, 3> items{"plain", "with \"quote\"",
        "back\\slash"};
    w.array(items);
    w.end();
    CHECK(out == "\\N\t\\N\t\\\\x00ab10\t"
        "{\"plain\",\"with \\\\\"quote\\\\\"\",\"back\\\\\\\\slash\"}\n");
}

TEST_CASE("COPY text drops NUL bytes and replaces bytes that are not UTF-8",
    "[ops][copy]") {
    std::string out;
    copy_writer w(out);
    w.text(std::string("a\0b", 3));
    w.text("bad\xff");
    w.end();
    CHECK(out == "ab\tbad\xef\xbf\xbd\n");
}
