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
#include "compass.org/io/dump.hpp"

#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace compass::org::types;

/**
 * Fills the tables by hand. Elements and objects are added in pre-order
 * with the end of their subtree given, as a parser leaves them.
 */
struct builder {
    tables t;

    name_id name(std::string_view text) {
        t.names.firsts.values.push_back(
            static_cast<std::uint32_t>(t.name_bytes.size()));
        t.name_bytes.insert(t.name_bytes.end(), text.begin(), text.end());
        return {static_cast<std::uint32_t>(t.names.size() - 1)};
    }

    element_id element(element_kind kind, source_range range,
        source_range contents, std::uint32_t subtree_end,
        std::uint8_t post_blank = 0) {
        auto& e = t.elements;
        e.kinds.values.push_back(kind);
        e.post_blanks.values.push_back(post_blank);
        e.ranges.values.push_back(range);
        e.contents.values.push_back(contents);
        e.parents.values.push_back(element_id::none());
        e.subtree_ends.values.push_back({subtree_end});
        e.object_firsts.values.push_back(
            {static_cast<std::uint32_t>(t.objects.size())});
        return {static_cast<std::uint32_t>(e.size() - 1)};
    }

    object_id object(object_kind kind, source_range range,
        source_range contents, std::uint32_t subtree_end,
        std::uint8_t post_blank = 0) {
        auto& o = t.objects;
        o.kinds.values.push_back(kind);
        o.post_blanks.values.push_back(post_blank);
        o.ranges.values.push_back(range);
        o.contents.values.push_back(contents);
        o.subtree_ends.values.push_back({subtree_end});
        return {static_cast<std::uint32_t>(o.size() - 1)};
    }

    void affiliated(element_id owner, name_id key, source_range value) {
        auto& a = t.affiliated;
        a.owners.values.push_back(owner);
        a.names.values.push_back(key);
        a.options.values.push_back({});
        a.values.values.push_back(value);
        a.object_firsts.values.push_back({0});
    }

    std::string dump(std::string_view source) const {
        std::ostringstream os;
        compass::org::io::dump(os, source, view(t));
        return os.str();
    }
};

}

TEST_CASE("dump indents elements and objects by their depth", "[io][dump]") {
    const std::string_view source = "*a /b/*  \n\n";
    builder b;
    b.element(element_kind::document, {0, 11}, {0, 11}, 2);
    b.element(element_kind::paragraph, {0, 11}, {0, 10}, 2, 1);
    b.object(object_kind::bold, {0, 9}, {1, 6}, 2, 2);
    b.object(object_kind::italic, {3, 6}, {4, 5}, 2);

    CHECK(b.dump(source) ==
        "org-data 0 11 0 11 0\n"
        "  paragraph 0 11 0 10 1\n"
        "    bold 0 9 1 6 2\n"
        "      italic 3 6 4 5 0\n");
}

TEST_CASE("dump prints an empty contents range as dashes and escapes values",
    "[io][dump]") {
    const std::string_view source = "#+k: a\\b\tc\n";
    builder b;
    b.element(element_kind::document, {0, 11}, {0, 11}, 2);
    const auto keyword = b.element(element_kind::keyword, {0, 11}, {}, 2);
    b.t.keywords.elements.values.push_back(keyword);
    b.t.keywords.names.values.push_back(b.name("k"));
    b.t.keywords.values.values.push_back({5, 10});

    CHECK(b.dump(source) ==
        "org-data 0 11 0 11 0\n"
        "  keyword 0 11 - - 0 key=k value=a\\\\b\\tc\n");
}

TEST_CASE("dump sorts affiliated keywords and keeps the last of a name",
    "[io][dump]") {
    const std::string_view source =
        "#+name: a\n#+attr_x: 1\n#+name: b\n#+attr_x: 2\ntext\n";
    builder b;
    b.element(element_kind::document, {0, 49}, {0, 49}, 2);
    const auto paragraph =
        b.element(element_kind::paragraph, {0, 49}, {44, 49}, 2);
    const auto name = b.name("name");
    const auto attribute = b.name("attr_x");
    b.affiliated(paragraph, name, {8, 9});
    b.affiliated(paragraph, attribute, {20, 21});
    b.affiliated(paragraph, name, {30, 31});
    b.affiliated(paragraph, attribute, {42, 43});

    CHECK(b.dump(source) ==
        "org-data 0 49 0 49 0\n"
        "  paragraph 0 49 44 49 0\n"
        "    affiliated key=attr_x value=1\n"
        "    affiliated key=attr_x value=2\n"
        "    affiliated key=name value=b\n");
}

TEST_CASE("dump prints a link path as org-element reads it", "[io][dump]") {
    const std::string_view source = "[[id:a\\]b\n  c]]\n";
    builder b;
    b.element(element_kind::document, {0, 16}, {0, 16}, 2);
    b.element(element_kind::paragraph, {0, 16}, {0, 16}, 2);
    const auto link = b.object(object_kind::link, {0, 15}, {}, 1);
    b.t.links.objects.values.push_back(link);
    b.t.links.types.values.push_back(b.name("id"));
    b.t.links.formats.values.push_back(link_format::bracket);
    b.t.links.paths.values.push_back({5, 13});

    CHECK(b.dump(source) ==
        "org-data 0 16 0 16 0\n"
        "  paragraph 0 16 0 16 0\n"
        "    link 0 15 - - 0 type=id format=bracket path=a]b c\n");
}
