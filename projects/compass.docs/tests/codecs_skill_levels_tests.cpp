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

#include <string>
#include <vector>
#include "compass.docs/codecs/skill_tables.hpp"
#include "test_tree.hpp"

namespace {

using namespace compass::docs;

const std::string levels_doc = "doc/llm/skills/skill_systems_levels.org";
const std::string begin = "# BEGIN generated inventory "
    "(compass generate: skill_levels)";

std::string skill_path(std::string_view name) {
    return "doc/llm/skills/" + std::string(name) + "/SKILL.org";
}

/**
 * @brief Every skill of the table at its level, and the levels page with a
 * stale inventory.
 */
tests::files full_tree() {
    tests::files all;
    for (const auto& [name, level] : codecs::skill_level_table())
        all[skill_path(name)] = "#+title: x\n#+export_exclude_tags: "
            "noexport\n#+level: " + std::string(level) + "\n\n* Body\n";
    all[levels_doc] = "Intro.\n" + begin + "\nstale\n"
        "# END generated inventory\nOutro.\n";
    return all;
}

}

TEST_CASE("the inventory counts each System's slice, the auditor's by its "
    "read-only verbs", "[codecs][skill_levels]") {
    const auto out = tests::generate(codecs::skill_levels, full_tree());

    const auto& text = out.at(levels_doc);
    CHECK(text.starts_with("Intro.\n" + begin + "\n\nWhat each System"));
    CHECK(text.ends_with("# END generated inventory\nOutro.\n"));
    CHECK(text.find("| =s1= | Agent | Hours | 64 skills | advisory |") !=
        std::string::npos);
    CHECK(text.find("| =s3star= | Auditor | Continuous | 5 skills | "
        "binding |") != std::string::npos);
    CHECK(text.find("\n15 skills are tagged =cross=") != std::string::npos);
}

TEST_CASE("an inventory under either script's marker is rewritten under "
    "the marker naming compass", "[codecs][skill_levels]") {
    for (const std::string script : {"build/scripts/apply_skill_levels.py",
             "build/scripts/generate_skills_catalogue.py"}) {
        auto all = full_tree();
        all[levels_doc] = "Intro.\n# BEGIN generated inventory (" + script +
            ")\nstale\n# END generated inventory\nOutro.\n";
        const auto out = tests::generate(codecs::skill_levels, all);

        const auto& text = out.at(levels_doc);
        CHECK(text.starts_with("Intro.\n" + begin + "\n\nWhat each System"));
        CHECK(text.find(script) == std::string::npos);
        CHECK(text.find("stale") == std::string::npos);
    }
}

TEST_CASE("a skill the table lacks and an entry with no skill write no "
    "inventory and are named in the refusal", "[codecs][skill_levels]") {
    auto all = full_tree();
    all[skill_path("compass-new-skill")] = "#+level: s1\n";
    all.erase(skill_path("compass-pr-merge"));
    CHECK(tests::generate(codecs::skill_levels, all).empty());
    CHECK(tests::messages(tests::run(codecs::skill_levels, all)) ==
        std::vector<std::string>{levels_doc + " is not generated: the level "
            "table and the skills differ: no level for compass-new-skill; "
            "no skill for compass-pr-merge"});
}
