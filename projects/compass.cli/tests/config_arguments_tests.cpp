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
#include <string_view>
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

TEST_CASE("-C is relative to the working directory; --meta, --out and "
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

TEST_CASE("--help and --version win over everything else",
    "[config][arguments]") {
    CHECK(std::holds_alternative<config::show_help>(
        parse({"check", "--bogus", "-h"})));
    CHECK(std::holds_alternative<config::show_version>(
        parse({"--version"})));
}
