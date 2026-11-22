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

#include <chrono>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.platform/filesystem/file_status.hpp"
#include "scoped_git_repo.hpp"

namespace {

namespace fs = std::filesystem;
using compass::cli::tests::scoped_git_repo;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::StartsWith;

struct outcome {
    int status = -1;
    std::string out;
    std::string err;
};

outcome run(const scoped_git_repo& repo, std::vector<std::string> args) {
    args.insert(args.begin() + 1, {"-C", repo.checkout().generic_string()});
    const std::vector<std::string_view> views(args.begin(), args.end());
    std::istringstream in;
    std::ostringstream out;
    std::ostringstream err;
    const auto status = compass::cli::app::run(views, "/", in, out, err);
    return {status, out.str(), err.str()};
}

outcome generate(const scoped_git_repo& repo) {
    auto r = run(repo, {"generate", "-j", "2"});
    if (r.status != 0)
        UNSCOPED_INFO(r.out + r.err);
    return r;
}

/**
 * @brief The fake formatter both codegen and the format check run: it
 * puts a line on top of each file once.
 */
constexpr std::string_view fake_format = "#!/bin/sh\n"
    "if [ \"$1\" = --version ]; then echo fake 1; exit 0; fi\n"
    "for f in \"$@\"; do\n"
    "  case \"$f\" in -*) continue;; esac\n"
    "  if [ \"$(head -n 1 \"$f\")\" != '// formatted' ]; then\n"
    "    { echo '// formatted'; cat \"$f\"; } > \"$f.new\"\n"
    "    mv \"$f.new\" \"$f\"\n"
    "  fi\n"
    "done\n";

std::string archetype(std::string_view block) {
    return "#+title: ores.cpp.component.header\n#+type: archetype\n"
        "#+facet: ores.cpp.component\n#+component_kind: flat\n"
        "#+output: projects/{component_dir}/a.hpp\n"
        "#+begin_src mustache :tangle header.mustache\n" +
        std::string(block) + "#+end_src\n";
}

/**
 * @brief A checkout, its own output root, that runs every kind of
 * generator compass has: the document index, tangle, codegen from a model
 * of one component through a formatter, the rule table, the check
 * inventory, and a format check, with an env format beside them.
 */
void set_up(const scoped_git_repo& repo) {
    repo.write(".compass.org", "#+output: .\n\n* Codecs\n\n"
        "| name | enabled | options |\n"
        "| document_index | yes | |\n"
        "| required_sections | no | |\n"
        "| tangle | yes | |\n"
        "| codegen | yes | library=lib product=ores convention_component=a year=2031 "
        "clang_format=bin/fake-format clang_format_style=style |\n"
        "| rule_table | yes | path=doc/rules.org |\n"
        "| check_inventory | yes | id=A1D08435-9DCB-4B63-A33F-2E1A87DA62A2 "
        "created=2026-11-21 updated=2026-11-21 |\n");
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
        archetype("int {{component.name}};\n"));
    repo.write("lib/templates/header.mustache", "int {{component.name}};\n");
    repo.write("projects/ores.a/modeling/component_overview.org",
        "#+title: ores.a\n#+type: ores.codegen.component\n#+name: a\n"
        "#+full_name: ores.a\n#+brief: A\n#+component_kind: flat\n");
    repo.write("doc/meta/document_type_memory.org",
        "#+title: Document type: memory\n#+type: meta\n\n* Contract\n\n"
        "- Folder :: =doc/llm/memory/=\n- Level :: =cross=\n");
    repo.write("doc/llm/memory/no_numbers.org",
        ":PROPERTIES:\n:ID: 177EDB1C-6241-45AA-81B1-46C4B9D6329D\n:END:\n"
        "#+title: Do not number org headings\n#+type: memory\n");
    repo.write("compass/checks/cpp_format.org",
        ":PROPERTIES:\n:ID: 11111111-2222-3333-4444-555555555555\n:END:\n"
        "#+title: Check: C++ formatting\n\n"
        "* Definition\n:PROPERTIES:\n:ENGINE: format\n"
        ":FORMATTER: clang-format\n:PROGRAM: bin/fake-format\n"
        ":STYLE: style\n:FILES: src/**/*.cpp\n:END:\n\n"
        "* Exceptions\n\n"
        "| path | line | rule | expires | approver | reason |\n");
    repo.write("compass/checks/env_format.org",
        "#+title: Check: environment file format\n\n"
        "* Definition\n:PROPERTIES:\n:CODEC: env_format\n:END:\n\n"
        "* Settings\n\n"
        "| setting          | value        |\n"
        "|------------------+--------------|\n"
        "| version_variable | ACME_VERSION |\n"
        "| baseline         | 1            |\n\n"
        "* Inputs\n\n"
        "| input | purpose               |\n"
        "|-------+-----------------------|\n"
        "| name  | The environment name. |\n\n"
        "* Versions\n\n"
        "| version | date       | adds | removes | format only | description |\n"
        "|---------+------------+------+---------+-------------+-------------|\n"
        "| 1       | 2026-01-01 |      |         |             | Initial.    |\n\n"
        "* Variables\n\n"
        "| variable       | kind    | argument | added | removed | section | comment | when | services | purpose |\n"
        "|----------------+---------+----------+-------+---------+---------+---------+------+----------+---------|\n"
        "| =ACME_VERSION= | version |          | 1     |         | head    |         |      |          | V.      |\n"
        "| ACME_NAME      | value   | {name}   | 1     |         |         |         |      |          | N.      |\n"
        "| ACME_SECRET    | secret  |          | 1     |         |         |         |      |          | S.      |\n"
        "\n* Comments\n\n"
        "| comment | text                          |\n"
        "|---------+-------------------------------|\n"
        "| head    | # Acme for {name}, made {now} |\n");
    repo.write("src/core/good.cpp", "// formatted\nint a;\n");
    repo.write("src/core/bad.cpp", "int b;\n");
    repo.write("style", "fake\n");
    const auto program = repo.checkout() / "bin/fake-format";
    scoped_git_repo::write_file(program, fake_format);
    fs::permissions(program, fs::perms::owner_all);
    repo.git("add -A");
}

/**
 * @brief What a test compares of a file: its modification time, inode and
 * mode.
 */
struct stamp {
    std::int64_t mtime = 0;
    std::uint64_t inode = 0;
    fs::perms mode = fs::perms::none;

    bool operator==(const stamp&) const = default;
};

using stamps = std::map<std::string, stamp>;

/**
 * @brief Every regular file under the checkout outside .git, by its path
 * relative to it.
 */
stamps stamps_of(const scoped_git_repo& repo) {
    stamps out;
    const auto root = repo.checkout();
    for (auto it = fs::recursive_directory_iterator(root);
         it != fs::recursive_directory_iterator(); ++it) {
        if (it->path().filename() == ".git") {
            it.disable_recursion_pending();
            continue;
        }
        if (!it->is_regular_file() || it->is_symlink())
            continue;
        const auto st =
            compass::platform::filesystem::regular_file_status(it->path());
        REQUIRE(st);
        out[fs::relative(it->path(), root).generic_string()] = {
            st->modified.count_ns(), st->inode,
            fs::status(it->path()).permissions()};
    }
    return out;
}

/**
 * @brief Sets every file under the checkout outside .git to a fixed time
 * long past, so a file written later differs from it at any clock
 * resolution, and returns their stamps.
 */
stamps age_files(const scoped_git_repo& repo) {
    const auto old = fs::file_time_type::clock::now() -
        std::chrono::hours(24 * 365 * 10);
    for (const auto& [path, s] : stamps_of(repo))
        fs::last_write_time(repo.checkout() / path, old);
    return stamps_of(repo);
}

/**
 * @brief The paths whose stamp differs between @p before and @p after,
 * including those only one of them has.
 */
std::set<std::string> touched(const stamps& before, const stamps& after) {
    std::set<std::string> out;
    for (const auto& [path, s] : before) {
        const auto it = after.find(path);
        if (it == after.end() || it->second != s)
            out.insert(path);
    }
    for (const auto& [path, s] : after)
        if (!before.contains(path))
            out.insert(path);
    return out;
}

}

TEST_CASE("a generate that changes nothing touches no file, and one input "
    "edited touches only it and what it feeds", "[app][untouched][posix]") {
    scoped_git_repo repo;
    set_up(repo);
    const auto first = generate(repo);
    REQUIRE(first.status == 0);
    const auto made = stamps_of(repo);
    REQUIRE(made.contains("projects/ores.a/a.hpp"));
    REQUIRE(made.contains("lib/templates/header.mustache"));
    REQUIRE(made.contains("doc/rules.org"));
    REQUIRE(made.contains("doc/knowledge/compass/check_inventory.org"));
    REQUIRE(scoped_git_repo::read_file(repo.checkout() /
        "projects/ores.a/a.hpp") == "// formatted\nint a;\n");
    REQUIRE(scoped_git_repo::read_file(repo.checkout() /
        "src/core/bad.cpp") == "int b;\n");

    const auto before = age_files(repo);
    const auto again = generate(repo);
    REQUIRE(again.status == 0);
    CHECK(touched(before, stamps_of(repo)).empty());

    const auto settled = age_files(repo);
    repo.write("lib/templates/ores.cpp.component.header.org",
        archetype("long {{component.name}};\n"));
    const auto edited = stamps_of(repo);
    const auto changed = generate(repo);
    REQUIRE(changed.status == 0);
    CHECK(scoped_git_repo::read_file(repo.checkout() /
        "projects/ores.a/a.hpp") == "// formatted\nlong a;\n");
    CHECK(touched(settled, stamps_of(repo)) == std::set<std::string>{
        "lib/templates/ores.cpp.component.header.org",
        "lib/templates/header.mustache", "projects/ores.a/a.hpp"});
    CHECK(touched(edited, stamps_of(repo)) == std::set<std::string>{
        "lib/templates/header.mustache", "projects/ores.a/a.hpp"});
}

TEST_CASE("generate writes back a deleted output and touches nothing else",
    "[app][untouched][posix]") {
    scoped_git_repo repo;
    set_up(repo);
    REQUIRE(generate(repo).status == 0);
    const auto before = age_files(repo);
    const auto output = repo.checkout() / "projects/ores.a/a.hpp";
    fs::remove(output);

    const auto restored = generate(repo);

    REQUIRE(restored.status == 0);
    CHECK(scoped_git_repo::read_file(output) == "// formatted\nint a;\n");
    CHECK(touched(before, stamps_of(repo)) ==
        std::set<std::string>{"projects/ores.a/a.hpp"});
}

TEST_CASE("generate keeps the mode of an output it writes",
    "[app][untouched][posix]") {
    scoped_git_repo repo;
    set_up(repo);
    REQUIRE(generate(repo).status == 0);
    const auto output = repo.checkout() / "projects/ores.a/a.hpp";
    const auto mode = fs::perms::owner_read | fs::perms::owner_write |
        fs::perms::owner_exec | fs::perms::group_read;
    fs::permissions(output, mode);
    repo.write("lib/templates/ores.cpp.component.header.org",
        archetype("long {{component.name}};\n"));

    REQUIRE(generate(repo).status == 0);

    CHECK(scoped_git_repo::read_file(output) == "// formatted\nlong a;\n");
    CHECK(fs::status(output).permissions() == mode);
}

TEST_CASE("env generate run again touches neither the file nor its backup, "
    "though the time it was made moved on", "[app][untouched][env]") {
    scoped_git_repo repo;
    set_up(repo);
    const auto env = repo.checkout() / ".env";
    const auto backup = repo.checkout() / ".env.old";
    REQUIRE(run(repo, {"env", "generate", "--input", "name=dev"}).status ==
        0);
    const auto text = scoped_git_repo::read_file(env);
    REQUIRE_THAT(text, StartsWith("# Acme for dev, made "));
    REQUIRE_FALSE(fs::exists(backup));

    const auto before = age_files(repo);
    const auto again = run(repo, {"env", "generate", "--input", "name=dev"});
    REQUIRE(again.status == 0);
    CHECK_THAT(again.out, ContainsSubstring(".env unchanged"));
    CHECK(touched(before, stamps_of(repo)).empty());
    CHECK_FALSE(fs::exists(backup));

    const auto changed = run(repo, {"env", "generate", "--input",
        "name=prod"});
    REQUIRE(changed.status == 0);
    CHECK(touched(before, stamps_of(repo)) ==
        std::set<std::string>{".env", ".env.old"});
    CHECK(scoped_git_repo::read_file(backup) == text);
    CHECK_THAT(scoped_git_repo::read_file(env),
        StartsWith("# Acme for prod, made "));
}
