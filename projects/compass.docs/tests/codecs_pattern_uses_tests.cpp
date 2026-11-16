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
#include "compass.docs/codecs/pattern_uses.hpp"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include "test_tree.hpp"

namespace {

using namespace compass::docs;
using tests::files;

constexpr std::string_view pattern_id = "11111111-1111-1111-1111-111111111111";
constexpr std::string_view use_id = "22222222-2222-2222-2222-222222222222";
constexpr std::string_view other_id = "33333333-3333-3333-3333-333333333333";
constexpr std::string_view missing_id = "44444444-4444-4444-4444-444444444444";

std::string link(std::string_view id) {
    return "[[id:" + std::string(id) + "][page]]";
}

std::string pattern(std::string_view uses) {
    return ":PROPERTIES:\n:ID: " + std::string(pattern_id) + "\n:END:\n"
        "#+type: service_architecture_pattern\n* Summary\n\n"
        "* In ORE Studio\n" + std::string(uses) + "\n* See also\n";
}

std::vector<std::string> sorted(std::vector<std::string> v) {
    std::ranges::sort(v);
    return v;
}

}

TEST_CASE("a pattern and its uses that link both ways pass",
    "[codecs][pattern_uses]") {
    const files t{
        {"doc/knowledge/pattern.org", pattern(link(use_id))},
        {"projects/ores.a/modeling/component_overview.org",
         ":ID: " + std::string(use_id) + "\n* Patterns used\n" +
         link(pattern_id) + "\n"}};
    CHECK(tests::run(codecs::pattern_uses, t).empty());
}

TEST_CASE("each one-way link is reported", "[codecs][pattern_uses]") {
    const files t{
        {"doc/knowledge/pattern.org",
         pattern(link(use_id) + link(missing_id))},
        {"projects/ores.a/modeling/component_overview.org",
         ":ID: " + std::string(use_id) + "\n* Summary\nno link back\n"},
        {"projects/ores.b/modeling/component_overview.org",
         ":ID: " + std::string(other_id) + "\n* Patterns used\n" +
         link(pattern_id) + link(use_id) + "\n"},
        {"doc/agile/story.org",
         ":ID: 55555555-5555-5555-5555-555555555555\n* Patterns used\n" +
         link(missing_id) + "\n"}};
    CHECK(sorted(tests::messages(tests::run(codecs::pattern_uses, t))) ==
        sorted({
            "doc/knowledge/pattern.org lists id " + std::string(missing_id) +
            " under In ORE Studio, which no page has",
            "projects/ores.a/modeling/component_overview.org is a use "
            "listed by doc/knowledge/pattern.org but does not link the "
            "pattern back",
            "projects/ores.b/modeling/component_overview.org lists "
            "projects/ores.a/modeling/component_overview.org under Patterns "
            "used, which is not a service architecture pattern",
            "projects/ores.b/modeling/component_overview.org uses "
            "doc/knowledge/pattern.org, which does not list it under In ORE "
            "Studio"}));
}

TEST_CASE("an ID two pages share is reported only when a link involves it",
    "[codecs][pattern_uses]") {
    files t{
        {"doc/knowledge/pattern.org", pattern(link(use_id))},
        {"projects/ores.a/modeling/component_overview.org",
         ":ID: " + std::string(use_id) + "\n* Patterns used\n" +
         link(pattern_id) + "\n"},
        {"projects/ores.a/modeling/copy.org",
         ":ID: " + std::string(use_id) + "\n* Patterns used\n" +
         link(pattern_id) + "\n"},
        {"doc/x.org", ":ID: " + std::string(other_id) + "\n"},
        {"doc/y.org", ":ID: " + std::string(other_id) + "\n"}};
    CHECK(tests::messages(tests::run(codecs::pattern_uses, t)) ==
        std::vector<std::string>{"id " + std::string(use_id) + " is shared "
            "by projects/ores.a/modeling/component_overview.org, "
            "projects/ores.a/modeling/copy.org"});
}
