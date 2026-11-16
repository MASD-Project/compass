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
#include "compass.codegen/ops/compile.hpp"

#include <catch2/catch_test_macros.hpp>

namespace {

using namespace compass::codegen;
using types::node_id;
using types::node_kind;

}

TEST_CASE("a section's body is the nodes up to its end",
    "[codegen][compile]") {
    const auto t = ops::compile("a{{#s}}b{{x}}{{/s}}c");
    REQUIRE(t.size() == 5);
    CHECK(t.kinds[node_id{1}] == node_kind::section);
    CHECK(t.text(node_id{1}) == "s");
    CHECK(t.ends[node_id{1}] == node_id{4});
    CHECK(t.kinds[node_id{3}] == node_kind::escaped);
    CHECK(t.text(node_id{4}) == "c");
}

TEST_CASE("comments and delimiter changes leave no node",
    "[codegen][compile]") {
    const auto t = ops::compile("{{! c }}{{=<% %>=}}<% x %><%={{ }}=%>{{{y}}}");
    REQUIRE(t.size() == 2);
    CHECK(t.kinds[node_id{0}] == node_kind::escaped);
    CHECK(t.text(node_id{0}) == "x");
    CHECK(t.kinds[node_id{1}] == node_kind::literal);
    CHECK(t.text(node_id{1}) == "y");
}

TEST_CASE("sections that do not pair are refused", "[codegen][compile]") {
    CHECK_THROWS_AS(ops::compile("{{#a}}{{/b}}"), ops::template_error);
    CHECK_THROWS_AS(ops::compile("{{/a}}"), ops::template_error);
    CHECK_THROWS_AS(ops::compile("{{#a}}"), ops::template_error);
    CHECK_THROWS_AS(ops::compile("{{=x=}}"), ops::template_error);
}
