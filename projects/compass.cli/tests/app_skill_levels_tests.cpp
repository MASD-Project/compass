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
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.cli/app/application.hpp"
#include "scoped_git_repo.hpp"

namespace {

using namespace compass::cli;
using tests::scoped_git_repo;
using Catch::Matchers::ContainsSubstring;

constexpr std::string_view configuration =
    "#+output: .\n\n* Codecs\n\n"
    "| name | enabled | options |\n"
    "| document_index | no | |\n"
    "| required_sections | no | |\n"
    "| skill_levels | yes | |\n";

std::string document(std::string_view build_level) {
    return "#+title: Check: skill levels\n\n"
        "* Definition\n:PROPERTIES:\n:CODEC: skill_levels\n:END:\n\n"
        "* Levels\n\n"
        "| skill           | level | reason   |\n"
        "|-----------------+-------+----------|\n"
        "| acme-ship-build | " + std::string(build_level) + " | Builds.  |\n"
        "| acme-ship-view  | cross | Reads.   |\n\n"
        "* Systems\n\n"
        "| level | system  | horizon | attenuation |\n"
        "|-------+---------+---------+-------------|\n"
        "| =work= | Worker  | Hours   | advisory    |\n"
        "| =plan= | Planner | Weeks   | advisory    |\n"
        "| =audit= | Auditor | Always | binding     |\n"
        "| =cross= |        |         | cross       |\n\n"
        "* Read-only verbs\n\n"
        "| verb |\n|------|\n| view |\n\n"
        "* Settings\n\n"
        "| setting          | value         |\n"
        "|------------------+---------------|\n"
        "| skills_directory | skills        |\n"
        "| levels_page      | levels.org    |\n";
}

struct outcome {
    int status = -1;
    std::string out;
    std::string err;
};

outcome run(const scoped_git_repo& repo, std::vector<std::string> args) {
    args.insert(args.begin() + 1, {"-C", repo.checkout().generic_string(),
        "-j", "2"});
    const std::vector<std::string_view> views(args.begin(), args.end());
    std::istringstream in;
    std::ostringstream out;
    std::ostringstream err;
    const auto status = app::run(views, "/", in, out, err);
    return {status, out.str(), err.str()};
}

std::string skill(std::string_view level_line) {
    return "#+title: x\n#+export_exclude_tags: noexport\n" +
        std::string(level_line) + "\n* Body\n";
}

}

TEST_CASE("generate stamps the levels of the check document, check reports "
    "a level edited by hand as drift, and a changed row restamps the skill",
    "[app][skill_levels]") {
    const scoped_git_repo repo;
    repo.write(".compass.org", configuration);
    repo.write("compass/checks/skill_levels.org", document("work"));
    repo.write("skills/acme-ship-build/SKILL.org", skill(""));
    repo.write("skills/acme-ship-view/SKILL.org", skill("#+level: work"));
    repo.write("levels.org", "#+title: Levels\n\n# BEGIN generated inventory "
        "(compass generate: skill_levels)\n# END generated inventory\n");
    repo.git("add -A");

    const auto build = repo.checkout() / "skills/acme-ship-build/SKILL.org";
    const auto view = repo.checkout() / "skills/acme-ship-view/SKILL.org";
    const auto generated = run(repo, {"generate"});
    INFO(generated.out + generated.err);
    REQUIRE(generated.status == app::exit_success);
    CHECK(scoped_git_repo::read_file(build) == skill("#+level: work\n"));
    CHECK(scoped_git_repo::read_file(view) == skill("#+level: cross"));
    CHECK_THAT(scoped_git_repo::read_file(repo.checkout() / "levels.org"),
        ContainsSubstring("=audit=, binding: 1 of 2 skills.\n\n"
            "- acme-ship-view\n"));
    repo.git("add -A");
    CHECK(run(repo, {"check", "--codec", "skill_levels"}).status ==
        app::exit_success);

    repo.write("skills/acme-ship-view/SKILL.org", skill("#+level: plan"));
    repo.git("add -A");
    const auto drift = run(repo, {"check", "--codec", "skill_levels"});
    CHECK(drift.status == app::exit_findings);
    CHECK_THAT(drift.out, ContainsSubstring("skill_levels/drift"));
    CHECK_THAT(drift.out, ContainsSubstring("skills/acme-ship-view/SKILL.org"));

    repo.write("compass/checks/skill_levels.org", document("plan"));
    repo.git("add -A");
    REQUIRE(run(repo, {"generate"}).status == app::exit_success);
    CHECK(scoped_git_repo::read_file(build) == skill("#+level: plan\n"));
    CHECK(scoped_git_repo::read_file(view) == skill("#+level: cross"));
}
