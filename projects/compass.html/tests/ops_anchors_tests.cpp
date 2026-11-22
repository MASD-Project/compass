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
#include "compass.html/ops/anchors.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.ingest/org/parser.hpp"

namespace {

std::vector<std::string> anchors_of(std::string_view source) {
    compass::org::types::tables t;
    compass::ingest::org::parse(source, t);
    return compass::html::ops::heading_anchors(source,
        compass::org::types::view(t));
}

}

TEST_CASE("a heading with an ID is anchored by it", "[html][anchors]") {
    const auto a = anchors_of(
        "* Interfaces\n"
        ":PROPERTIES:\n"
        ":ID: 21C1D3CC-9EE3-48C3-8EDD-BA0C16032AB2\n"
        ":END:\n");
    REQUIRE(a == std::vector<std::string>{
        "21C1D3CC-9EE3-48C3-8EDD-BA0C16032AB2"});
}

TEST_CASE("a heading without an ID is anchored by the slug of its path",
    "[html][anchors]") {
    const auto a = anchors_of(
        "* The Interfaces\n"
        "** HTML /and/ =MCP=!\n"
        "*** [[https://example.com][A link]] here\n");
    REQUIRE(a == std::vector<std::string>{"the-interfaces",
        "the-interfaces-html-and-mcp",
        "the-interfaces-html-and-mcp-a-link-here"});
}

TEST_CASE("slugs are made unique within the document", "[html][anchors]") {
    const auto a = anchors_of(
        "* Notes\n"
        "* Notes\n"
        "* Notes\n"
        "* ?!\n");
    REQUIRE(a == std::vector<std::string>{"notes", "notes-2", "notes-3",
        "section"});
}

TEST_CASE("a repeated ID falls back to the slug", "[html][anchors]") {
    const auto a = anchors_of(
        "* One\n"
        ":PROPERTIES:\n"
        ":ID: X\n"
        ":END:\n"
        "* Two\n"
        ":PROPERTIES:\n"
        ":ID: X\n"
        ":END:\n");
    REQUIRE(a == std::vector<std::string>{"X", "two"});
}
