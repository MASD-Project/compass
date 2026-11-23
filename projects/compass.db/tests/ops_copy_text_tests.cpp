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
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.db/types/step_result_row.hpp"
#include "compass.store/ops/blob_hash.hpp"

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

TEST_CASE("a generated row's field takes the COPY text form of its type",
    "[ops][copy]") {
    using compass::db::types::step_outcome;
    std::string out;
    copy_writer w(out);
    w.field(std::string_view("view"));
    w.field(std::string("string"));
    w.field(compass::store::ops::blob_id_of(std::string_view("x")));
    w.field(std::vector<std::string>{"a", "b"});
    w.field(std::int32_t{-7});
    w.field(std::int64_t{-8});
    w.field(std::uint32_t{9});
    w.field(std::size_t{10});
    w.field(true);
    w.field(false);
    w.field(step_outcome::blocked);
    w.field(std::optional<step_outcome>{});
    w.field(std::optional<step_outcome>{step_outcome::fail});
    w.field(std::optional<std::int64_t>{});
    w.field(std::optional<std::int64_t>{11});
    w.field(std::optional<bool>{true});
    w.field(std::optional<std::string>{});
    w.end();
    CHECK(out == "view\tstring\t\\\\x"
        "c1b0730e0133447badcfd47fd144e254807b06e1\t{\"a\",\"b\"}\t-7\t-8\t9\t"
        "10\tt\tf\tblocked\t\\N\tfail\t\\N\t11\tt\t\\N\n");
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
