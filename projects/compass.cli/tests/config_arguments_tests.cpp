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
#include "compass.cli/config/arguments.hpp"

#include <chrono>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace compass::cli;

config::parsed_arguments parse(std::vector<std::string_view> args) {
    return config::parse_arguments(args, "/work/repo", 8);
}

}

TEST_CASE("a bare command runs over the working directory and sets "
    "nothing", "[config][arguments]") {
    const auto parsed = parse({"gen"});
    REQUIRE(std::holds_alternative<types::invocation>(parsed));
    const auto& o = std::get<types::invocation>(parsed);
    CHECK(o.which == types::command::generate);
    CHECK(o.checkout == "/work/repo");
    CHECK_FALSE(o.command_line.meta);
    CHECK_FALSE(o.command_line.output);
    CHECK(o.threads == 8);
    CHECK(o.command_line.settings.empty());
}

TEST_CASE("The -C option is relative to the working directory; --meta, --out and "
    "--set are kept as written", "[config][arguments]") {
    const auto parsed = parse({"-C", "../other/", "diff", "--meta", "meta",
        "--out", "/tmp/out", "-j", "3", "--set", "document_index.path=x.tsv",
        "--set", "required_sections.enabled=false"});
    REQUIRE(std::holds_alternative<types::invocation>(parsed));
    const auto& o = std::get<types::invocation>(parsed);
    CHECK(o.which == types::command::diff);
    CHECK(o.checkout == "/work/other");
    REQUIRE(o.command_line.meta);
    CHECK(o.command_line.meta->value == "meta");
    CHECK(o.command_line.meta->from.kind ==
        types::origin_kind::command_line);
    REQUIRE(o.command_line.output);
    CHECK(o.command_line.output->value == "/tmp/out");
    CHECK(o.threads == 3);
    const auto& s = o.command_line.settings;
    REQUIRE(s.size() == 2);
    CHECK(s[0].codec == "document_index");
    CHECK(s[0].key == "path");
    CHECK(s[0].value == "x.tsv");
    CHECK(s[0].from.kind == types::origin_kind::command_line);
}

TEST_CASE("config is a command", "[config][arguments]") {
    const auto parsed = parse({"config"});
    REQUIRE(std::holds_alternative<types::invocation>(parsed));
    CHECK(std::get<types::invocation>(parsed).which ==
        types::command::config);
}

TEST_CASE("gc takes its collection options", "[config][arguments]") {
    const auto parsed = parse({"gc", "--dry-run", "--cap", "1000",
        "--grace", "60"});
    REQUIRE(std::holds_alternative<types::invocation>(parsed));
    const auto& c = std::get<types::invocation>(parsed).collection;
    CHECK(c.dry_run);
    CHECK(c.cap == 1000);
    CHECK(c.grace == std::chrono::seconds(60));
}

TEST_CASE("malformed command lines are usage errors", "[config][arguments]") {
    for (const auto& args : std::vector<std::vector<std::string_view>>{
             {}, {"write"}, {"check", "diff"}, {"check", "-j", "0"},
             {"check", "--set", "novalue"}, {"diff", "--grace", "5"},
             {"check", "--frobnicate"}, {"gc", "--cap"},
             {"generate", "--commit", "HEAD"}, {"diff", "--commit", "HEAD"},
             {"check", "--commit", ""}, {"check", "--commit", "--timings"},
             {"check", "--commit"}})
        CHECK(std::holds_alternative<config::usage_error>(parse(args)));
}

TEST_CASE("check takes the revision of --commit as written",
    "[config][arguments]") {
    const auto parsed = parse({"check", "--commit", "main~2"});
    REQUIRE(std::holds_alternative<types::invocation>(parsed));
    CHECK(std::get<types::invocation>(parsed).revision == "main~2");
    CHECK(std::get<types::invocation>(parse({"check"})).revision.empty());
}

TEST_CASE("check takes the base of --ratchet when one follows it",
    "[config][arguments]") {
    const auto base_of = [](std::vector<std::string_view> args) {
        const auto parsed = parse(std::move(args));
        REQUIRE(std::holds_alternative<types::invocation>(parsed));
        return std::get<types::invocation>(parsed).ratchet;
    };
    CHECK_FALSE(base_of({"check"}));
    CHECK(base_of({"check", "--ratchet"}) == "");
    CHECK(base_of({"check", "--ratchet", "main"}) == "main");
    CHECK(base_of({"check", "--ratchet", "--timings"}) == "");
    CHECK(base_of({"--ratchet", "check"}) == "");
    CHECK(base_of({"check", "--ratchet=check"}) == "check");

    for (const auto& args :
         std::vector<std::vector<std::string_view>>{{"diff", "--ratchet"},
             {"check", "--ratchet=", "x"},
             {"check", "--ratchet", "main", "--commit", "HEAD"}})
        CHECK(std::holds_alternative<config::usage_error>(parse(args)));
}

TEST_CASE("The --help and --version options win over everything else",
    "[config][arguments]") {
    CHECK(std::holds_alternative<config::show_help>(
        parse({"check", "--bogus", "-h"})));
    CHECK(std::holds_alternative<config::show_version>(
        parse({"--version"})));
}

TEST_CASE("check takes each --codec, in order, and no other command does",
    "[config][arguments]") {
    const auto parsed = parse({"check", "--codec", "portable_cpp",
        "--commit", "HEAD", "--codec", "sql_hygiene"});
    REQUIRE(std::holds_alternative<types::invocation>(parsed));
    CHECK(std::get<types::invocation>(parsed).only ==
        std::vector<std::string>{"portable_cpp", "sql_hygiene"});
    CHECK(std::get<types::invocation>(parse({"check"})).only.empty());

    for (const auto& args : std::vector<std::vector<std::string_view>>{
             {"generate", "--codec", "x"}, {"check", "--codec"},
             {"check", "--codec", "--timings"}, {"check", "--codec", ""}})
        CHECK(std::holds_alternative<config::usage_error>(parse(args)));
}

TEST_CASE("env takes its action, version new's description and its "
    "options", "[config][arguments]") {
    const auto env_of = [](std::vector<std::string_view> args) {
        const auto parsed = parse(std::move(args));
        REQUIRE(std::holds_alternative<types::invocation>(parsed));
        const auto& i = std::get<types::invocation>(parsed);
        CHECK(i.which == types::command::env);
        return i.env;
    };
    const auto g = env_of({"env", "generate", "--input", "name=dev",
        "--input", "empty=", "--var", "A_B=x=y", "--file", "local.env"});
    CHECK(g.action == types::env_action::generate);
    CHECK(g.inputs == std::vector<std::pair<std::string, std::string>>{
        {"name", "dev"}, {"empty", ""}});
    CHECK(g.variables == std::vector<std::pair<std::string, std::string>>{
        {"A_B", "x=y"}});
    CHECK(g.file == "local.env");

    CHECK(env_of({"env", "version"}).action == types::env_action::version);
    CHECK(env_of({"env", "status"}).action == types::env_action::status);

    const auto n = env_of({"env", "version", "new", "Adds two.", "--adds",
        "A,B", "--adds", "C", "--removes", "D", "--today", "2026-11-21"});
    CHECK(n.action == types::env_action::version_new);
    CHECK(n.description == "Adds two.");
    CHECK(n.adds == std::vector<std::string>{"A", "B", "C"});
    CHECK(n.removes == std::vector<std::string>{"D"});
    CHECK(env_of({"env", "version", "new", "Moved.", "--format-only"})
        .format_only);
}

TEST_CASE("malformed env command lines are usage errors",
    "[config][arguments]") {
    for (const auto& args : std::vector<std::vector<std::string_view>>{
             {"env"}, {"env", "frobnicate"}, {"env", "version", "new"},
             {"env", "status", "extra"}, {"env", "status", "--input", "a=b"},
             {"env", "generate", "--input", "novalue"},
             {"env", "generate", "--adds", "A"},
             {"env", "version", "--format-only"},
             {"env", "version", "new", "x", "--file", "f"},
             {"env", "generate", "--today", "2026-11-21"},
             {"check", "--input", "a=b"}, {"generate", "--format-only"}})
        CHECK(std::holds_alternative<config::usage_error>(parse(args)));
}

TEST_CASE("sync takes an environment and, for a release, a commit",
    "[config][arguments]") {
    const auto plain = parse({"sync"});
    REQUIRE(std::holds_alternative<types::invocation>(plain));
    CHECK(std::get<types::invocation>(plain).which == types::command::sync);
    const auto release = parse({"sync", "--environment", "v1", "--commit",
        "v1.0"});
    REQUIRE(std::holds_alternative<types::invocation>(release));
    const auto& o = std::get<types::invocation>(release);
    CHECK(o.environment == "v1");
    CHECK(o.revision == "v1.0");
    CHECK(std::holds_alternative<config::usage_error>(
        parse({"sync", "--commit", "v1.0"})));
}

TEST_CASE("db takes an action and the options of init",
    "[config][arguments]") {
    const auto init = parse({"db", "init", "--database", "dbname=compass",
        "--text-search", "simple"});
    REQUIRE(std::holds_alternative<types::invocation>(init));
    const auto& o = std::get<types::invocation>(init);
    CHECK(o.which == types::command::db);
    CHECK(o.db.action == types::db_action::init);
    CHECK(o.db.database == "dbname=compass");
    CHECK(o.db.text_search == "simple");
    const auto hooks = parse({"db", "hooks", "install"});
    REQUIRE(std::holds_alternative<types::invocation>(hooks));
    CHECK(std::get<types::invocation>(hooks).db.action ==
        types::db_action::hooks_install);
    for (const auto& args : std::vector<std::vector<std::string_view>>{
             {"db"}, {"db", "drop"}, {"db", "hooks"},
             {"db", "status", "--database", "x"},
             {"check", "--environment", "x"}, {"gen", "--commit", "HEAD"}})
        CHECK(std::holds_alternative<config::usage_error>(parse(args)));
}
