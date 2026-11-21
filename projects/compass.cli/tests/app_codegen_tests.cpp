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
#include "compass.cli/app/application.hpp"

#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "scoped_git_repo.hpp"

namespace {

using compass::cli::tests::scoped_git_repo;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::StartsWith;

struct outcome {
    int status = -1;
    std::string out;
    std::string err;
};

outcome run_on(const scoped_git_repo& repo, const std::string& command) {
    const std::vector<std::string> args{command, "-C",
        repo.checkout().generic_string(), "--out",
        repo.output_root().generic_string(), "-j", "2", "--timings"};
    const std::vector<std::string_view> views(args.begin(), args.end());
    std::istringstream in;
    std::ostringstream out;
    std::ostringstream err;
    const auto status = compass::cli::app::run(views, "/", in, out, err);
    return {status, out.str(), err.str()};
}

/**
 * @brief A checkout with a library of one archetype that writes a header
 * per component model, one component model, a source file nobody reads,
 * and a fake clang-format that puts a line on top of each file once.
 */
void set_up(const scoped_git_repo& repo) {
    repo.write(".compass.org", "* Codecs\n"
        "| codegen | yes | library=lib convention_component=a year=2031 "
        "clang_format=bin/fake-format clang_format_style=style |\n");
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
    repo.write("lib/templates/header.mustache",
        "int {{component.name}};\n");
    repo.write("projects/ores.a/modeling/component_overview.org",
        "#+title: ores.a\n#+type: ores.codegen.component\n#+name: a\n"
        "#+full_name: ores.a\n#+brief: A\n#+component_kind: flat\n");
    repo.write("src/main.cpp", "int main() {}\n");
    repo.write("style", "fake\n");
    const auto program = repo.checkout() / "bin/fake-format";
    scoped_git_repo::write_file(program, "#!/bin/sh\n"
        "if [ \"$1\" = --version ]; then echo fake 1; exit 0; fi\n"
        "for f in \"$@\"; do\n"
        "  case \"$f\" in -*) continue;; esac\n"
        "  if [ \"$(head -n 1 \"$f\")\" != '// formatted' ]; then\n"
        "    { echo '// formatted'; cat \"$f\"; } > \"$f.new\"\n"
        "    mv \"$f.new\" \"$f\"\n"
        "  fi\n"
        "done\n");
    std::filesystem::permissions(program, std::filesystem::perms::owner_all);
    repo.git("add -A");
}

std::filesystem::path header(const scoped_git_repo& repo) {
    return repo.output_root() / "projects/ores.a/a.hpp";
}

}

TEST_CASE("generate writes the codegen outputs as the configured formatter "
    "leaves them", "[app][codegen][posix]") {
    scoped_git_repo repo;
    set_up(repo);

    const auto r = run_on(repo, "gen");

    REQUIRE(r.status == 0);
    CHECK(scoped_git_repo::read_file(header(repo)) ==
        "// formatted\nint a;\n");
    CHECK_THAT(r.err, ContainsSubstring("compass: codec codegen: 2 run, 0 "
        "from the record, 0 from the cache; format 0 stored, 1 formatted"));
}

TEST_CASE("a second generate reuses the codegen units and its formatted "
    "output, and an unread file changes nothing", "[app][codegen][posix]") {
    scoped_git_repo repo;
    set_up(repo);
    REQUIRE(run_on(repo, "gen").status == 0);
    repo.write("src/main.cpp", "int main() { return 0; }\n");
    repo.write("doc/task_b.org", "#+title: Task: b\n#+type: task\n* Goal\n"
        "More.\n");

    const auto again = run_on(repo, "gen");

    CHECK(again.status == 0);
    CHECK_THAT(again.out, StartsWith("0 written, "));
    CHECK_THAT(again.err, ContainsSubstring("compass: codec codegen: 0 run, "
        "2 from the record, 0 from the cache; format 1 stored, 0 formatted"));
}

TEST_CASE("editing a template runs codegen again and diff shows the "
    "formatted change", "[app][codegen][posix]") {
    scoped_git_repo repo;
    set_up(repo);
    REQUIRE(run_on(repo, "gen").status == 0);
    repo.write("lib/templates/header.mustache", "long {{component.name}};\n");

    const auto diffed = run_on(repo, "diff");

    CHECK(diffed.status == 1);
    CHECK_THAT(diffed.out, ContainsSubstring("-int a;\n+long a;\n"));
    CHECK_THAT(diffed.err, ContainsSubstring("compass: codec codegen: 2 run"));
}

TEST_CASE("a codegen formatter that cannot be found fails the command",
    "[app][codegen]") {
    scoped_git_repo repo;
    set_up(repo);
    repo.write(".compass.org", "* Codecs\n"
        "| codegen | yes | library=lib convention_component=a year=2031 "
        "clang_format=bin/missing clang_format_style=style |\n");

    const auto r = run_on(repo, "gen");

    CHECK(r.status == 2);
    CHECK_THAT(r.err, ContainsSubstring("bin/missing"));
    CHECK_FALSE(std::filesystem::exists(header(repo)));
}

namespace {

outcome run_in_place(const scoped_git_repo& repo, const std::string& command,
    const std::vector<std::string>& more = {}) {
    std::vector<std::string> args{command, "-C",
        repo.checkout().generic_string(), "--out",
        repo.checkout().generic_string(), "-j", "2"};
    args.insert(args.end(), more.begin(), more.end());
    const std::vector<std::string_view> views(args.begin(), args.end());
    std::istringstream in;
    std::ostringstream out;
    std::ostringstream err;
    const auto status = compass::cli::app::run(views, "/", in, out, err);
    return {status, out.str(), err.str()};
}

/**
 * @brief The library of set_up with tangle on and the checkout as the
 * output root, so the template codegen renders is the one tangle writes
 * from @p block, the body of the archetype's mustache block.
 */
void set_up_tangled(const scoped_git_repo& repo, std::string_view block) {
    set_up(repo);
    repo.write(".compass.org", "* Codecs\n"
        "| codegen | yes | library=lib convention_component=a year=2031 |\n"
        "| tangle | yes | |\n");
    repo.write("lib/templates/ores.cpp.component.header.org",
        "#+title: ores.cpp.component.header\n#+type: archetype\n"
        "#+facet: ores.cpp.component\n#+component_kind: flat\n"
        "#+output: projects/{component_dir}/a.hpp\n"
        "#+begin_src mustache :tangle header.mustache\n" +
        std::string(block) + "#+end_src\n");
    repo.git("add -A");
}

std::filesystem::path header_in_place(const scoped_git_repo& repo) {
    return repo.checkout() / "projects/ores.a/a.hpp";
}

}

TEST_CASE("one generate renders with the template it tangles, so check "
    "finds no drift and a second generate writes nothing",
    "[app][codegen][converge]") {
    scoped_git_repo repo;
    set_up_tangled(repo, "int {{component.name}};\n");
    REQUIRE(run_in_place(repo, "gen").status == 0);
    REQUIRE(scoped_git_repo::read_file(header_in_place(repo)) == "int a;\n");
    set_up_tangled(repo, "long {{component.name}};\n");

    const auto generated = run_in_place(repo, "gen");

    REQUIRE(generated.status == 0);
    CHECK(scoped_git_repo::read_file(repo.checkout() /
        "lib/templates/header.mustache") == "long {{component.name}};\n");
    CHECK(scoped_git_repo::read_file(header_in_place(repo)) == "long a;\n");
    CHECK_THAT(generated.out, StartsWith("2 written, 1 unchanged, ") &&
        ContainsSubstring(" in 2 passes "));
    const auto checked = run_in_place(repo, "check", {"--codec", "codegen",
        "--codec", "tangle"});
    CHECK_THAT(checked.out, !ContainsSubstring("drift"));
    CHECK(checked.status == 0);
    const auto again = run_in_place(repo, "gen");
    CHECK(again.status == 0);
    CHECK_THAT(again.out, StartsWith("0 written, 3 unchanged, ") &&
        !ContainsSubstring("passes"));
}

TEST_CASE("diff shows what one generate writes when an output is a "
    "template another codec renders", "[app][codegen][converge]") {
    scoped_git_repo repo;
    set_up_tangled(repo, "int {{component.name}};\n");
    REQUIRE(run_in_place(repo, "gen").status == 0);
    set_up_tangled(repo, "long {{component.name}};\n");

    const auto diffed = run_in_place(repo, "diff");

    CHECK(diffed.status == 1);
    CHECK_THAT(diffed.out, ContainsSubstring("-int a;\n+long a;\n") &&
        ContainsSubstring("-int {{component.name}};\n"
                          "+long {{component.name}};\n"));
    CHECK_THAT(diffed.err, StartsWith("2 to write, 1 unchanged, ") &&
        ContainsSubstring(" in 2 passes "));
    CHECK(scoped_git_repo::read_file(header_in_place(repo)) == "int a;\n");
}

TEST_CASE("generate stops with a problem when an output feeds its own "
    "input without end", "[app][codegen][converge]") {
    scoped_git_repo repo;
    set_up(repo);
    repo.write(".compass.org", "* Codecs\n"
        "| codegen | yes | library=lib convention_component=a year=2031 |\n");
    repo.write("lib/templates/ores.cpp.component.header.org",
        "#+title: ores.cpp.component.header\n#+type: archetype\n"
        "#+facet: ores.cpp.component\n#+component_kind: flat\n"
        "#+output: lib/templates/header.mustache\n"
        "#+begin_src mustache :tangle header.mustache\n#+end_src\n");
    repo.write("lib/templates/header.mustache", "{{{component.brief}}}\n");
    repo.write("projects/ores.a/modeling/component_overview.org",
        "#+title: ores.a\n#+type: ores.codegen.component\n#+name: a\n"
        "#+full_name: ores.a\n#+brief: x{{{component.brief}}}\n"
        "#+component_kind: flat\n");
    repo.git("add -A");

    const auto generated = run_in_place(repo, "gen");

    CHECK(generated.status == 2);
    CHECK_THAT(generated.err, ContainsSubstring("did not settle in 4 passes")
        && ContainsSubstring("lib/templates/header.mustache"));
    CHECK_THAT(generated.out, ContainsSubstring(" in 4 passes "));
}
