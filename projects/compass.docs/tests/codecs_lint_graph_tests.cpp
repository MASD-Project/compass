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
using places = std::vector<std::pair<std::string, std::uint32_t>>;

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

const std::string hub_id = "AAAAAAAA-0000-0000-0000-000000000001";
const std::string a_id = "AAAAAAAA-0000-0000-0000-000000000002";
const std::string b_id = "AAAAAAAA-0000-0000-0000-000000000003";
const std::string c_id = "AAAAAAAA-0000-0000-0000-000000000004";
const std::string d_id = "AAAAAAAA-0000-0000-0000-000000000005";
const std::string e_id = "AAAAAAAA-0000-0000-0000-000000000006";
const std::string hub2_id = "AAAAAAAA-0000-0000-0000-000000000007";

std::string missing(std::string_view page, std::string_view hub) {
    return std::string(page) + " is in the cluster of the structure note " +
        std::string(hub) + " but does not link it back";
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

TEST_CASE("each page a structure note orders, or sets aside, and that does "
    "not link it back is reported at its See also section",
    "[codecs][lint][structure_note]") {
    const files t{
        {"doc/hub.org", drawer(hub_id) + "#+title: Curves\n"
            "#+filetags: :knowledge:structure_note:\n"
            "* Order\n[[id:" + a_id + "][a]] [[id:" + d_id + "]]\n"
            "[[id:" + e_id + "][e]] [[id:" + hub2_id + "][the other note]]\n"
            "* Beside\n# aside: around the sequence\n[[id:" + b_id + "][b]]\n"
            "* See also\n[[id:" + c_id + "][c]]\n"},
        {"doc/hub2.org", drawer(hub2_id) +
            "#+filetags: :structure_note:\n"},
        {"doc/a.org", drawer(a_id) + "* Body\n\n* See also\n\n- other\n"},
        {"doc/b.org", drawer(b_id) + "* Body\n"},
        {"doc/c.org", drawer(c_id) + "* Body\n"},
        {"doc/d.org", drawer(d_id) + "See [[id:" + hub_id + "]].\n"},
        {"doc/e.org", drawer(e_id) + "#+begin_example\n[[id:" + hub_id +
            "][hub]]\n#+end_example\n"}};
    const auto found = tests::run(codecs::lint, t);
    CHECK(tests::messages(found) == std::vector<std::string>{
        missing("doc/a.org", "doc/hub.org"),
        missing("doc/e.org", "doc/hub.org"),
        missing("doc/b.org", "doc/hub.org")});
    CHECK(tests::places(found) == places{{"doc/a.org", 6}, {"doc/e.org", 1},
        {"doc/b.org", 1}});
}

TEST_CASE("the fix of a missing back-link is the line the note's pages "
    "carry", "[codecs][lint][structure_note]") {
    const files t{
        {"doc/hub.org", drawer(hub_id) + "#+title: Curves\n"
            "#+filetags: :structure_note:\n[[id:" + a_id + "][a]]\n"},
        {"doc/a.org", drawer(a_id)}};
    const auto fixes = tests::with_run(codecs::lint, t, {},
        [](const auto& r, const auto&) {
            std::vector<std::string> out;
            for (std::uint32_t i = 0; i < r.diagnostics[0].size(); ++i)
                out.emplace_back(r.diagnostics[0].fixes[
                    compass::codec::types::diagnostic_id{i}]);
            return out;
        });
    CHECK(fixes == std::vector<std::string>{"Add \"- [[id:" + hub_id +
        "][Curves]] \xe2\x80\x94 the structure note that orders this "
        "cluster, and where to read this page in it.\" to the page's See "
        "also section."});
}

TEST_CASE("a tag that only contains structure_note does not make a note",
    "[codecs][lint][structure_note]") {
    const files t{
        {"doc/hub.org", drawer(hub_id) + "#+filetags: :structure_notes:\n"
            "[[id:" + a_id + "][a]]\n"},
        {"doc/a.org", drawer(a_id)}};
    CHECK(tests::run(codecs::lint, t).empty());
}

TEST_CASE("an evidence page lends its IDs to links that no live page "
    "answers, and is itself neither linted nor counted as sharing an ID",
    "[codecs][lint][evidence]") {
    const std::string tags = "#+filetags: :evidence:Bad:\n";
    const files t{
        {"doc/knowledge/pattern.org", pattern(link(use_id))},
        {"projects/ores.a/modeling/component_overview.org",
         use(use_id, link(pattern_id))},
        {"doc/evidence/copy.org", drawer(use_id) + tags +
         "* Patterns used\n" + link(pattern_id) + link(missing_id) + "\n"},
        {"doc/evidence/only.org", drawer(other_id) + tags},
        {"doc/live.org", drawer(bare_id) + link(other_id) + "\n"}};
    CHECK(tests::run(codecs::lint, t).empty());
}
