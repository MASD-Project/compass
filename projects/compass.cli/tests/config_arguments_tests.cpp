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

TEST_CASE("a bare command runs over the working directory with the "
    "defaults", "[config][arguments]") {
    const auto parsed = parse({"gen"});
    REQUIRE(std::holds_alternative<types::options>(parsed));
    const auto& o = std::get<types::options>(parsed);
    CHECK(o.which == types::command::generate);
    CHECK(o.checkout == "/work/repo");
    CHECK(o.meta == "/work/repo/doc/meta");
    CHECK(o.output_root == "/work/repo/build/output/compass");
    CHECK(o.threads == 8);
    CHECK(o.settings.size() == 0);
}

TEST_CASE("-C is relative to the working directory, --meta and --out to "
    "the checkout", "[config][arguments]") {
    const auto parsed = parse({"-C", "../other/", "diff", "--meta", "meta",
        "--out", "/tmp/out", "-j", "3", "--set", "document_index.path=x.tsv",
        "--set", "required_sections.enabled=false"});
    REQUIRE(std::holds_alternative<types::options>(parsed));
    const auto& o = std::get<types::options>(parsed);
    CHECK(o.which == types::command::diff);
    CHECK(o.checkout == "/work/other");
    CHECK(o.meta == "/work/other/meta");
    CHECK(o.output_root == "/tmp/out");
    CHECK(o.threads == 3);
    REQUIRE(o.settings.size() == 2);
    CHECK(o.settings.codecs[0] == "document_index");
    CHECK(o.settings.keys[0] == "path");
    CHECK(o.settings.values[0] == "x.tsv");
}

TEST_CASE("gc takes its collection options", "[config][arguments]") {
    const auto parsed = parse({"gc", "--dry-run", "--cap", "1000",
        "--grace", "60"});
    REQUIRE(std::holds_alternative<types::options>(parsed));
    const auto& c = std::get<types::options>(parsed).collection;
    CHECK(c.dry_run);
    CHECK(c.cap == 1000);
    CHECK(c.grace == std::chrono::seconds(60));
}

TEST_CASE("malformed command lines are usage errors", "[config][arguments]") {
    for (const auto& args : std::vector<std::vector<std::string_view>>{
             {}, {"write"}, {"check", "diff"}, {"check", "-j", "0"},
             {"check", "--set", "novalue"}, {"diff", "--grace", "5"},
             {"check", "--frobnicate"}, {"gc", "--cap"}})
        CHECK(std::holds_alternative<config::usage_error>(parse(args)));
}

TEST_CASE("--help and --version win over everything else",
    "[config][arguments]") {
    CHECK(std::holds_alternative<config::show_help>(
        parse({"check", "--bogus", "-h"})));
    CHECK(std::holds_alternative<config::show_version>(
        parse({"--version"})));
}
