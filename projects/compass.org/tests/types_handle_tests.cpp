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
#include "compass.org/types/handle.hpp"

#include <concepts>
#include <cstdint>
#include <type_traits>
#include <catch2/catch_test_macros.hpp>
#include "compass.org/types/tables.hpp"

namespace {

using namespace compass::org::types;

template <typename Column, typename Handle>
concept indexable_by = requires(const Column& c, Handle h) { c[h]; };

static_assert(sizeof(heading_id) == 4);
static_assert(std::is_trivially_copyable_v<heading_id>);

static_assert(!std::is_convertible_v<heading_id, block_id>);
static_assert(!std::is_constructible_v<block_id, heading_id>);
static_assert(!std::is_assignable_v<block_id&, heading_id>);
static_assert(!std::is_convertible_v<std::uint32_t, heading_id>);
static_assert(!std::is_convertible_v<heading_id, std::uint32_t>);
static_assert(!std::equality_comparable_with<heading_id, block_id>);

static_assert(indexable_by<decltype(heading_table::levels), heading_id>);
static_assert(!indexable_by<decltype(heading_table::levels), block_id>);
static_assert(!indexable_by<decltype(heading_table::levels), element_id>);
static_assert(!indexable_by<decltype(heading_table::levels), std::uint32_t>);
static_assert(!indexable_by<decltype(block_table::bodies), heading_id>);
static_assert(!indexable_by<decltype(element_table::kinds), object_id>);
static_assert(!indexable_by<decltype(object_table::kinds), element_id>);

}

TEST_CASE("a default handle indexes no row", "[types][handle]") {
    const element_id h;
    CHECK(h.is_none());
    CHECK(h == element_id::none());
    CHECK_FALSE(element_id{0}.is_none());
}

TEST_CASE("handles of one table order by row", "[types][handle]") {
    CHECK(element_id{3} < element_id{4});
    CHECK(element_id{3}.next() == element_id{4});
    CHECK(element_id{7} != element_id{8});
}
