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
#include <catch2/catch_test_macros.hpp>

#include <array>
#include "compass.docs/codecs/skill_tables.hpp"
#include "compass.docs/ops/regions.hpp"
#include "test_tree.hpp"

namespace {

using namespace compass::docs;

const std::string catalogue = "doc/llm/skills/claude_code_skills.org";
const std::string begin = "# BEGIN generated catalogue "
    "(compass generate: skill_catalogue)";
const std::string script_begin = "# BEGIN generated catalogue "
    "(build/scripts/generate_skills_catalogue.py)";

std::string skill(std::string_view id, std::string_view description) {
    return ":PROPERTIES:\n:ID: " + std::string(id) + "\n:END:\n"
        "#+title: A skill\n#+begin_src yaml\ndescription: " +
        std::string(description) + "\n#+end_src\n";
}

}

TEST_CASE("the catalogue groups skills by domain in name order and cuts a "
    "long description", "[codecs][skill_tables]") {
    const auto out = tests::generate(codecs::skill_catalogue, {
        {catalogue, "Intro.\n" + begin + "\nstale\n# END generated catalogue"
            "\nOutro.\n"},
        {"doc/llm/skills/compass-pr-merge/SKILL.org",
            skill("P1", "Merge | safely")},
        {"doc/llm/skills/compass-agile-b/SKILL.org", skill("A2", "B.")},
        {"doc/llm/skills/compass-agile-a-x/SKILL.org", skill("A1",
            std::string(120, 'x'))},
        {"doc/llm/skills/compass-agile-a/SKILL.org", skill("A0", "A.")}});

    const auto& text = out.at(catalogue);
    CHECK(text.starts_with("Intro.\n" + begin + "\n\n* Agile"));
    CHECK(text.ends_with("# END generated catalogue\nOutro.\n"));
    const auto a0 = text.find("[[id:A0][compass-agile-a]]");
    const auto a1 = text.find("[[id:A1][compass-agile-a-x]] | A skill | " +
        std::string(107, 'x') + "... |");
    const auto a2 = text.find("[[id:A2][compass-agile-b]]");
    CHECK(a0 < a1);
    CHECK(a1 < a2);
    CHECK(a2 != std::string::npos);
    CHECK(text.find("| Merge / safely |") != std::string::npos);
}

TEST_CASE("a region is replaced once or everywhere, and missing markers "
    "give nothing", "[ops][regions]") {
    const std::string text = "a <b>1<e> c <b>2<e>";
    CHECK(ops::replace_region(text, "<b>", "<e>", "X", ops::replace::first) ==
        "a X c <b>2<e>");
    CHECK(ops::replace_region(text, "<b>", "<e>", "X", ops::replace::every) ==
        "a X c X");
    CHECK_FALSE(ops::replace_region("none", "<b>", "<e>", "X",
        ops::replace::first));
}

TEST_CASE("a catalogue under its script's marker is rewritten under the "
    "marker naming compass", "[codecs][skill_tables]") {
    const auto out = tests::generate(codecs::skill_catalogue, {
        {catalogue, "Intro.\n" + script_begin + "\nstale\n"
            "# END generated catalogue\nOutro.\n"},
        {"doc/llm/skills/compass-agile-a/SKILL.org", skill("A0", "A.")}});

    const auto& text = out.at(catalogue);
    CHECK(text.starts_with("Intro.\n" + begin + "\n\n* Agile"));
    CHECK(text.find("generate_skills_catalogue.py") == std::string::npos);
    CHECK(text.find("stale") == std::string::npos);
}

TEST_CASE("the type tables under their script's markers are rewritten "
    "under the markers naming compass", "[codecs][skill_tables]") {
    const std::string add = "doc/llm/skills/compass-doc-add/SKILL.org";
    const std::string show = "doc/llm/skills/compass-doc-show/SKILL.org";
    const std::string script = "(build/scripts/generate_skill_type_tables.py)";
    const auto out = tests::generate(codecs::skill_type_tables, {
        {"projects/ores.compass/src/compass.py", ""},
        {add, "A.\n# BEGIN generated types " + script + "\nstale\n"
            "# END generated types\nB.\n"},
        {show, "A.\n# BEGIN generated targets " + script + "\nstale\n"
            "# END generated targets\nB.\n"}});

    const auto& types = out.at(add);
    CHECK(types.starts_with("A.\n# BEGIN generated types (compass "
        "generate: skill_type_tables)\n\nOne row per document type."));
    CHECK(types.ends_with("\n# END generated types\nB.\n"));
    const auto& targets = out.at(show);
    CHECK(targets.starts_with("A.\n# BEGIN generated targets (compass "
        "generate: skill_type_tables)\n\nOne row per read-only command."));
    CHECK(targets.find("| =compass show= | show |") != std::string::npos);
    for (const auto& text : {types, targets}) {
        CHECK(text.find("generate_skill_type_tables.py") ==
            std::string::npos);
        CHECK(text.find("stale") == std::string::npos);
    }
}

TEST_CASE("a generated region is found under its marker or a former one, "
    "and written under its marker", "[ops][regions]") {
    constexpr std::array<std::string_view, 2> former{"<old>", "<older>"};
    const ops::generated_region r{"x", "a_codec", former};
    CHECK(ops::begin_marker(r) ==
        "# BEGIN generated x (compass generate: a_codec)");
    CHECK(ops::end_marker(r) == "# END generated x");

    const std::string text = "<older>1# END generated x <old>2# END "
        "generated x " + ops::begin_marker(r) + "3# END generated x";
    CHECK(ops::replace_region(text, r, "X", ops::replace::every) ==
        "X X X");
    CHECK(ops::replace_region(text, r, "X", ops::replace::first) ==
        "<older>1# END generated x <old>2# END generated x X");
    CHECK(ops::replace_region("<older>1# END generated x", r, "X",
        ops::replace::first) == "X");
    CHECK_FALSE(ops::replace_region("<old>1", r, "X",
        ops::replace::every));
}
