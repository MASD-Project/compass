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
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "compass.codec/ops/data_tables.hpp"
#include "compass.docs/codecs/skill_tables.hpp"
#include "test_tree.hpp"

namespace {

using namespace compass::docs;
namespace cops = compass::codec::ops;
using Catch::Matchers::ContainsSubstring;

using rows = std::vector<std::vector<std::string>>;

const std::string document = "compass/checks/skill_levels.org";
const std::string page = "kb/levels.org";
const std::string begin = "# BEGIN generated inventory "
    "(compass generate: skill_levels)";
const std::string scaffold = "#+title: x\n#+export_exclude_tags: noexport\n";

std::string skill_path(std::string_view name) {
    return "kb/skills/" + std::string(name) + "/SKILL.org";
}

cops::data_table table(std::string name, std::uint32_t line,
    std::vector<std::string> columns, rows cells) {
    cops::data_table t{.name = std::move(name), .line = line,
        .columns = std::move(columns), .rows = std::move(cells),
        .row_lines = {}};
    for (std::size_t r = 0; r < t.rows.size(); ++r)
        t.row_lines.push_back(line + 2 + static_cast<std::uint32_t>(r));
    return t;
}

/**
 * @brief Acme's levels: work, plan, the binding audit level and the
 * everywhere level, five skills, and the read-only verbs view and list.
 */
rows acme_levels() {
    return {{"acme-ship-build", "work", "Builds."},
        {"acme-ship-view-log", "work", "Reads the log."},
        {"acme-plan-sprint", "plan", "Plans."},
        {"acme-plan-list-goals", "plan", "Reads the goals."},
        {"acme-doc-add", "everywhere", "Every level writes documents."}};
}

cops::data_tables acme_document(rows levels = acme_levels()) {
    cops::data_tables t{.source = document, .tables = {}};
    t.tables = {
        table("Levels", 10, {"skill", "level", "reason"}, std::move(levels)),
        table("Systems", 30, {"level", "system", "horizon", "attenuation"},
            {{"work", "Worker", "Hours", "advisory"},
             {"plan", "Planner", "Weeks", "advisory"},
             {"audit", "Auditor", "Always", "binding"},
             {"everywhere", "", "", "cross"}}),
        table("Read-only verbs", 40, {"verb"}, {{"view"}, {"list"}}),
        table("Settings", 50, {"setting", "value"},
            {{"skills_directory", "kb/skills"}, {"levels_page", page},
             {"inventory_summary", "What each System sees."},
             {"cross_summary", "the documents."}})};
    return t;
}

tests::options data(const cops::data_tables& t) {
    return {{std::string(cops::data_option), cops::encode_data_tables(t)}};
}

/**
 * @brief Acme's skills at the levels of its table, but for acme-ship-build,
 * which carries the wrong level, and acme-doc-add, which carries none, and
 * the levels page with a stale inventory.
 */
tests::files acme_tree() {
    tests::files all;
    for (const auto& row : acme_levels())
        all[skill_path(row[0])] = scaffold + "#+level: " + row[1] +
            "\n\n* Body\n";
    all[skill_path("acme-ship-build")] = scaffold + "#+level: plan\n\n* Body\n";
    all[skill_path("acme-doc-add")] = scaffold + "\n* Body\n";
    all[page] = "Intro.\n" + begin + "\nstale\n# END generated inventory\n"
        "\nOutro.\n";
    all[document] = "line\n";
    for (int i = 0; i < 20; ++i)
        all[document] += "line\n";
    return all;
}

}

TEST_CASE("the levels come from the check document and stamp each skill, "
    "replacing a level line or putting one after the scaffold's",
    "[codecs][skill_levels]") {
    const auto out = tests::generate(codecs::skill_levels, acme_tree(),
        data(acme_document()));

    CHECK(out.at(skill_path("acme-ship-build")) ==
        scaffold + "#+level: work\n\n* Body\n");
    CHECK(out.at(skill_path("acme-doc-add")) ==
        scaffold + "#+level: everywhere\n\n* Body\n");
    CHECK(out.at(skill_path("acme-plan-sprint")) ==
        scaffold + "#+level: plan\n\n* Body\n");
}

TEST_CASE("a second run writes what the first left", "[codecs][skill_levels]") {
    auto all = acme_tree();
    const auto settings = data(acme_document());
    const auto first = tests::generate(codecs::skill_levels, all, settings);
    for (const auto& [path, text] : first)
        all[path] = text;
    CHECK(tests::generate(codecs::skill_levels, all, settings) == first);
    CHECK(tests::run(codecs::skill_levels, all, settings).empty());
}

TEST_CASE("a skill whose level differs from its row is generated anew, "
    "which check reports as drift", "[codecs][skill_levels]") {
    auto levels = acme_levels();
    levels[2][1] = "work";
    const auto all = acme_tree();
    const auto out = tests::generate(codecs::skill_levels, all,
        data(acme_document(levels)));

    const auto path = skill_path("acme-plan-sprint");
    CHECK(all.at(path) == scaffold + "#+level: plan\n\n* Body\n");
    CHECK(out.at(path) == scaffold + "#+level: work\n\n* Body\n");
}

TEST_CASE("the inventory counts each System's slice and the cross skills",
    "[codecs][skill_levels]") {
    const auto out = tests::generate(codecs::skill_levels, acme_tree(),
        data(acme_document()));

    const auto& text = out.at(page);
    CHECK(text.starts_with("Intro.\n" + begin +
        "\n\nWhat each System sees.\n\n#+ATTR_HTML"));
    CHECK_THAT(text, ContainsSubstring("| =work= | Worker | Hours | 3 skills "
        "| advisory |\n| =plan= | Planner | Weeks | 3 skills | advisory |\n"
        "| =audit= | Auditor | Always | 2 skills | binding |\n\n"
        "1 skills are tagged =everywhere= and appear in every advisory slice: "
        "the documents.\n\n# END generated inventory\n\n"));
    CHECK(text.find("everywhere= |") == std::string::npos);
}

TEST_CASE("the slices list each level's skills, the binding level's by "
    "their read-only verbs, after the inventory", "[codecs][skill_levels]") {
    const auto out = tests::generate(codecs::skill_levels, acme_tree(),
        data(acme_document()));

    const auto& text = out.at(page);
    CHECK_THAT(text, ContainsSubstring("# END generated inventory\n\n"
        "# BEGIN generated slices (compass generate: skill_levels)\n"));
    CHECK_THAT(text, ContainsSubstring(
        "=work=, advisory: 3 of 5 skills.\n\n- acme-doc-add\n"
        "- acme-ship-build\n- acme-ship-view-log\n\n"
        "=plan=, advisory: 3 of 5 skills.\n\n- acme-doc-add\n"
        "- acme-plan-list-goals\n- acme-plan-sprint\n\n"
        "=audit=, binding: 2 of 5 skills.\n\n- acme-plan-list-goals\n"
        "- acme-ship-view-log\n\n"
        "=everywhere=, advisory: 1 of 5 skills.\n\n- acme-doc-add\n\n"
        "# END generated slices\n\nOutro.\n"));
}

TEST_CASE("slices the page already holds are replaced where they are",
    "[codecs][skill_levels]") {
    auto all = acme_tree();
    all[page] = "Intro.\n" + begin + "\nstale\n# END generated inventory\n"
        "\nMiddle.\n\n# BEGIN generated slices (compass generate: "
        "skill_levels)\nstale\n# END generated slices\n\nOutro.\n";
    const auto out = tests::generate(codecs::skill_levels, all,
        data(acme_document()));

    const auto& text = out.at(page);
    CHECK_THAT(text, ContainsSubstring("# END generated inventory\n\nMiddle."
        "\n\n# BEGIN generated slices"));
    CHECK(text.find("stale") == std::string::npos);
    CHECK(text.ends_with("# END generated slices\n\nOutro.\n"));
}

TEST_CASE("a skill with no row and a row with no skill are findings, and "
    "nothing is written", "[codecs][skill_levels]") {
    auto all = acme_tree();
    all[skill_path("acme-ship-deploy")] = scaffold;
    all.erase(skill_path("acme-plan-sprint"));
    const auto settings = data(acme_document());

    CHECK(tests::generate(codecs::skill_levels, all, settings).empty());
    const auto findings = tests::run(codecs::skill_levels, all, settings);
    CHECK(tests::places(findings) ==
        std::vector<std::pair<std::string, std::uint32_t>>{
            {skill_path("acme-ship-deploy"), 1}, {document, 14}});
    CHECK(tests::messages(findings) == std::vector<std::string>{
        "the skill acme-ship-deploy has no row in the table \"Levels\" of " +
            document,
        "the row names the skill acme-plan-sprint, which has no " +
            skill_path("acme-plan-sprint")});
}

TEST_CASE("a check document missing a table, a column or a setting is "
    "unreadable", "[codecs][skill_levels]") {
    const auto unreadable = [](const cops::data_tables& d) {
        const auto findings = tests::run(codecs::skill_levels, acme_tree(),
            data(d));
        REQUIRE(findings.size() == 1);
        CHECK(tests::generate(codecs::skill_levels, acme_tree(),
            data(d)).empty());
        return std::get<2>(findings[0]);
    };
    auto no_table = acme_document();
    no_table.tables.erase(no_table.tables.begin() + 2);
    CHECK_THAT(unreadable(no_table), ContainsSubstring("Read-only verbs"));

    auto no_column = acme_document();
    no_column.tables[0].columns[2] = "why";
    CHECK_THAT(unreadable(no_column), ContainsSubstring("reason"));

    auto no_setting = acme_document();
    no_setting.tables[3].rows.erase(no_setting.tables[3].rows.begin() + 1);
    CHECK_THAT(unreadable(no_setting), ContainsSubstring(
        "no row states the setting 'levels_page'"));

    auto unknown_level = acme_document();
    unknown_level.tables[0].rows[0][1] = "s9";
    CHECK_THAT(unreadable(unknown_level), ContainsSubstring(
        "the level 's9' of acme-ship-build is no row of the table "
        "\"Systems\""));

    const auto none = tests::run(codecs::skill_levels, acme_tree());
    REQUIRE(none.size() == 1);
    CHECK(std::get<0>(none[0]) == document);
}

TEST_CASE("an inventory under a script's marker is rewritten under the "
    "marker naming compass", "[codecs][skill_levels]") {
    auto all = acme_tree();
    all[page] = "Intro.\n# BEGIN generated inventory (build/scripts/"
        "apply_skill_levels.py)\nstale\n# END generated inventory\nOutro.\n";
    const auto out = tests::generate(codecs::skill_levels, all,
        data(acme_document()));

    const auto& text = out.at(page);
    CHECK(text.starts_with("Intro.\n" + begin + "\n\n"));
    CHECK(text.find("apply_skill_levels.py") == std::string::npos);
}

TEST_CASE("a skill with neither a level line nor the scaffold's line is "
    "refused", "[codecs][skill_levels]") {
    auto all = acme_tree();
    all[skill_path("acme-doc-add")] = "#+title: x\n";
    const auto settings = data(acme_document());
    CHECK_FALSE(tests::generate(codecs::skill_levels, all, settings).contains(
        skill_path("acme-doc-add")));
    CHECK(tests::messages(tests::run(codecs::skill_levels, all, settings)) ==
        std::vector<std::string>{skill_path("acme-doc-add") + " is not "
            "generated: the skill has no #+level: line, and no "
            "#+export_exclude_tags: noexport line to put one after"});
}

TEST_CASE("a skill page holding another generator's region is left to it, "
    "and a level it lacks is refused", "[codecs][skill_levels]") {
    const std::string region = "# BEGIN generated types (compass generate: "
        "other)\n# END generated types\n";
    auto all = acme_tree();
    const auto path = skill_path("acme-plan-sprint");
    all[path] = scaffold + "#+level: plan\n\n" + region;
    const auto settings = data(acme_document());
    CHECK_FALSE(tests::generate(codecs::skill_levels, all, settings)
        .contains(path));
    CHECK(tests::run(codecs::skill_levels, all, settings).empty());

    all[path] = scaffold + "#+level: work\n\n" + region;
    CHECK_FALSE(tests::generate(codecs::skill_levels, all, settings)
        .contains(path));
    CHECK(tests::messages(tests::run(codecs::skill_levels, all, settings)) ==
        std::vector<std::string>{path + " is not generated: the page holds "
            "the region \"# BEGIN generated types (compass generate: "
            "other)\", so its generator writes it, and its #+level: line "
            "does not say plan"});
}
