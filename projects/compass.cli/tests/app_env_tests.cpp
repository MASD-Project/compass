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

constexpr std::string_view registry =
    "#+title: Registry\n\n"
    "* web\n:PROPERTIES:\n:key: WEB\n:replicas: 1\n:END:\n\n"
    "* jobs\n:PROPERTIES:\n:key: JOBS\n:replicas: 1\n:END:\n";

std::string document(std::string_view extra_variable = {}) {
    return "#+title: Check: environment file format\n\n"
        "* Definition\n:PROPERTIES:\n:CODEC: env_format\n:END:\n\n"
        "* Settings\n\n"
        "| setting          | value         |\n"
        "|------------------+---------------|\n"
        "| version_variable | ACME_VERSION  |\n"
        "| service_registry | registry.org  |\n"
        "| baseline         | 1             |\n\n"
        "* Inputs\n\n"
        "| input | purpose               |\n"
        "|-------+-----------------------|\n"
        "| name  | The environment name. |\n\n"
        "* Versions\n\n"
        "| version | date       | adds     | removes | format only | description |\n"
        "|---------+------------+----------+---------+-------------+-------------|\n"
        "| 1       | 2026-01-01 |          |         |             | Initial.    |\n\n"
        "* Variables\n\n"
        "| variable              | kind    | argument             | added | removed | section | comment | when | services | purpose |\n"
        "|-----------------------+---------+----------------------+-------+---------+---------+---------+------+----------+---------|\n"
        "| =ACME_VERSION=        | version |                      | 1     |         | head    |         |      |          | V.      |\n"
        "| ACME_NAME             | value   | {name}               | 1     |         |         |         |      |          | N.      |\n"
        "| ACME_SECRET           | secret  |                      | 1     |         |         |         |      |          | S.      |\n"
        "| ACME_{service.key}_PW | secret  | 16                   | 1     |         |         |         |      |          | P.      |\n" +
        std::string(extra_variable) +
        "\n* Comments\n\n"
        "| comment | text              |\n"
        "|---------+-------------------|\n"
        "| head    | # Acme for {name} |\n";
}

struct outcome {
    int status = -1;
    std::string out;
    std::string err;
};

outcome run(const scoped_git_repo& repo, std::vector<std::string> args) {
    args.insert(args.begin(), {"-C", repo.checkout().generic_string()});
    const std::vector<std::string_view> views(args.begin(), args.end());
    std::istringstream in;
    std::ostringstream out;
    std::ostringstream err;
    const auto status = app::run(views, "/", in, out, err);
    return {status, out.str(), err.str()};
}

std::string value_of(const std::string& text, std::string_view name) {
    const auto at = text.find(std::string(name) + "=");
    REQUIRE(at != std::string::npos);
    const auto begin = at + name.size() + 1;
    return text.substr(begin, text.find('\n', begin) - begin);
}

void write_project(const scoped_git_repo& repo,
    std::string_view extra_variable = {}) {
    repo.write(".compass.org", configuration);
    repo.write("registry.org", registry);
    repo.write("compass/checks/env_format.org", document(extra_variable));
}

}

TEST_CASE("env generate writes the file, owner-only, with a secret per "
    "service, and keeps every secret when run again", "[app][env]") {
    const scoped_git_repo repo;
    write_project(repo);
    const auto first = run(repo, {"env", "generate", "--input", "name=dev"});
    REQUIRE(first.status == app::exit_success);
    CHECK_THAT(first.out, ContainsSubstring("version 1, 5 variables"));
    const auto path = repo.checkout() / ".env";
    const auto text = scoped_git_repo::read_file(path);
    CHECK(text.starts_with("# Acme for dev\nACME_VERSION=1\nACME_NAME=dev\n"));
    CHECK(value_of(text, "ACME_SECRET").size() == 32);
    CHECK(value_of(text, "ACME_WEB_PW").size() == 16);
    CHECK(value_of(text, "ACME_WEB_PW") != value_of(text, "ACME_JOBS_PW"));
#if !defined(_WIN32)
    namespace fs = std::filesystem;
    CHECK(fs::status(path).permissions() ==
        (fs::perms::owner_read | fs::perms::owner_write));
#endif

    const auto again = run(repo, {"env", "generate", "--input", "name=dev"});
    REQUIRE(again.status == app::exit_success);
    CHECK(scoped_git_repo::read_file(path) == text);
    CHECK_FALSE(std::filesystem::exists(repo.checkout() / ".env.old"));

    const auto changed = run(repo, {"env", "generate", "--input",
        "name=prod"});
    REQUIRE(changed.status == app::exit_success);
    CHECK(scoped_git_repo::read_file(repo.checkout() / ".env.old") == text);
    CHECK(value_of(scoped_git_repo::read_file(path), "ACME_SECRET") ==
        value_of(text, "ACME_SECRET"));
}

TEST_CASE("env generate fails without a declared input", "[app][env]") {
    const scoped_git_repo repo;
    write_project(repo);
    const auto r = run(repo, {"env", "generate"});
    CHECK(r.status == app::exit_failure);
    CHECK_THAT(r.err, ContainsSubstring("--input name="));
}

TEST_CASE("a row without a new version is a finding of check; version new "
    "clears it, and the regenerated file gains the variable",
    "[app][env]") {
    const scoped_git_repo repo;
    write_project(repo);
    REQUIRE(run(repo, {"env", "generate", "--input", "name=dev"}).status ==
        app::exit_success);
    CHECK(run(repo, {"env", "status"}).status == app::exit_success);

    write_project(repo, "| ACME_NEW | value | on | | | | | | | New. |\n");
    repo.git("add -A");
    const auto found = run(repo, {"check", "--codec", "env_format", "--today",
        "2026-11-21"});
    CHECK(found.status == app::exit_findings);
    CHECK_THAT(found.out, ContainsSubstring("env_format/unknown_version"));

    const auto stale = run(repo, {"env", "status"});
    CHECK(stale.status == app::exit_findings);
    CHECK_THAT(stale.out, ContainsSubstring("missing: ACME_NEW"));

    const auto recorded = run(repo, {"env", "version", "new", "Adds new.",
        "--adds", "ACME_NEW", "--today", "2026-11-21"});
    REQUIRE(recorded.status == app::exit_success);
    CHECK(run(repo, {"env", "version"}).out == "2\n");
    repo.git("add -A");
    CHECK(run(repo, {"check", "--codec", "env_format", "--today",
        "2026-11-21"}).status == app::exit_success);
    const auto doc = scoped_git_repo::read_file(repo.checkout() /
        "compass/checks/env_format.org");
    CHECK_THAT(doc, ContainsSubstring("| 2       | 2026-11-21 | ACME_NEW |"));

    const auto old = scoped_git_repo::read_file(repo.checkout() / ".env");
    REQUIRE(run(repo, {"env", "generate", "--input", "name=dev"}).status ==
        app::exit_success);
    const auto text = scoped_git_repo::read_file(repo.checkout() / ".env");
    CHECK_THAT(text, ContainsSubstring("ACME_VERSION=2\n"));
    CHECK_THAT(text, ContainsSubstring("ACME_NEW=on\n"));
    CHECK(value_of(text, "ACME_SECRET") == value_of(old, "ACME_SECRET"));
    CHECK(run(repo, {"env", "status"}).status == app::exit_success);
}

TEST_CASE("version new refuses a version that declares nothing",
    "[app][env]") {
    const scoped_git_repo repo;
    write_project(repo);
    const auto r = run(repo, {"env", "version", "new", "Nothing."});
    CHECK(r.status == app::exit_failure);
    CHECK_THAT(r.err, ContainsSubstring("--format-only"));
    CHECK(run(repo, {"env", "version", "new", "Moved.",
        "--format-only"}).status == app::exit_success);
    CHECK(run(repo, {"env", "version"}).out == "2\n");
}

TEST_CASE("env without the check document says which to write",
    "[app][env]") {
    const scoped_git_repo repo;
    const auto r = run(repo, {"env", "version"});
    CHECK(r.status == app::exit_failure);
    CHECK_THAT(r.err, ContainsSubstring("compass/checks/env_format.org"));
}
