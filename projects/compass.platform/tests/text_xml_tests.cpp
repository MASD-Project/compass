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
#include "compass.platform/text/xml.hpp"

#include <catch2/catch_test_macros.hpp>

using compass::platform::text::xml_document;

TEST_CASE("text and tails are split as ElementTree splits them, with CDATA, "
    "references and line ends resolved", "[text][xml]") {
    const auto doc = xml_document::parse(
        "<?xml version=\"1.0\"?>\r\n<!-- dropped -->\r\n"
        "<r>a&lt;b<![CDATA[ <c> ]]>&#65;<x k=\"1\" j=\"2\">in</x>tail\r\nend"
        "<!-- c --><y/></r>");
    REQUIRE(doc);
    const auto& r = doc->root();
    CHECK(r.tag == "r");
    CHECK(r.text == "a<b <c> A");
    REQUIRE(r.children.size() == 2);
    const auto& x = (*doc)[r.children[0]];
    CHECK(x.text == "in");
    CHECK(x.tail == "tail\nend");
    CHECK(xml_document::get(x, "j") == "2");
    CHECK_FALSE(xml_document::get(x, "z"));
    const auto& y = (*doc)[r.children[1]];
    CHECK_FALSE(y.text);
    CHECK_FALSE(y.tail);
}

TEST_CASE("paths find children step by step, and findtext tells a missing "
    "element from an empty one", "[text][xml]") {
    const auto doc = xml_document::parse(
        "<a><b><c>1</c><c>2</c></b><d/><b><c>3</c></b><e><c>4</c></e></a>");
    REQUIRE(doc);
    const auto found = doc->find_all(doc->root(), "b/c");
    REQUIRE(found.size() == 3);
    CHECK(found[2]->text == "3");
    CHECK(doc->find_text(doc->root(), "d") == "");
    CHECK_FALSE(doc->find_text(doc->root(), "z"));
    CHECK(doc->iter(doc->root(), "c").size() == 4);
}

TEST_CASE("a namespaced name is spelled {uri}local, and broken XML is "
    "refused", "[text][xml]") {
    const auto doc = xml_document::parse(
        "<xs:schema xmlns:xs=\"http://w/XMLSchema\"><xs:element name=\"n\"/>"
        "</xs:schema>");
    REQUIRE(doc);
    CHECK(doc->root().tag == "{http://w/XMLSchema}schema");
    CHECK_FALSE(xml_document::parse("<a><b></a>"));
    CHECK_FALSE(xml_document::parse("<a>&undefined;</a>"));
}
