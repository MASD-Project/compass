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
#include <filesystem>
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
    "| required_sections | no | |\n";

std::string check_document(std::string_view exceptions = {},
    std::string_view examples = {}) {
    return "#+title: Check: portable C++\n\n"
        "* Definition\n"
        ":PROPERTIES:\n"
        ":ENGINE: forbid\n"
        ":FILES: src/**/*.cpp\n"
        ":OUTSIDE: src/platform/**\n"
        ":END:\n\n"
        "** atomic_shared_ptr\n"
        ":PROPERTIES:\n"
        ":CPP_TEXT: \\batomic\\s*<\\s*(std\\s*::\\s*)?shared_ptr\\b\n"
        ":MESSAGE: std::atomic<std::shared_ptr> is not portable\n"
        ":END:\n\n"
        "Apple's libc++ lacks it.\n\n" + std::string(examples) +
        "* Exceptions\n\n"
        "| path | line | rule | expires | approver | reason |\n" +
        std::string(exceptions);
}

constexpr std::string_view violation =
    "std::atomic<std::shared_ptr<int>> current;\n";

void commit_all(const scoped_git_repo& repo) {
    repo.git("add -A");
    repo.git("-c user.name=t -c user.email=t@example.org "
             "-c commit.gpgsign=false commit -q -m c");
}

struct outcome {
    int status = -1;
    std::string out;
    std::string err;

    [[nodiscard]] std::vector<std::string> findings() const {
        static const std::regex finding(
            "^([^: ]+:[0-9]+):[0-9]+: ([^: ]+): .*");
        std::vector<std::string> lines;
        std::istringstream in(out);
        std::smatch m;
        for (std::string line; std::getline(in, line);) {
            if (std::regex_match(line, m, finding))
                lines.push_back(m[1].str() + ":" + m[2].str());
        }
        return lines;
    }
};

outcome check(const scoped_git_repo& repo,
    std::vector<std::string> extra = {}) {
    std::vector<std::string> args{"check", "-C",
        repo.checkout().generic_string(), "-j", "2", "--today",
        "2026-11-18"};
    args.insert(args.end(), extra.begin(), extra.end());
    const std::vector<std::string_view> views(args.begin(), args.end());
    std::istringstream in;
    std::ostringstream out;
    std::ostringstream err;
    const auto status = app::run(views, "/", in, out, err);
    return {status, out.str(), err.str()};
}

void write_checkout(const scoped_git_repo& repo) {
    repo.write(".compass.org", configuration);
    repo.write("compass/checks/portable_cpp.org", check_document());
    repo.write("src/core/cache.cpp", std::string("int a;\n") +
        std::string(violation));
    repo.write("src/platform/atomic.cpp", violation);
}

}

TEST_CASE("a check document runs as a codec over the files it covers",
    "[app][checks]") {
    scoped_git_repo repo;
    write_checkout(repo);

    const auto r = check(repo);
    CHECK(r.status == 1);
    CHECK(r.findings() == std::vector<std::string>{
        "src/core/cache.cpp:2:portable_cpp/atomic_shared_ptr"});
}

TEST_CASE("an exception in the check's own table excuses its finding",
    "[app][checks]") {
    scoped_git_repo repo;
    write_checkout(repo);
    repo.write("compass/checks/portable_cpp.org", check_document(
        "| src/core/cache.cpp | 2 | atomic_shared_ptr | 2027-01-31 | marco "
        "| guarded by a mutex until the platform wrapper lands |\n"));

    const auto r = check(repo);
    CHECK(r.status == 0);
    CHECK_THAT(r.out, ContainsSubstring("1 excused by exception"));
}

TEST_CASE("editing a check's prose runs nothing again; editing its "
    "definition runs it again", "[app][checks]") {
    scoped_git_repo repo;
    write_checkout(repo);
    CHECK(check(repo).status == 1);

    auto prose = check_document();
    prose.replace(prose.find("lacks it."), 9, "has none.");
    repo.write("compass/checks/portable_cpp.org", prose);
    const auto unchanged = check(repo);
    CHECK(unchanged.findings().size() == 1);
    CHECK_THAT(unchanged.out, ContainsSubstring(" portable_cpp 0"));

    auto definition = prose;
    definition.replace(definition.find(":OUTSIDE: src/platform/**"), 25,
        ":OUTSIDE: src/none/**");
    repo.write("compass/checks/portable_cpp.org", definition);
    const auto changed = check(repo);
    CHECK(changed.findings().size() == 2);
    CHECK_THAT(changed.out, ContainsSubstring(" portable_cpp 1"));
}

TEST_CASE("check of a commit with no check documents uses the working "
    "tree's", "[app][checks]") {
    scoped_git_repo repo;
    repo.write(".compass.org", configuration);
    repo.write("src/core/cache.cpp", violation);
    commit_all(repo);
    repo.write("compass/checks/portable_cpp.org", check_document());

    const auto r = check(repo, {"--commit", "HEAD"});
    CHECK(r.findings() == std::vector<std::string>{
        "src/core/cache.cpp:1:portable_cpp/atomic_shared_ptr"});
    CHECK_THAT(r.err, ContainsSubstring("has no check documents; using the "
        "working tree's"));
}

TEST_CASE("a check document that cannot run fails the command and says "
    "why", "[app][checks]") {
    scoped_git_repo repo;
    write_checkout(repo);
    auto broken = check_document();
    broken.replace(broken.find(":ENGINE: forbid"), 15, ":ENGINE: limit");
    repo.write("compass/checks/portable_cpp.org", broken);

    const auto r = check(repo);
    CHECK(r.status == 2);
    CHECK_THAT(r.err, ContainsSubstring("compass/checks/portable_cpp.org:5: "
        "the engine is 'limit'"));
}

TEST_CASE("an example that does not fire is reported on its block, which "
    "moves with the prose around it, and editing it runs the check again",
    "[app][checks]") {
    scoped_git_repo repo;
    write_checkout(repo);
    const std::string examples =
        "* Examples\n\n"
        "#+begin_src cpp :compass fires=atomic_shared_ptr :path src/a.cpp\n"
        "std::atomic<std::shared_ptr<int>> p;\n"
        "#+end_src\n\n"
        "#+begin_src cpp :compass quiet :path src/platform/a.cpp\n"
        "std::atomic<std::shared_ptr<int>> p;\n"
        "#+end_src\n\n"
        "#+begin_src cpp :compass fires=atomic_shared_ptr :path src/b.cpp\n"
        "std::shared_ptr<int> p;\n"
        "#+end_src\n\n";
    repo.write("compass/checks/portable_cpp.org", check_document({},
        examples));

    const auto r = check(repo);
    CHECK(r.findings() == std::vector<std::string>{
        "compass/checks/portable_cpp.org:28:portable_cpp/example",
        "src/core/cache.cpp:2:portable_cpp/atomic_shared_ptr"});

    auto moved = check_document({}, examples);
    moved.replace(moved.find("Apple's libc++ lacks it."), 24,
        "Apple's libc++\nlacks it.");
    repo.write("compass/checks/portable_cpp.org", moved);
    const auto shifted = check(repo);
    CHECK(shifted.findings() == std::vector<std::string>{
        "compass/checks/portable_cpp.org:29:portable_cpp/example",
        "src/core/cache.cpp:2:portable_cpp/atomic_shared_ptr"});

    auto fixed = examples;
    fixed.replace(fixed.find("std::shared_ptr<int> p;"), 23,
        "std::atomic<std::shared_ptr<int>> q;");
    repo.write("compass/checks/portable_cpp.org", check_document({}, fixed));
    const auto again = check(repo);
    CHECK(again.findings() == std::vector<std::string>{
        "src/core/cache.cpp:2:portable_cpp/atomic_shared_ptr"});
    CHECK_THAT(again.out, ContainsSubstring(" portable_cpp 1"));
}

TEST_CASE("check --codec runs the named checks and codecs and nothing else",
    "[app][checks]") {
    scoped_git_repo repo;
    write_checkout(repo);
    repo.write(".compass.org", std::string(configuration) +
        "| sql_hygiene | yes | roots=sql |\n");
    repo.write("sql/a.sql", "create table a (x int);\n");

    CHECK(check(repo).findings() == std::vector<std::string>{
        "sql/a.sql:1:sql_hygiene/not_re_runnable",
        "src/core/cache.cpp:2:portable_cpp/atomic_shared_ptr"});
    CHECK(check(repo, {"--codec", "portable_cpp"}).findings() ==
        std::vector<std::string>{
            "src/core/cache.cpp:2:portable_cpp/atomic_shared_ptr"});
    CHECK(check(repo, {"--codec", "sql_hygiene"}).findings() ==
        std::vector<std::string>{"sql/a.sql:1:sql_hygiene/not_re_runnable"});
    CHECK(check(repo, {"--codec", "lint", "--codec", "portable_cpp"})
        .findings() == std::vector<std::string>{
            "src/core/cache.cpp:2:portable_cpp/atomic_shared_ptr"});

    const auto unknown = check(repo, {"--codec", "portable_c"});
    CHECK(unknown.status == 2);
    CHECK_THAT(unknown.err, ContainsSubstring("--codec portable_c: no codec "
        "or check has that name"));
}

namespace {

/**
 * @brief A checkout with a format check over the C++ under src, outside
 * src/platform, run by a fake formatter that puts a line on top of each
 * file once, and three files: one it leaves alone, one it would change,
 * and one it would change that the check does not cover.
 */
void write_format_checkout(const scoped_git_repo& repo,
    std::string_view program = "bin/fake-format") {
    repo.write(".compass.org", configuration);
    repo.write("compass/checks/cpp_format.org",
        "#+title: Check: C++ formatting\n\n"
        "* Definition\n"
        ":PROPERTIES:\n"
        ":ENGINE: format\n"
        ":FORMATTER: clang-format\n"
        ":PROGRAM: " + std::string(program) + "\n"
        ":STYLE: style\n"
        ":FILES: src/**/*.cpp\n"
        ":OUTSIDE: src/platform/**\n"
        ":END:\n\n"
        "* Exceptions\n\n"
        "| path | line | rule | expires | approver | reason |\n");
    repo.write("style", "fake\n");
    repo.write("src/core/good.cpp", "// formatted\nint a;\n");
    repo.write("src/core/bad.cpp", "int b;\n");
    repo.write("src/platform/skipped.cpp", "int c;\n");
    const auto fake = repo.checkout() / "bin/fake-format";
    scoped_git_repo::write_file(fake, "#!/bin/sh\n"
        "if [ \"$1\" = --version ]; then echo fake 1; exit 0; fi\n"
        "for f in \"$@\"; do\n"
        "  case \"$f\" in -*) continue;; esac\n"
        "  if [ \"$(head -n 1 \"$f\")\" != '// formatted' ]; then\n"
        "    { echo '// formatted'; cat \"$f\"; } > \"$f.new\"\n"
        "    mv \"$f.new\" \"$f\"\n"
        "  fi\n"
        "done\n");
    std::filesystem::permissions(fake, std::filesystem::perms::owner_all);
}

int generate(const scoped_git_repo& repo) {
    const std::vector<std::string> args{"generate", "-C",
        repo.checkout().generic_string(), "-j", "2"};
    const std::vector<std::string_view> views(args.begin(), args.end());
    std::istringstream in;
    std::ostringstream out;
    std::ostringstream err;
    return app::run(views, "/", in, out, err);
}

}

TEST_CASE("a format check reports each file it covers that its formatter "
    "would change, and generate leaves the files as they are",
    "[app][checks][posix]") {
    scoped_git_repo repo;
    write_format_checkout(repo);

    const auto r = check(repo);
    CHECK(r.status == 1);
    CHECK(r.findings() == std::vector<std::string>{
        "src/core/bad.cpp:1:cpp_format/unformatted"});
    CHECK_THAT(r.out, ContainsSubstring("clang-format formats this file "
        "differently"));

    CHECK(generate(repo) == 0);
    CHECK(scoped_git_repo::read_file(repo.checkout() / "src/core/bad.cpp") == "int b;\n");

    repo.write("src/core/bad.cpp", "// formatted\nint b;\n");
    CHECK(check(repo).status == 0);
}

TEST_CASE("an exception in a format check's table excuses its file",
    "[app][checks][posix]") {
    scoped_git_repo repo;
    write_format_checkout(repo);
    const auto document = scoped_git_repo::read_file(
        repo.checkout() / "compass/checks/cpp_format.org");
    repo.write("compass/checks/cpp_format.org", document +
        "| src/core/bad.cpp | 1 | unformatted | 2027-01-31 | marco "
        "| vendored as it came |\n");

    const auto r = check(repo);
    CHECK(r.status == 0);
    CHECK_THAT(r.out, ContainsSubstring("1 excused by exception"));
}

TEST_CASE("a format check whose program is not found fails the command and "
    "names it", "[app][checks]") {
    scoped_git_repo repo;
    write_format_checkout(repo, "bin/no-such-format");

    const auto r = check(repo);
    CHECK(r.status == 2);
    CHECK_THAT(r.err, ContainsSubstring("none of bin/no-such-format is a "
        "program, for the formatter clang-format"));
}
