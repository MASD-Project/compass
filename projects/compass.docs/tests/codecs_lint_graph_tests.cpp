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
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include "compass.docs/codecs/lint.hpp"
#include "test_tree.hpp"

namespace {

using namespace compass::docs;
using tests::files;

constexpr std::string_view pattern_id = "11111111-1111-1111-1111-111111111111";
constexpr std::string_view use_id = "22222222-2222-2222-2222-222222222222";
constexpr std::string_view other_id = "33333333-3333-3333-3333-333333333333";
constexpr std::string_view missing_id = "44444444-4444-4444-4444-444444444444";
constexpr std::string_view bare_id = "66666666-6666-6666-6666-666666666666";

std::string link(std::string_view id) {
    return "[[id:" + std::string(id) + "][page]]";
}

std::string drawer(std::string_view id) {
    return ":PROPERTIES:\n:ID: " + std::string(id) + "\n:END:\n";
}

std::string pattern(std::string_view uses) {
    return drawer(pattern_id) + "#+type: service_architecture_pattern\n"
        "* Summary\n\n* In ORE Studio\n" + std::string(uses) +
        "\n* See also\n";
}

std::string use(std::string_view id, std::string_view used) {
    return drawer(id) + "* Patterns used\n" + std::string(used) + "\n";
}

std::vector<std::string> sorted(std::vector<std::string> v) {
    std::ranges::sort(v);
    return v;
}

}

TEST_CASE("a pattern and its uses that link both ways pass, a bare link "
    "back included", "[codecs][lint][pattern]") {
    const files t{
        {"doc/knowledge/pattern.org",
         pattern(link(use_id) + "\n" + link(bare_id))},
        {"projects/ores.a/modeling/component_overview.org",
         use(use_id, link(pattern_id))},
        {"projects/ores.c/modeling/component_overview.org",
         use(bare_id, "[[id:" + std::string(pattern_id) + "]]")}};
    CHECK(tests::run(codecs::lint, t).empty());
}

TEST_CASE("each one-way pattern link is reported", "[codecs][lint][pattern]") {
    const files t{
        {"doc/knowledge/pattern.org",
         pattern(link(use_id) + link(missing_id))},
        {"projects/ores.a/modeling/component_overview.org",
         drawer(use_id) + "* Summary\nno link back\n"
         "#+begin_src org\n" + link(pattern_id) + "\n#+end_src\n"},
        {"projects/ores.b/modeling/component_overview.org",
         use(other_id, link(pattern_id) + link(use_id))},
        {"doc/agile/story.org",
         use("55555555-5555-5555-5555-555555555555", link(missing_id))}};
    CHECK(sorted(tests::messages(tests::run(codecs::lint, t))) ==
        sorted({
            "[[id:" + std::string(missing_id) + "]] -> page resolves to no "
            ":ID: in the corpus",
            "[[id:" + std::string(missing_id) + "]] -> page resolves to no "
            ":ID: in the corpus",
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

TEST_CASE("an ID two pages share is reported only when a pattern link "
    "involves it", "[codecs][lint][pattern]") {
    const files t{
        {"doc/knowledge/pattern.org", pattern(link(use_id))},
        {"projects/ores.a/modeling/component_overview.org",
         use(use_id, link(pattern_id))},
        {"projects/ores.a/modeling/copy.org", use(use_id, link(pattern_id))},
        {"doc/x.org", drawer(other_id)},
        {"doc/y.org", drawer(other_id)}};
    CHECK(tests::messages(tests::run(codecs::lint, t)) ==
        std::vector<std::string>{"id " + std::string(use_id) + " is shared "
            "by projects/ores.a/modeling/component_overview.org, "
            "projects/ores.a/modeling/copy.org"});
}
