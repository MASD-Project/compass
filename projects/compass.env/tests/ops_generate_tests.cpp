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
#include "compass.env/ops/generate.hpp"

#include <algorithm>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.env/ops/format.hpp"
#include "acme_format.hpp"

namespace {

using namespace compass::env;
using Catch::Matchers::ContainsSubstring;

types::env_format format_of(const tests::acme& a) {
    auto f = ops::read_format(a.tables());
    REQUIRE(f);
    return *f;
}

ops::sources sources_with(ops::values existing = {}) {
    return {.inputs = {{"name", "dev"}, {"base", "100"}, {"now", "T"}},
        .overrides = {},
        .existing = std::move(existing),
        .environment = [](const std::string&) {
            return std::optional<std::string>{};
        },
        .read_file = {},
        .secret = [](std::size_t n) { return std::string(n, 's'); }};
}

constexpr std::string_view expected =
    "# Acme environment: dev\n"
    "# Generated on T\n"
    "\n"
    "ACME_ENV_VERSION=3\n"
    "\n"
    "# Identity\n"
    "ACME_NAME=dev\n"
    "ACME_PORT=105\n"
    "ACME_HOST=localhost\n"
    "# The address, {scheme}://host:port.\n"
    "ACME_URL=http://localhost:105\n"
    "\n"
    "# Secrets\n"
    "ACME_DB_PASSWORD=ssssssssssssssssssssssssssssssss\n"
    "\n"
    "# Services\n"
    "\n"
    "ACME_SMALL_USER=acme_dev_SMALL\n"
    "ACME_SMALL_POOL=8\n"
    "ACME_SMALL_PORT=5432\n"
    "\n"
    "ACME_BIG_USER=acme_dev_BIG\n"
    "ACME_BIG_POOL=16\n"
    "ACME_BIG_PORT=5432\n";

}

TEST_CASE("generation writes every live row in order, with its comments, "
    "and stamps the highest version", "[generate]") {
    const auto f = format_of({});
    const auto services = ops::read_services(tests::acme_registry);
    const auto g = ops::generate(f, services, sources_with());
    REQUIRE(g);
    CHECK(g->text == expected);
    CHECK(g->names.front() == "ACME_ENV_VERSION");
    CHECK(g->names.size() == 12);
}

TEST_CASE("a per-service variable is written for each service with its "
    "fields, in registry order, and a service list narrows it",
    "[generate]") {
    const auto services = ops::read_services(tests::acme_registry);
    REQUIRE(services.size() == 3);
    CHECK(services[2].fields.at("name") == "proxy");
    const auto g = ops::generate(format_of({}), services, sources_with());
    REQUIRE(g);
    CHECK_THAT(g->text, !ContainsSubstring("proxy"));
    CHECK(g->text.find("ACME_SMALL_USER") < g->text.find("ACME_BIG_USER"));
    CHECK_THAT(g->text, ContainsSubstring("ACME_BIG_POOL=16\n"));
    CHECK_THAT(g->text, ContainsSubstring("ACME_SMALL_POOL=8\n"));
}

TEST_CASE("a secret and a kept value survive regeneration; a value made "
    "from inputs does not", "[generate]") {
    const auto services = ops::read_services(tests::acme_registry);
    const auto g = ops::generate(format_of({}), services, sources_with({
        {"ACME_DB_PASSWORD", "kept-secret"},
        {"ACME_HOST", "db.example"},
        {"ACME_NAME", "stale"}}));
    REQUIRE(g);
    CHECK_THAT(g->text, ContainsSubstring("ACME_DB_PASSWORD=kept-secret\n"));
    CHECK_THAT(g->text, ContainsSubstring("ACME_HOST=db.example\n"));
    CHECK_THAT(g->text, ContainsSubstring("ACME_URL=http://db.example:105\n"));
    CHECK_THAT(g->text, ContainsSubstring("ACME_NAME=dev\n"));
}

TEST_CASE("an empty kept secret is generated again", "[generate]") {
    const auto g = ops::generate(format_of({}), {},
        sources_with({{"ACME_DB_PASSWORD", ""}}));
    REQUIRE(g);
    CHECK_THAT(g->text, ContainsSubstring(
        "ACME_DB_PASSWORD=ssssssssssssssssssssssssssssssss\n"));
}

TEST_CASE("a conditional row and its section are written only when its "
    "condition holds, and a fallback reads its value", "[generate]") {
    const auto services = ops::read_services(tests::acme_registry);
    const auto g = ops::generate(format_of({}), services,
        sources_with({{"PGPORT", "6543"}}));
    REQUIRE(g);
    CHECK_THAT(g->text, ContainsSubstring("\n# Late\nPGPORT=6543\n"));
    CHECK_THAT(g->text, ContainsSubstring("ACME_BIG_PORT=6543\n"));
}

TEST_CASE("carried variables keep each matching variable of the old file, "
    "in name order, under their section", "[generate]") {
    const auto g = ops::generate(format_of({}), {}, sources_with({
        {"ACME_SOURCE_ZED_KEY", "z"},
        {"ACME_SOURCE_ABC_KEY", "a"},
        {"ACME_SOURCE_EMPTY_KEY", ""},
        {"ACME_OTHER", "o"}}));
    REQUIRE(g);
    CHECK(g->text.ends_with("\n# Sources\nACME_SOURCE_ABC_KEY=a\n"
                            "ACME_SOURCE_ZED_KEY=z\n"));
    CHECK_THAT(g->text, !ContainsSubstring("ACME_OTHER"));
}

TEST_CASE("an override sets a variable whatever its kind, and one naming "
    "no variable fails", "[generate]") {
    auto s = sources_with({{"ACME_HOST", "db.example"}});
    s.overrides = {{"ACME_HOST", "forced"}};
    const auto g = ops::generate(format_of({}), {}, s);
    REQUIRE(g);
    CHECK_THAT(g->text, ContainsSubstring("ACME_HOST=forced\n"));
    s.overrides = {{"ACME_NOPE", "x"}};
    const auto bad = ops::generate(format_of({}), {}, s);
    REQUIRE_FALSE(bad);
    CHECK_THAT(bad.error(), ContainsSubstring("ACME_NOPE"));
}

TEST_CASE("generation needs every declared input and refuses any other",
    "[generate]") {
    auto s = sources_with();
    s.inputs.erase("base");
    const auto missing = ops::generate(format_of({}), {}, s);
    REQUIRE_FALSE(missing);
    CHECK_THAT(missing.error(), ContainsSubstring("base"));
    s = sources_with();
    s.inputs["colour"] = "red";
    const auto extra = ops::generate(format_of({}), {}, s);
    REQUIRE_FALSE(extra);
    CHECK_THAT(extra.error(), ContainsSubstring("colour"));
}

TEST_CASE("of two rows of one name with exclusive conditions, the one "
    "that holds is written in the place of its row", "[generate]") {
    const auto f = format_of(tests::pooled_acme());
    const auto services = ops::read_services(tests::acme_registry);

    const auto on = ops::generate(f, services, sources_with());
    REQUIRE(on);
    CHECK_THAT(on->text, ContainsSubstring("ACME_POOLER=1\nACME_DB=105\n"
        "ACME_DB_URL=db:105\n"));
    CHECK_THAT(on->text, ContainsSubstring("ACME_SMALL_PORT=5432\n"
        "ACME_SMALL_DB=105\nACME_SMALL_LISTENER=5432\n\nACME_BIG_USER"));
    CHECK(std::ranges::count(on->names, "ACME_DB") == 1);

    const auto off = ops::generate(f, services,
        sources_with({{"ACME_POOLER", "0"}}));
    REQUIRE(off);
    CHECK_THAT(off->text, ContainsSubstring("ACME_POOLER=0\nACME_DB=5432\n"
        "ACME_DB_URL=db:5432\n"));
    CHECK_THAT(off->text, ContainsSubstring("ACME_SMALL_PORT=5432\n"
        "ACME_SMALL_DB=5432\n\nACME_BIG_USER"));
    CHECK_THAT(off->text, !ContainsSubstring("LISTENER"));
    CHECK(std::ranges::count(off->names, "ACME_DB") == 1);
}

TEST_CASE("a skipped row leaves no value for later templates", "[generate]") {
    auto a = tests::pooled_acme();
    a.variables.insert(a.variables.begin() + 11, {
        {"ACME_EXTRA", "value", "x", "1", "", "secrets", "",
            "{ACME_POOLER} = 9", "", "Never written."},
        {"ACME_EXTRA_URL", "value", "{ACME_EXTRA:-none}", "1", "",
            "secrets", "", "", "", "Reads the skipped row."}});
    const auto g = ops::generate(format_of(a), {}, sources_with());
    REQUIRE(g);
    CHECK_THAT(g->text, ContainsSubstring("ACME_DB_URL=db:105\n"));
    CHECK_THAT(g->text, ContainsSubstring("ACME_EXTRA_URL=none\n"));
    CHECK_THAT(g->text, !ContainsSubstring("ACME_EXTRA="));
}

TEST_CASE("a condition may compare the row's own value", "[generate]") {
    tests::acme a;
    a.variables.insert(a.variables.begin() + 7, tests::row{"ACME_MODE",
        "kept", "on", "1", "", "secrets", "", "{ACME_MODE} != off", "",
        "Written unless off."});
    const auto on = ops::generate(format_of(a), {}, sources_with());
    REQUIRE(on);
    CHECK_THAT(on->text, ContainsSubstring("ACME_MODE=on\n"));
    const auto off = ops::generate(format_of(a), {},
        sources_with({{"ACME_MODE", "off"}}));
    REQUIRE(off);
    CHECK_THAT(off->text, !ContainsSubstring("ACME_MODE"));
}

TEST_CASE("an inherited value reads the process environment when the file "
    "has none, and a file value quotes a file's lines", "[generate]") {
    tests::acme a;
    a.variables.push_back({"ACME_LEVEL", "inherited", "info", "1", "",
        "identity", "", "", "", "A level."});
    a.variables.push_back({"ACME_KEY", "file", "{checkout}/key.pem", "1", "",
        "identity", "", "", "", "A key."});
    auto s = sources_with();
    s.inputs["checkout"] = "/c";
    s.environment = [](const std::string& n) {
        return n == "ACME_LEVEL" ? std::optional<std::string>("debug")
                                 : std::nullopt;
    };
    s.read_file = [](const std::string& p) {
        return p == "/c/key.pem" ? std::optional<std::string>("A\nB\n")
                                 : std::nullopt;
    };
    const auto g = ops::generate(format_of(a), {}, s);
    REQUIRE(g);
    CHECK_THAT(g->text, ContainsSubstring("ACME_LEVEL=debug\n"));
    CHECK_THAT(g->text, ContainsSubstring("ACME_KEY=\"A\\nB\\n\"\n"));
}

TEST_CASE("an environment file reads as its first value of each name",
    "[generate]") {
    const auto v = ops::read_env("# c\nA=1\n\n  # d\nB=x=y\nA=2\nnoequals\n");
    CHECK(v == ops::values{{"A", "1"}, {"B", "x=y"}});
}

TEST_CASE("status is current for a file generation just wrote",
    "[status]") {
    const auto f = format_of({});
    const auto services = ops::read_services(tests::acme_registry);
    const auto g = ops::generate(f, services, sources_with());
    REQUIRE(g);
    const auto s = ops::status_of(f, services, ops::read_env(g->text));
    CHECK_FALSE(s.stale());
    CHECK(s.version == 3u);
    CHECK(s.current == 3);
}

TEST_CASE("status finds an old version, a missing variable, a removed one "
    "and an undeclared one", "[status]") {
    const auto f = format_of({});
    const auto services = ops::read_services(tests::acme_registry);
    auto g = ops::generate(f, services, sources_with());
    REQUIRE(g);
    auto existing = ops::read_env(g->text);
    existing["ACME_ENV_VERSION"] = "2";
    existing.erase("ACME_BIG_POOL");
    existing["ACME_OLD"] = "x";
    existing["ACME_STRAY"] = "y";
    existing["ACME_SOURCE_ABC_KEY"] = "kept";
    existing["PGPORT"] = "1";
    const auto s = ops::status_of(f, services, existing);
    CHECK(s.stale());
    CHECK(s.version == 2u);
    CHECK(s.missing == std::vector<std::string>{"ACME_BIG_POOL"});
    CHECK(s.removed == std::vector<std::string>{"ACME_OLD"});
    CHECK(s.undeclared == std::vector<std::string>{"ACME_STRAY"});
}

TEST_CASE("two rows of one name count once in status, whichever is "
    "written", "[status]") {
    const auto f = format_of(tests::pooled_acme());
    const auto services = ops::read_services(tests::acme_registry);
    for (const auto* pooler : {"1", "0"}) {
        const auto g = ops::generate(f, services,
            sources_with({{"ACME_POOLER", pooler}}));
        REQUIRE(g);
        const auto s = ops::status_of(f, services, ops::read_env(g->text));
        CHECK_FALSE(s.stale());
        CHECK(s.missing.empty());
        CHECK(s.undeclared.empty());
        CHECK(s.removed.empty());
    }
}

TEST_CASE("a new service in the registry makes a file stale", "[status]") {
    const auto f = format_of({});
    const auto before = ops::read_services(tests::acme_registry);
    const auto g = ops::generate(f, before, sources_with());
    REQUIRE(g);
    auto after = before;
    after.push_back({{{"name", "new"}, {"key", "NEW"}, {"replicas", "1"}}});
    const auto s = ops::status_of(f, after, ops::read_env(g->text));
    CHECK(s.stale());
    CHECK(s.missing == std::vector<std::string>{"ACME_NEW_USER",
        "ACME_NEW_POOL", "ACME_NEW_PORT"});
}

TEST_CASE("a file that differs only in the time it was made is the same "
    "file; any other difference is not", "[generate]") {
    const std::string now = "2026-11-22 10:00:05 UTC";
    const std::string before = "# made 2026-11-21 09:59:59 UTC\nA=1\n"
                               "B=x 2026-11-21 09:59:59 UTC y\n";
    const std::string after = "# made " + now + "\nA=1\nB=x " + now + " y\n";
    CHECK(ops::same_but_time(before, after, now));
    CHECK(ops::same_but_time(after, after, now));
    CHECK_FALSE(ops::same_but_time(before, after + "C=2\n", now));
    CHECK_FALSE(ops::same_but_time("# made 2026-11-21 09:59:59 UTC\nA=2\n"
        "B=x 2026-11-21 09:59:59 UTC y\n", after, now));
    CHECK_FALSE(ops::same_but_time("# made yesterday at noon UTC.\nA=1\n"
        "B=x 2026-11-21 09:59:59 UTC y\n", after, now));
    CHECK_FALSE(ops::same_but_time("A=1\n", "A=2\n", ""));
}
