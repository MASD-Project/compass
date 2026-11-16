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
#include "compass.ingest/org/parser.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "dump_of.hpp"

namespace {

using namespace compass::org::types;
using compass::ingest::org::parse;
using compass::ingest::tests::dump_of;

std::string_view name_of(const tables& t, name_id id) {
    const auto first = t.names.firsts[id];
    const auto next = id.value + 1 < t.names.size()
        ? t.names.firsts.values[id.value + 1]
        : static_cast<std::uint32_t>(t.name_bytes.size());
    return {t.name_bytes.data() + first, next - first};
}

tables parsed(std::string_view source) {
    tables t;
    parse(source, t);
    return t;
}

}

TEST_CASE("every element names its parent and the end of its subtree",
    "[org][tables]") {
    const std::string_view source = "* H\n- a\n  - b\ntext\n";
    const auto t = parsed(source);

    const std::vector<element_kind> kinds = {element_kind::document,
        element_kind::heading, element_kind::section,
        element_kind::plain_list, element_kind::item,
        element_kind::paragraph, element_kind::plain_list,
        element_kind::item, element_kind::paragraph,
        element_kind::paragraph};
    REQUIRE(t.elements.kinds.values == kinds);

    const std::vector<std::uint32_t> parents = {element_id::none_value, 0,
        1, 2, 3, 4, 4, 6, 7, 2};
    const std::vector<std::uint32_t> ends = {10, 10, 10, 9, 9, 6, 9, 9, 9,
        10};
    for (std::uint32_t i = 0; i < t.elements.size(); ++i) {
        CAPTURE(i);
        CHECK(t.elements.parents[element_id{i}].value == parents[i]);
        CHECK(t.elements.subtree_ends[element_id{i}].value == ends[i]);
    }
}

TEST_CASE("a block row holds its name, parameters and body",
    "[org][tables]") {
    const std::string_view source =
        "#+begin_src cpp  -n :tangle x  \n"
        "int x;\n"
        "#+end_src\n"
        "#+begin_quote  ignored\n"
        "q\n"
        "#+end_quote\n"
        "#+begin_export html\n"
        "#+end_export\n";
    const auto t = parsed(source);
    REQUIRE(t.blocks.size() == 3);

    const block_id src{0};
    CHECK(t.elements.kinds[t.blocks.elements[src]] ==
        element_kind::src_block);
    CHECK(name_of(t, t.blocks.names[src]) == "cpp");
    CHECK(t.blocks.parameters[src].of(source) == "-n :tangle x");
    CHECK(t.blocks.bodies[src].of(source) == "int x;\n");

    const block_id quote{1};
    CHECK(t.blocks.names[quote].is_none());
    CHECK(t.blocks.parameters[quote].of(source) == "ignored");
    CHECK(t.blocks.bodies[quote].of(source) == "q\n");

    const block_id exported{2};
    CHECK(name_of(t, t.blocks.names[exported]) == "html");
    CHECK(t.blocks.bodies[exported].empty());
}

TEST_CASE("an item row holds its checkbox and the range of its tag",
    "[org][tables]") {
    const std::string_view source = "- [X] term one :: d\n- plain\n";
    const auto t = parsed(source);
    REQUIRE(t.items.size() == 2);

    CHECK(t.items.checkboxes[item_id{0}] == checkbox::on);
    CHECK(t.items.tags[item_id{0}].of(source) == "term one");
    CHECK(t.items.checkboxes[item_id{1}] == checkbox::none);
    CHECK(t.items.tags[item_id{1}].empty());
    REQUIRE(t.lists.size() == 1);
    CHECK(t.lists.types[list_id{0}] == list_type::descriptive);
}

TEST_CASE("an affiliated row holds its owner, translated name, option and "
          "value", "[org][tables]") {
    const std::string_view source =
        "#+caption[short]: long *b*\n"
        "#+tblname: t\n"
        "#+attr_html: :w 1\n"
        "| a |\n";
    const auto t = parsed(source);
    REQUIRE(t.affiliated.size() == 3);

    const auto owner = t.affiliated.owners[affiliated_id{0}];
    CHECK(t.elements.kinds[owner] == element_kind::table);
    CHECK(t.affiliated.owners[affiliated_id{2}] == owner);
    CHECK(t.elements.ranges[owner].begin == 0);

    CHECK(name_of(t, t.affiliated.names[affiliated_id{0}]) == "caption");
    CHECK(t.affiliated.options[affiliated_id{0}].of(source) == "short");
    CHECK(t.affiliated.values[affiliated_id{0}].of(source) == "long *b*");
    CHECK(name_of(t, t.affiliated.names[affiliated_id{1}]) == "name");
    CHECK(t.affiliated.values[affiliated_id{1}].of(source) == "t");
    CHECK(name_of(t, t.affiliated.names[affiliated_id{2}]) == "attr_html");
}

TEST_CASE("the objects of a caption come before the objects of the elements",
    "[org][tables]") {
    const std::string_view source = "/i/\n\n#+caption: *b*\n| =v= |\n";
    const auto t = parsed(source);

    REQUIRE(t.objects.size() == 4);
    CHECK(t.objects.kinds[object_id{0}] == object_kind::bold);
    CHECK(t.affiliated.object_firsts[affiliated_id{0}] == object_id{0});
    CHECK(t.elements.object_firsts[element_id{0}] == object_id{1});
    CHECK(t.objects.kinds[object_id{1}] == object_kind::italic);
    CHECK(t.objects.kinds[object_id{2}] == object_kind::table_cell);
    CHECK(t.objects.subtree_ends[object_id{2}] == object_id{4});
    CHECK(std::ranges::is_sorted(t.elements.object_firsts.values));
}

TEST_CASE("a heading row points at its tags and its properties",
    "[org][tables]") {
    const std::string_view source =
        ":PROPERTIES:\n:ID: doc\n:END:\n"
        "* A :x:y:\n:PROPERTIES:\n:ID: a\n:K: v\n:END:\n"
        "* B\n"
        "* C :z:\n:PROPERTIES:\n:ID: c\n:END:\n";
    const auto t = parsed(source);
    REQUIRE(t.headings.size() == 3);
    REQUIRE(t.properties.size() == 4);
    REQUIRE(t.heading_tags.size() == 3);

    CHECK(t.headings.property_firsts[heading_id{0}] == property_id{1});
    CHECK(t.headings.property_firsts[heading_id{1}] == property_id{3});
    CHECK(t.headings.property_firsts[heading_id{2}] == property_id{3});
    CHECK(t.headings.tag_firsts[heading_id{0}] == tag_id{0});
    CHECK(t.headings.tag_firsts[heading_id{1}] == tag_id{2});
    CHECK(t.headings.tag_firsts[heading_id{2}] == tag_id{2});
    CHECK(t.properties.values[property_id{0}].of(source) == "doc");
    CHECK(t.headings.titles[heading_id{0}].of(source) == "A");
    CHECK(t.headings.levels[heading_id{2}] == 1);
}

TEST_CASE("equal names of a document share one handle in canonical case",
    "[org][tables]") {
    const std::string_view source =
        "#+TITLE: a\n#+title: b\n"
        "* H\n:PROPERTIES:\n:id: x\n:ID: y\n:END:\n"
        "[[id:p]] [[id:q]]\n";
    const auto t = parsed(source);

    REQUIRE(t.keywords.size() == 2);
    CHECK(t.keywords.names[keyword_id{0}] == t.keywords.names[keyword_id{1}]);
    CHECK(name_of(t, t.keywords.names[keyword_id{0}]) == "title");
    REQUIRE(t.properties.size() == 2);
    CHECK(t.properties.names[property_id{0}] ==
        t.properties.names[property_id{1}]);
    CHECK(name_of(t, t.properties.names[property_id{0}]) == "ID");
    REQUIRE(t.links.size() == 2);
    CHECK(t.links.types[link_id{0}] == t.links.types[link_id{1}]);
    CHECK(name_of(t, t.links.types[link_id{0}]) == "id");
    CHECK(t.links.types[link_id{0}] != t.properties.names[property_id{0}]);
}

TEST_CASE("a link row holds its object, format and the path as written",
    "[org][tables]") {
    const std::string_view source = "[[file:a\\]b.org::*H][d]] <https://x.y>\n";
    const auto t = parsed(source);
    REQUIRE(t.links.size() == 2);

    CHECK(t.objects.kinds[t.links.objects[link_id{0}]] == object_kind::link);
    CHECK(t.links.formats[link_id{0}] == link_format::bracket);
    CHECK(t.links.paths[link_id{0}].of(source) == "a\\]b.org::*H");
    CHECK(t.objects.contents[t.links.objects[link_id{0}]].of(source) == "d");
    CHECK(t.links.formats[link_id{1}] == link_format::angle);
    CHECK(t.links.paths[link_id{1}].of(source) == "//x.y");
}

TEST_CASE("post-blank saturates at 255", "[org][tables]") {
    const auto source = "a\n" + std::string(300, '\n') + "b\n";
    const auto t = parsed(source);

    REQUIRE(t.elements.size() == 4);
    CHECK(t.elements.kinds[element_id{2}] == element_kind::paragraph);
    CHECK(t.elements.post_blanks[element_id{2}] == post_blank_saturated);
    CHECK(t.elements.ranges[element_id{2}].end == 302);
}

TEST_CASE("parsing into tables that hold another document replaces it",
    "[org][tables]") {
    const std::string_view first =
        "#+caption: *c*\n| a | [[id:x][y]] |\n* TODO H :t:\n:PROPERTIES:\n"
        ":ID: i\n:END:\n- [X] item :: d\n#+begin_src c\n#+end_src\n";
    const std::string_view second = "* DONE other\ntext =v=\n";

    tables reused;
    parse(first, reused);
    parse(second, reused);
    std::ostringstream os;
    compass::org::io::dump(os, second,
        compass::org::types::view(reused));

    CHECK(os.str() == dump_of(second));
    CHECK(reused.elements.size() == parsed(second).elements.size());
    CHECK(reused.affiliated.size() == 0);
    CHECK(reused.blocks.size() == 0);
    CHECK(reused.items.size() == 0);
    CHECK(reused.properties.size() == 0);
    CHECK(reused.heading_tags.size() == 0);
    CHECK(reused.names.size() == parsed(second).names.size());
    CHECK(reused.name_bytes.size() == parsed(second).name_bytes.size());
}

TEST_CASE("a document has no documents row", "[org][tables]") {
    CHECK(parsed("* H\n").documents.size() == 0);
}
