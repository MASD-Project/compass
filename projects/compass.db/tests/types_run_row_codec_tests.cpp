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
/**
 * AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Template: masd_cpp_json_tests.cpp.mustache
 * To modify, update the template and regenerate.
 */
#include "compass.db/types/run_row_codec.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>
#include "compass.db/codec/json_schema.hpp"
#include "compass.db/codec/member_count.hpp"
#include "compass.db/codec/sample.hpp"
#include "compass.db/codec/schema_check.hpp"

namespace {

using roundtrip_type = compass::db::types::run_row;
using roundtrip_traits = compass::db::codec::wire_traits<roundtrip_type>;

}

static_assert(compass::db::codec::names_every_member<roundtrip_type>(),
    "run_row has a member its wire shape does not name");
static_assert(roundtrip_traits::is_generated);

TEST_CASE("run_row reads back the JSON it writes", "[codec][roundtrip]") {
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<roundtrip_type>(which);
        CHECK(roundtrip_traits::read_json(roundtrip_traits::write_json(v)) == v);
    }
    const roundtrip_type empty{};
    CHECK(roundtrip_traits::read_json(roundtrip_traits::write_json(empty)) == empty);
}

TEST_CASE("run_row's JSON Schema allows the JSON it writes",
    "[codec][schema]") {
    const auto schema = compass::db::codec::json_schema<roundtrip_type>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto text = roundtrip_traits::write_json(
            compass::db::codec::sample<roundtrip_type>(which));
        const auto violation = compass::db::codec::schema_violation(schema, text);
        INFO(text);
        CHECK(violation.value_or("") == "");
    }
}
