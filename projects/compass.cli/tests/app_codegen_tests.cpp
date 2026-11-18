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
