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

#include <format>
#include <tuple>
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

outcome run(const scoped_git_repo& repo, const std::string& command,
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

void commit(const scoped_git_repo& repo, const std::string& message) {
    repo.git("add -A");
    repo.git("-c user.name=t -c user.email=t@example.com commit -q -m '" +
        message + "'");
}

constexpr std::string_view header = "lib/templates/header.hpp.mustache";
constexpr std::string_view source = "lib/templates/source.cpp.mustache";
constexpr std::string_view model_a =
    "projects/ores.a/modeling/component_overview.org";

/**
 * @brief A library of two archetypes, a header and a source, each
 * rendered for the two component models a and b, generated and
 * committed; then b's header is edited and committed, so HEAD holds a
 * file already out of date.
 */
void set_up(const scoped_git_repo& repo) {
    repo.write(".compass.org", "* Codecs\n"
        "| codegen | yes | library=lib convention_component=a year=2031 |\n");
    repo.write("lib/data/licence-GPL-v3.txt", "Licence.\n");
    repo.write("lib/data/modeline.org", "#+title: Modelines\n");
    repo.write("lib/component_catalogue.org",
        "* Components\n\n| name | modeling_dir | exclude_org_types |\n"
        "|------+--------------+-------------------|\n"
        "| a | projects/ores.a/modeling | |\n"
        "| b | projects/ores.b/modeling | |\n");
    repo.write("lib/templates/ores.cpp.component.org",
        "#+title: ores.cpp.component\n#+type: facet\n"
        "#+facet_group: ores.cpp\n#+model_types: component\n");
    for (const auto& [name, file, output] : {
             std::tuple{"header", "header.hpp", "x.hpp"},
             std::tuple{"source", "source.cpp", "x.cpp"}})
        repo.write(std::format("lib/templates/ores.cpp.component.{}.org",
            name), std::format("#+title: ores.cpp.component.{}\n"
            "#+type: archetype\n#+facet: ores.cpp.component\n"
            "#+component_kind: flat\n#+output: projects/{{component_dir}}/{}\n"
            "#+begin_src mustache :tangle {}.mustache\n#+end_src\n", name,
            output, file));
    repo.write(std::string(header),
        "{{{cpp_license}}}\nint {{component.name}};\n");
    repo.write(std::string(source),
        "{{{cpp_license}}}\nlong {{component.name}};\n");
    for (const auto* name : {"a", "b"})
        repo.write(std::format("projects/ores.{}/modeling/"
            "component_overview.org", name), std::format("#+title: ores.{0}\n"
            "#+type: ores.codegen.component\n#+name: {0}\n"
            "#+full_name: ores.{0}\n#+brief: {0}\n#+component_kind: flat\n",
            name));
    REQUIRE(run(repo, "gen").status == 0);
    commit(repo, "generate");
    const auto drifted = repo.checkout() / "projects/ores.b/x.hpp";
    scoped_git_repo::write_file(drifted,
        scoped_git_repo::read_file(drifted) + "// by hand\n");
    commit(repo, "edit a generated file");
}

void edit_source(const scoped_git_repo& repo) {
    repo.write(std::string(source),
        "{{{cpp_license}}}\nshort {{component.name}};\n");
}

}

TEST_CASE("diff names the codec, model and template of each file and keeps "
    "the drift HEAD already had apart", "[app][attribution][posix]") {
    scoped_git_repo repo;
    set_up(repo);
    edit_source(repo);

    const auto r = run(repo, "diff");

    CHECK(r.status == 1);
    CHECK_THAT(r.err, ContainsSubstring(std::format(
        "from your change: 2 files\n"
        "  write projects/ores.a/x.cpp (codegen, {}, {})\n"
        "  write projects/ores.b/x.cpp (codegen, "
        "projects/ores.b/modeling/component_overview.org, {})\n",
        model_a, source, source)));
    CHECK_THAT(r.err, ContainsSubstring("already stale at HEAD, as nothing "
        "changed since HEAD caused them; generate writes them too; ") &&
        ContainsSubstring(std::format("1 file\n  restore projects/ores.b/x.hpp "
            "(codegen, projects/ores.b/modeling/component_overview.org, "
            "{})\n", header)));
    CHECK_THAT(r.err, !ContainsSubstring("hand edit: commit"));
}

TEST_CASE("with no change since HEAD every change is drift HEAD already had",
    "[app][attribution][posix]") {
    scoped_git_repo repo;
    set_up(repo);

    const auto r = run(repo, "diff");

    CHECK(r.status == 1);
    CHECK_THAT(r.err, !ContainsSubstring("from your change:") &&
        ContainsSubstring("already stale at HEAD") &&
        ContainsSubstring("  restore projects/ores.b/x.hpp "));
}

TEST_CASE("a file whose input is only an untracked file is from the change",
    "[app][attribution]") {
    scoped_git_repo repo;
    repo.write(".compass.org", "* Codecs\n| tangle | yes | |\n");
    repo.write("doc/old.org", "#+title: Old\n"
        "#+begin_src text :tangle old.txt\nold\n#+end_src\n");
    REQUIRE(run(repo, "gen").status == 0);
    repo.write("doc/old.txt", "stale\n");
    commit(repo, "base");
    repo.write("doc/new.org", "#+title: New\n"
        "#+begin_src text :tangle new.txt\nnew\n#+end_src\n");

    const auto r = run(repo, "diff", {"--blame"});

    CHECK(r.status == 1);
    CHECK_THAT(r.err, ContainsSubstring("from your change: 2 files\n"
        "  write doc/document_index.tsv (document_index)\n"
        "  write doc/new.txt (tangle, doc/new.org)\n"));
    CHECK_THAT(r.err, ContainsSubstring("1 file\n"
        "  restore doc/old.txt (tangle, doc/old.org)\n"
        "      hand edit: commit "));
}

TEST_CASE("generate --skip-stale writes the change and leaves the drift",
    "[app][attribution][posix]") {
    scoped_git_repo repo;
    set_up(repo);
    edit_source(repo);
    const auto drifted = repo.checkout() / "projects/ores.b/x.hpp";
    const auto before = scoped_git_repo::read_file(drifted);

    const auto r = run(repo, "gen", {"--skip-stale"});

    CHECK(r.status == 0);
    CHECK_THAT(r.out, StartsWith("2 written, "));
    CHECK_THAT(scoped_git_repo::read_file(repo.checkout() /
        "projects/ores.a/x.cpp"), ContainsSubstring("short a;"));
    CHECK(scoped_git_repo::read_file(drifted) == before);
    CHECK_THAT(r.out, ContainsSubstring("left unwritten (--skip-stale)") &&
        ContainsSubstring("  write projects/ores.b/x.hpp (codegen, "));

    const auto plain = run(repo, "gen");

    CHECK_THAT(plain.out, StartsWith("0 written, 4 unchanged, 1 restored") &&
        ContainsSubstring("generate wrote them; "));
    CHECK(scoped_git_repo::read_file(drifted) != before);
}

TEST_CASE("a scope on a template takes only the files rendered from it",
    "[app][attribution][posix]") {
    scoped_git_repo repo;
    set_up(repo);
    edit_source(repo);

    const auto r = run(repo, "diff", {"--scope", std::string(source)});

    CHECK(r.status == 1);
    CHECK_THAT(r.err, StartsWith("2 to write, "));
    CHECK_THAT(r.err, ContainsSubstring(std::format("scope: the outputs of {} "
        "and what they feed\n", source)));
    CHECK_THAT(r.out, ContainsSubstring("projects/ores.a/x.cpp") &&
        ContainsSubstring("projects/ores.b/x.cpp") &&
        !ContainsSubstring("projects/ores.b/x.hpp"));

    const auto generated = run(repo, "gen", {"--scope",
        (repo.checkout() / source).generic_string()});

    CHECK_THAT(generated.out, StartsWith("2 written, "));
    CHECK_THAT(run(repo, "diff").err, StartsWith("0 to write, 4 unchanged, "
        "1 to restore, "));
}

TEST_CASE("a scope on a model takes only the files of that model",
    "[app][attribution][posix]") {
    scoped_git_repo repo;
    set_up(repo);
    edit_source(repo);

    const auto r = run(repo, "diff", {"--scope", std::string(model_a)});

    CHECK(r.status == 1);
    CHECK_THAT(r.err, StartsWith("1 to write, "));
    CHECK_THAT(r.out, ContainsSubstring("projects/ores.a/x.cpp") &&
        !ContainsSubstring("projects/ores.b/"));
}

TEST_CASE("a scope follows what the files it writes feed, pass by pass",
    "[app][attribution][converge][posix]") {
    scoped_git_repo repo;
    set_up(repo);
    const auto archetype = [&](std::string_view body) {
        repo.write("lib/templates/ores.cpp.component.source.org",
            "#+title: ores.cpp.component.source\n#+type: archetype\n"
            "#+facet: ores.cpp.component\n#+component_kind: flat\n"
            "#+output: projects/{component_dir}/x.cpp\n"
            "#+begin_src mustache :tangle source.cpp.mustache\n"
            "{{{cpp_license}}}\n" + std::string(body) + "#+end_src\n");
    };
    repo.write(".compass.org", "* Codecs\n"
        "| codegen | yes | library=lib convention_component=a year=2031 |\n"
        "| tangle | yes | |\n");
    archetype("long {{component.name}};\n");
    repo.write("lib/templates/ores.cpp.component.header.org",
        "#+title: ores.cpp.component.header\n#+type: archetype\n"
        "#+facet: ores.cpp.component\n#+component_kind: flat\n"
        "#+output: projects/{component_dir}/x.hpp\n"
        "#+begin_src mustache :tangle header.hpp.mustache\n"
        "{{{cpp_license}}}\nint {{component.name}};\n#+end_src\n");
    repo.write("doc/c.org", "#+title: C\n"
        "#+begin_src text :tangle c.txt\nc\n#+end_src\n");
    REQUIRE(run(repo, "gen").status == 0);
    commit(repo, "tangle");
    archetype("short {{component.name}};\n");
    repo.write("doc/c.org", "#+title: C\n"
        "#+begin_src text :tangle c.txt\nC\n#+end_src\n");

    const auto r = run(repo, "gen", {"--scope",
        "lib/templates/ores.cpp.component.source.org"});

    CHECK(r.status == 0);
    CHECK_THAT(r.out, StartsWith("3 written, ") &&
        ContainsSubstring(" in 2 passes "));
    CHECK_THAT(scoped_git_repo::read_file(repo.checkout() /
        "projects/ores.b/x.cpp"), ContainsSubstring("short b;"));
    CHECK(scoped_git_repo::read_file(repo.checkout() / "doc/c.txt") ==
        "c\n");
}

TEST_CASE("many changed files are counted by what produced them, and "
    "--files lists each", "[app][attribution]") {
    scoped_git_repo repo;
    repo.write(".compass.org", "* Codecs\n| tangle | yes | |\n"
        "| document_index | no | |\n");
    commit(repo, "base");
    std::string many = "#+title: Many\n";
    for (int i = 0; i < 25; ++i)
        many += std::format("#+begin_src text :tangle f{}.txt\n{}\n"
            "#+end_src\n", i, i);
    repo.write("doc/many.org", many);

    const auto counted = run(repo, "diff");

    CHECK_THAT(counted.err, ContainsSubstring("from your change: 25 files\n"
        "  25 files: tangle, doc/many.org\n  (--files lists each file)\n"));

    const auto listed = run(repo, "diff", {"--files"});

    CHECK_THAT(listed.err, ContainsSubstring("from your change: 25 files\n"
        "  write doc/f0.txt (tangle, doc/many.org)\n") &&
        ContainsSubstring("  write doc/f9.txt (tangle, doc/many.org)\n"));
}

TEST_CASE("the scope, skip-stale and files options belong to generate "
    "and diff",
    "[app][attribution]") {
    scoped_git_repo repo;

    const auto r = run(repo, "check", {"--skip-stale"});

    CHECK(r.status == 2);
    CHECK_THAT(r.err, ContainsSubstring("--skip-stale applies to generate "
        "and diff only"));
}
