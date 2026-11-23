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
#include <regex>
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
    "#+output: out\n\n* Codecs\n\n"
    "| name | enabled | options |\n"
    "| document_index | no | |\n"
    "| required_sections | no | |\n"
    "| sql_hygiene | yes | roots=sql |\n";

constexpr std::string_view renderings_configuration =
    "#+output: .\n\n* Codecs\n\n"
    "| name | enabled | options |\n"
    "| document_index | no | |\n"
    "| required_sections | no | |\n"
    "| ore_renderings_tables | yes | |\n";

constexpr std::string_view table_a = "create table a (x int);\n";
constexpr std::string_view table_b = "create table b (x int);\n";
constexpr std::string_view table_c = "create table c (x int);\n";

void commit_all(const scoped_git_repo& repo) {
    repo.git("add -A");
    repo.git("-c user.name=t -c user.email=t@example.org "
             "-c commit.gpgsign=false commit -q -m c");
}

/**
 * @brief A repository whose commit, branch base, holds sql/a.sql with
 * two tables that cannot be created twice.
 */
void make_base(const scoped_git_repo& repo,
    std::string_view config = configuration) {
    repo.write(".compass.org", config);
    repo.write("sql/a.sql", std::string(table_a) + std::string(table_b));
    commit_all(repo);
    repo.git("branch base");
}

struct outcome {
    int status = -1;
    std::string out;
    std::string err;

    /**
     * @brief The lines of out that are findings, as path:line.
     */
    [[nodiscard]] std::vector<std::string> fresh() const {
        static const std::regex finding("^([^: ]+:[0-9]+):[0-9]+: .*");
        std::vector<std::string> lines;
        std::istringstream in(out);
        std::smatch m;
        for (std::string line; std::getline(in, line);) {
            if (std::regex_match(line, m, finding))
                lines.push_back(m[1]);
        }
        return lines;
    }
};

outcome ratchet(const scoped_git_repo& repo,
    std::vector<std::string> extra = {"--ratchet", "base"}) {
    std::vector<std::string> args{"check", "-C",
        repo.checkout().generic_string(), "-j", "2"};
    args.insert(args.end(), extra.begin(), extra.end());
    const std::vector<std::string_view> views(args.begin(), args.end());
    std::istringstream in;
    std::ostringstream out;
    std::ostringstream err;
    const auto status = app::run(views, "/", in, out, err);
    return {status, out.str(), err.str()};
}

}

TEST_CASE("ratchet fails only on the new violation of a file that already "
    "has some", "[app][ratchet]") {
    scoped_git_repo repo;
    make_base(repo);
    repo.write("sql/a.sql", std::string(table_a) + std::string(table_b) +
        std::string(table_c));
    commit_all(repo);

    const auto plain = ratchet(repo, {});
    CHECK(plain.fresh().size() == 3);

    const auto r = ratchet(repo);
    CHECK(r.status == 1);
    CHECK(r.fresh() == std::vector<std::string>{"sql/a.sql:3"});
    CHECK_THAT(r.out, ContainsSubstring("1 new diagnostic, 2 excused"));
    CHECK_THAT(r.out, ContainsSubstring("the merge base of HEAD and base"));
}

TEST_CASE("ratchet passes when nothing is new", "[app][ratchet]") {
    scoped_git_repo repo;
    make_base(repo);

    SECTION("a violation that moved down") {
        repo.write("sql/a.sql", "-- header\n\n" + std::string(table_a) +
            std::string(table_b));
        commit_all(repo);
    }
    SECTION("a violation that was removed") {
        repo.write("sql/a.sql", table_a);
        commit_all(repo);
    }
    const auto r = ratchet(repo);
    CHECK(r.status == 0);
    CHECK(r.fresh().empty());
    CHECK_THAT(r.out, ContainsSubstring("0 new diagnostics"));
}

TEST_CASE("ratchet reports a second copy of a violation",
    "[app][ratchet]") {
    scoped_git_repo repo;
    make_base(repo);
    repo.write("sql/a.sql", std::string(table_a) + std::string(table_b) +
        std::string(table_a));
    commit_all(repo);

    const auto r = ratchet(repo);
    CHECK(r.status == 1);
    CHECK(r.fresh() == std::vector<std::string>{"sql/a.sql:3"});
}

TEST_CASE("ratchet follows a renamed file", "[app][ratchet]") {
    scoped_git_repo repo;
    make_base(repo);

    SECTION("with the same content") {
        repo.git("mv sql/a.sql sql/b.sql");
        commit_all(repo);
        const auto r = ratchet(repo);
        CHECK(r.status == 0);
        CHECK_THAT(r.out, ContainsSubstring("1 renamed file followed"));
    }
    SECTION("with its content edited") {
        repo.git("mv sql/a.sql sql/b.sql");
        repo.write("sql/b.sql", "-- moved here\n" +
            std::string(table_a) + std::string(table_b));
        commit_all(repo);
        const auto r = ratchet(repo);
        CHECK(r.status == 0);
        CHECK_THAT(r.out, ContainsSubstring("1 renamed file followed"));
    }
    SECTION("and still reports a violation added to it") {
        repo.git("mv sql/a.sql sql/b.sql");
        commit_all(repo);
        repo.write("sql/b.sql", std::string(table_a) +
            std::string(table_b) + std::string(table_c));
        const auto r = ratchet(repo);
        CHECK(r.status == 1);
        CHECK(r.fresh() == std::vector<std::string>{"sql/b.sql:3"});
    }
}

TEST_CASE("ratchet checks the files on disk, uncommitted changes included",
    "[app][ratchet]") {
    scoped_git_repo repo;
    make_base(repo);
    repo.write("sql/new.sql", table_c);

    const auto r = ratchet(repo);
    CHECK(r.status == 1);
    CHECK(r.fresh() == std::vector<std::string>{"sql/new.sql:1"});
}

TEST_CASE("ratchet checks the merge base with the checkout's "
    "configuration", "[app][ratchet]") {
    scoped_git_repo repo;
    make_base(repo, "#+output: out\n\n* Codecs\n\n"
        "| name | enabled | options |\n"
        "| document_index | no | |\n"
        "| sql_hygiene | no | roots=sql |\n");
    repo.write(".compass.org", configuration);
    commit_all(repo);

    const auto r = ratchet(repo);
    CHECK(r.status == 0);
    CHECK_THAT(r.out, ContainsSubstring("0 new diagnostics, 2 excused"));
}

TEST_CASE("ratchet compares with the merge base, not the tip of the base",
    "[app][ratchet]") {
    scoped_git_repo repo;
    make_base(repo);
    repo.git("checkout -q -b work");
    repo.write("sql/w.sql", table_c);
    commit_all(repo);
    repo.git("checkout -q base");
    repo.write("sql/w.sql", "-- on base\n" + std::string(table_c));
    commit_all(repo);
    repo.git("checkout -q work");

    const auto r = ratchet(repo);
    CHECK(r.status == 1);
    CHECK(r.fresh() == std::vector<std::string>{"sql/w.sql:1"});
}

TEST_CASE("ratchet excuses drift only when neither side changed",
    "[app][ratchet]") {
    scoped_git_repo repo;
    repo.write(".compass.org", "#+output: out\n\n* Codecs\n\n"
        "| name | enabled | options |\n"
        "| required_sections | no | |\n");
    repo.write("out/doc/document_index.tsv", "stale\n");
    commit_all(repo);
    repo.git("branch base");

    SECTION("drift the branch did not touch") {
        repo.write("doc/notes.txt", "not a document\n");
        commit_all(repo);
        const auto r = ratchet(repo);
        CHECK(r.status == 0);
        CHECK_THAT(r.out, ContainsSubstring("0 new diagnostics, 1 excused"));
    }
    SECTION("what is generated changed") {
        repo.write("doc/task_c.org", "#+title: Task: c\n#+type: task\n");
        commit_all(repo);
        const auto r = ratchet(repo);
        CHECK(r.status == 1);
        CHECK(r.fresh() == std::vector<std::string>{
            "out/doc/document_index.tsv:1"});
    }
    SECTION("the drifted file changed") {
        repo.write("out/doc/document_index.tsv", "staler\n");
        commit_all(repo);
        const auto r = ratchet(repo);
        CHECK(r.status == 1);
        CHECK(r.fresh() == std::vector<std::string>{
            "out/doc/document_index.tsv:1"});
    }
}

TEST_CASE("ratchet judges the scripts a generated header names by the "
    "files of each side", "[app][ratchet]") {
    scoped_git_repo repo;
    const auto generate_and_commit = [&] {
        const std::string dir = repo.checkout().generic_string();
        const std::vector<std::string_view> args{"generate", "-C", dir,
            "-j", "2"};
        std::istringstream in;
        std::ostringstream out;
        std::ostringstream err;
        REQUIRE(app::run(args, "/", in, out, err) == 0);
        commit_all(repo);
    };
    const auto write_renderings = [&] {
        repo.write("projects/ores.sql/utility/ore_renderings.json",
            "{\"tables\": {\"a\": {\"A\": [\"A\"]}}}");
    };
    repo.write(".compass.org", renderings_configuration);

    SECTION("a missing script the base already named is inherited") {
        write_renderings();
        generate_and_commit();
        repo.git("branch base");
        repo.write("doc/notes.txt", "not a document\n");
        commit_all(repo);
        const auto r = ratchet(repo);
        CHECK(r.status == 0);
        CHECK(r.fresh().empty());
        CHECK_THAT(r.out, ContainsSubstring("0 new diagnostics, 1 excused"));
    }
    SECTION("a missing script the change names is new") {
        commit_all(repo);
        repo.git("branch base");
        write_renderings();
        generate_and_commit();
        const auto r = ratchet(repo);
        CHECK(r.status == 1);
        CHECK(r.fresh().size() == 1);
        CHECK_THAT(r.out, ContainsSubstring(
            "ore_renderings_tables/names_missing_file"));
    }
    SECTION("a named script the change deletes is new") {
        write_renderings();
        repo.write("compass.sh", "#!/bin/sh\n");
        generate_and_commit();
        repo.git("branch base");
        repo.git("rm -q compass.sh");
        commit_all(repo);
        const auto r = ratchet(repo);
        CHECK(r.status == 1);
        CHECK(r.fresh().size() == 1);
        CHECK_THAT(r.out, ContainsSubstring(
            "ore_renderings_tables/names_missing_file"));
    }
}

TEST_CASE("ratchet fails when the base cannot be resolved",
    "[app][ratchet]") {
    scoped_git_repo repo;
    make_base(repo);

    const auto named = ratchet(repo, {"--ratchet", "no-such-branch"});
    CHECK(named.status == 2);
    CHECK_THAT(named.err, ContainsSubstring("no commit is named "
        "no-such-branch"));

    const auto fallback = ratchet(repo, {"--ratchet"});
    CHECK(fallback.status == 2);
    CHECK_THAT(fallback.err, ContainsSubstring("no base to ratchet against"));

    repo.git("update-ref refs/remotes/origin/main base");
    const auto origin = ratchet(repo, {"--ratchet"});
    CHECK(origin.status == 0);
    CHECK_THAT(origin.out, ContainsSubstring("of HEAD and origin/main"));

    repo.git("checkout -q --orphan unrelated");
    repo.write("sql/a.sql", table_a);
    commit_all(repo);
    const auto unrelated = ratchet(repo);
    CHECK(unrelated.status == 2);
    CHECK_THAT(unrelated.err, ContainsSubstring("share no history"));
}

namespace {

/**
 * @brief Adds a codegen library the merge base lacks, with one component
 * model whose header the library generates, and turns codegen on.
 */
void add_library(const scoped_git_repo& repo) {
    repo.write(".compass.org", "#+output: .\n\n* Codecs\n\n"
        "| name | enabled | options |\n"
        "| document_index | no | |\n"
        "| required_sections | no | |\n"
        "| sql_hygiene | yes | roots=sql |\n"
        "| codegen | yes | library=lib product=ores "
        "convention_component=a year=2031 |\n");
    repo.write("lib/data/licence-GPL-v3.txt", "Licence.\n");
    repo.write("lib/data/modeline.org", "#+title: Modelines\n");
    repo.write("lib/component_catalogue.org",
        "* Components\n\n| name | modeling_dir | exclude_org_types |\n"
        "|------+--------------+-------------------|\n"
        "| a | projects/ores.a/modeling | |\n");
    repo.write("lib/templates/ores.cpp.component.org",
        "#+title: ores.cpp.component\n#+type: facet\n"
        "#+facet_group: ores.cpp\n#+model_types: component\n");
    repo.write("lib/templates/ores.cpp.component.header.org",
        "#+title: ores.cpp.component.header\n#+type: archetype\n"
        "#+facet: ores.cpp.component\n#+component_kind: flat\n"
        "#+output: projects/{component_dir}/a.hpp\n"
        "#+begin_src mustache :tangle header.mustache\n#+end_src\n");
    repo.write("lib/templates/header.mustache", "int {{component.name}};\n");
    repo.write("projects/ores.a/modeling/component_overview.org",
        "#+title: ores.a\n#+type: ores.codegen.component\n#+name: a\n"
        "#+full_name: ores.a\n#+brief: A\n#+component_kind: flat\n");
}

}

TEST_CASE("ratchet reports a codec the merge base cannot run and counts "
    "its findings at HEAD as new", "[app][ratchet]") {
    scoped_git_repo repo;
    make_base(repo);
    add_library(repo);

    SECTION("an output HEAD has not generated is new") {
        commit_all(repo);
        const auto r = ratchet(repo);
        CHECK(r.status == 1);
        CHECK(r.fresh() == std::vector<std::string>{
            "projects/ores.a/a.hpp:1"});
        CHECK_THAT(r.out, ContainsSubstring("codegen did not run at the "
            "merge base (codegen: no library holds "
            "data/licence-GPL-v3.txt), so its findings at HEAD are new"));
        CHECK_THAT(r.out, ContainsSubstring("1 new diagnostic, 2 excused"));
    }
    SECTION("HEAD with its outputs generated passes") {
        const std::string dir = repo.checkout().generic_string();
        const std::vector<std::string_view> args{"generate", "-C", dir,
            "-j", "2"};
        std::istringstream in;
        std::ostringstream out;
        std::ostringstream err;
        REQUIRE(app::run(args, "/", in, out, err) == 0);
        commit_all(repo);
        const auto r = ratchet(repo);
        CHECK(r.status == 0);
        CHECK_THAT(r.out, ContainsSubstring("codegen did not run at the "
            "merge base"));
        CHECK_THAT(r.out, ContainsSubstring("0 new diagnostics, 2 excused"));
    }
}
