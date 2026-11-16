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
#include "compass.org/ops/document_fields.hpp"

#include <string_view>
#include <catch2/catch_test_macros.hpp>
#include "compass.ingest/org/parser.hpp"

namespace {

namespace ot = compass::org::types;
namespace ops = compass::org::ops;

constexpr std::string_view page =
    ":PROPERTIES:\n"
    ":ID: DOC-1\n"
    ":END:\n"
    "#+TITLE:   A title  \n"
    "#+type: task\n"
    "#+type: story\n"
    "\n"
    "* Goal\n"
    ":PROPERTIES:\n"
    ":ID: HEADING-1\n"
    ":CUSTOM_ID: goal\n"
    ":END:\n";

ot::tables parsed(std::string_view source) {
    ot::tables t;
    compass::ingest::org::parse(source, t);
    return t;
}

}

TEST_CASE("a keyword is found by its lower-case key and the first one wins",
    "[ops][document_fields]") {
    const auto tables = parsed(page);
    const auto t = ot::view(tables);

    const auto title = ops::find_keyword(t, "title");
    REQUIRE_FALSE(title.is_none());
    CHECK(ops::trimmed(page, t.keywords.values[title]).of(page) == "A title");

    const auto type = ops::find_keyword(t, "type");
    REQUIRE_FALSE(type.is_none());
    CHECK(ops::trimmed(page, t.keywords.values[type]).of(page) == "task");

    CHECK(ops::find_keyword(t, "description").is_none());
}

TEST_CASE("a document property is one of the drawer before the first heading",
    "[ops][document_fields]") {
    const auto tables = parsed(page);
    const auto t = ot::view(tables);

    const auto id = ops::find_document_property(t, "ID");
    REQUIRE_FALSE(id.is_none());
    CHECK(ops::trimmed(page, t.properties.values[id]).of(page) == "DOC-1");
    CHECK(ops::find_document_property(t, "CUSTOM_ID").is_none());
}

TEST_CASE("a document with no drawer of its own has no document property",
    "[ops][document_fields]") {
    constexpr std::string_view source =
        "#+title: t\n* Goal\n:PROPERTIES:\n:ID: HEADING-1\n:END:\n";
    const auto tables = parsed(source);
    CHECK(ops::find_document_property(ot::view(tables), "ID").is_none());

    constexpr std::string_view headless = ":PROPERTIES:\n:ID: ONLY\n:END:\n";
    const auto alone = parsed(headless);
    CHECK_FALSE(
        ops::find_document_property(ot::view(alone), "ID").is_none());
}

TEST_CASE("trimming removes blanks at both ends and keeps an empty range "
    "empty", "[ops][document_fields]") {
    constexpr std::string_view source = " \t a b \r\n";
    CHECK(ops::trimmed(source, {0, 9}).of(source) == "a b");
    CHECK(ops::trimmed(source, {0, 2}).empty());
}
