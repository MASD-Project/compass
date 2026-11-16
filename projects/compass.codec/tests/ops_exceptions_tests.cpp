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
#include "compass.codec/ops/exceptions.hpp"

#include <array>
#include <chrono>
#include <span>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

namespace {

namespace ct = compass::codec::types;
using namespace compass::codec::ops;
using namespace std::chrono_literals;
using Catch::Matchers::ContainsSubstring;

constexpr std::chrono::year_month_day today{2026y, std::chrono::November,
    16d};

constexpr std::string_view valid_fields =
    " expires=2026-12-31 approver=marco reason=bootstrap table";

std::string except(std::string_view rule,
    std::string_view fields = valid_fields) {
    return "compass:except(" + std::string(rule) + ")" + std::string(fields);
}

constexpr std::array<ct::rule, 2> sql_rules{{
    {"not_re_runnable", ""}, {"duplicate_index", ""}}};

void diagnostics(const ct::corpus_view&, ct::diagnostic_table&) {}
void files(const ct::corpus_view&, ct::file_table&) {}

/**
 * @brief A registry with sql, a diagnostics codec that runs, off, one
 * that does not, and gen, a files codec that runs.
 */
ct::descriptor codec(std::string_view name, ct::codec_function run,
    std::span<const ct::rule> rules = {}) {
    ct::descriptor d;
    d.name = name;
    d.rules = rules;
    d.run = run;
    return d;
}

ct::registry registry() {
    ct::registry r;
    r.codecs = {codec("sql", &diagnostics, sql_rules),
        codec("off", &diagnostics, sql_rules), codec("gen", &files)};
    r.enabled = {1, 0, 1};
    return r;
}

ct::file_exceptions parse(std::string_view path, const std::string& text) {
    return parse_exceptions(path, text);
}

std::vector<std::string> rules_of(const excusal& e) {
    std::vector<std::string> out;
    for (const auto& f : e.findings)
        out.push_back(f.path + ":" + std::to_string(f.line) + ":" + f.rule);
    return out;
}

}

TEST_CASE("a trailing exception covers its own line",
    "[ops_exceptions]") {
    const auto r = parse("a.sql", "select 1;\ncreate table a (x int); -- " +
        except("sql/not_re_runnable") + "\nselect 2;\n");
    REQUIRE(r.malformed.empty());
    REQUIRE(r.exceptions.size() == 1);
    const auto& e = r.exceptions[0];
    CHECK(e.line == 2);
    CHECK(e.covers == 2);
    CHECK(e.scope == ct::exception_scope::line);
    CHECK(e.rule == "sql/not_re_runnable");
    CHECK(e.approver == "marco");
    CHECK(e.reason == "bootstrap table");
    CHECK(e.expires == std::chrono::year_month_day{2026y,
        std::chrono::December, 31d});
}

TEST_CASE("an exception alone on its line covers the next line, and "
          "stacked exceptions cover the same line", "[ops_exceptions]") {
    const auto r = parse("a.cpp", "    // " + except("sql/not_re_runnable") +
        "\n    /// " + except("sql/duplicate_index") + "\nint x;\n");
    REQUIRE(r.exceptions.size() == 2);
    CHECK(r.exceptions[0].covers == 3);
    CHECK(r.exceptions[1].covers == 3);
}

TEST_CASE("the closer of a block or HTML comment is not part of the "
          "reason", "[ops_exceptions]") {
    const auto r = parse("a.cpp", "int x; /* " + except("sql/not_re_runnable")
        + " */\n<!-- " + except("sql/duplicate_index") + " -->\r\n");
    REQUIRE(r.exceptions.size() == 2);
    CHECK(r.exceptions[0].reason == "bootstrap table");
    CHECK(r.exceptions[1].reason == "bootstrap table");
    CHECK(r.exceptions[1].covers == 3);
}

TEST_CASE("each comment syntax opens an exception", "[ops_exceptions]") {
    for (const auto& [path, opener] : {
             std::pair{"a.sql", "-- "}, std::pair{"CMakeLists.txt", "# "},
             std::pair{"a.sh", "#"}, std::pair{"a.py", "# "},
             std::pair{"a.ts", "// "}, std::pair{"a.el", ";; "}}) {
        const auto r = parse(path, std::string(opener) +
            except("sql/not_re_runnable") + "\nx\n");
        INFO(path);
        REQUIRE(r.exceptions.size() == 1);
        CHECK(r.exceptions[0].covers == 2);
    }
}

TEST_CASE("an org file opens an exception only with a comment line",
    "[ops_exceptions]") {
    const auto text = "  # " + except("sql/not_re_runnable") +
        "\n- " + except("sql/not_re_runnable") +
        "\n#+begin_src sql\nx -- " + except("sql/not_re_runnable") +
        "\n// " + except("sql/not_re_runnable") + "\n#+end_src\n";
    const auto r = parse("doc/a.org", text);
    REQUIRE(r.exceptions.size() == 1);
    CHECK(r.exceptions[0].line == 1);
    CHECK(r.exceptions[0].covers == 2);
    CHECK(r.malformed.empty());
}

TEST_CASE("prose that names the marker outside a comment is not an "
          "exception", "[ops_exceptions]") {
    const auto r = parse("a.cpp", "auto s = \"" +
        except("sql/not_re_runnable") + "\";\n");
    CHECK(r.exceptions.empty());
    CHECK(r.malformed.empty());
}

TEST_CASE("an exception of file scope covers no line", "[ops_exceptions]") {
    const auto r = parse("a.sql", "-- compass:except-file(sql/duplicate_index)"
        + std::string(valid_fields) + "\nselect 1;\n");
    REQUIRE(r.exceptions.size() == 1);
    CHECK(r.exceptions[0].scope == ct::exception_scope::file);
    CHECK(r.exceptions[0].covers == 0);
}

TEST_CASE("a comment that is not an exception is malformed, with why",
    "[ops_exceptions]") {
    const auto r = parse("a.sql",
        "-- compass:except sql/not_re_runnable reason=x\n"
        "-- compass:except(not-a-rule)" + std::string(valid_fields) + "\n" +
        "-- " + except("sql/x", " expires=2027-02-30 approver=a reason=r") +
        "\n-- " + except("sql/x", " owner=a reason=r") +
        "\n-- " + except("sql/x", " approver=a approver=b reason=r") +
        "\n-- " + except("sql/x", " approver reason=r") + "\n");
    CHECK(r.exceptions.empty());
    REQUIRE(r.malformed.size() == 6);
    CHECK_THAT(r.malformed[0].why, ContainsSubstring("parentheses"));
    CHECK_THAT(r.malformed[1].why, ContainsSubstring("not-a-rule"));
    CHECK_THAT(r.malformed[2].why, ContainsSubstring("2027-02-30"));
    CHECK_THAT(r.malformed[3].why, ContainsSubstring("owner"));
    CHECK_THAT(r.malformed[4].why, ContainsSubstring("twice"));
    CHECK_THAT(r.malformed[5].why, ContainsSubstring("key=value"));
    for (std::uint32_t i = 0; i < 6; ++i)
        CHECK(r.malformed[i].line == i + 1);
}

TEST_CASE("a valid exception excuses only its rule on the line it covers",
    "[ops_exceptions]") {
    const std::array files{parse("a.sql", "x; -- " +
        except("sql/not_re_runnable") + "\ny;\n")};
    const std::array<finding_site, 4> sites{{
        {"a.sql", 1, "sql/not_re_runnable"},
        {"a.sql", 1, "sql/duplicate_index"},
        {"a.sql", 2, "sql/not_re_runnable"},
        {"b.sql", 1, "sql/not_re_runnable"}}};
    const auto e = excuse(sites, files, registry(), today);
    CHECK(e.excused == std::vector<std::uint8_t>{1, 0, 0, 0});
    CHECK(e.findings.empty());
}

TEST_CASE("an exception is valid through its expiry date and reported on "
          "the line it covers after it", "[ops_exceptions]") {
    const std::array files{parse("a.sql", "-- " + except("sql/not_re_runnable",
        " expires=2026-11-16 approver=marco reason=r") + "\nx;\n")};
    const std::array<finding_site, 1> sites{{
        {"a.sql", 2, "sql/not_re_runnable"}}};

    const auto on_the_day = excuse(sites, files, registry(), today);
    CHECK(on_the_day.excused == std::vector<std::uint8_t>{1});
    CHECK(on_the_day.findings.empty());

    const auto day_after = excuse(sites, files, registry(),
        std::chrono::year_month_day{2026y, std::chrono::November, 17d});
    CHECK(day_after.excused == std::vector<std::uint8_t>{0});
    CHECK(rules_of(day_after) ==
        std::vector<std::string>{"a.sql:2:exception/expired"});
    CHECK_THAT(day_after.findings[0].message,
        ContainsSubstring("expired on 2026-11-16"));
    CHECK_THAT(day_after.findings[0].fix, ContainsSubstring("line 1"));
}

TEST_CASE("an exception missing a field excuses nothing and names what is "
          "missing", "[ops_exceptions]") {
    const std::array files{parse("a.sql", "x; -- " +
        except("sql/not_re_runnable", " approver=marco") + "\n")};
    const std::array<finding_site, 1> sites{{
        {"a.sql", 1, "sql/not_re_runnable"}}};
    const auto e = excuse(sites, files, registry(), today);
    CHECK(e.excused == std::vector<std::uint8_t>{0});
    REQUIRE(rules_of(e) ==
        std::vector<std::string>{"a.sql:1:exception/incomplete"});
    CHECK_THAT(e.findings[0].message, ContainsSubstring("a reason"));
    CHECK_THAT(e.findings[0].message, ContainsSubstring("an expiry"));
    CHECK_THAT(e.findings[0].message, !ContainsSubstring("approver"));
}

TEST_CASE("an exception that covers no finding is stale, unless its codec "
          "does not run", "[ops_exceptions]") {
    const std::array files{parse("a.sql", "-- " +
        except("sql/not_re_runnable") + "\nx;\n-- " +
        except("off/not_re_runnable") + "\ny;\n")};
    const auto e = excuse({}, files, registry(), today);
    CHECK(rules_of(e) == std::vector<std::string>{"a.sql:1:exception/stale"});
}

TEST_CASE("an exception naming an unknown rule or an exception rule is "
          "malformed and excuses nothing", "[ops_exceptions]") {
    const std::array files{parse("a.sql", "x; -- " +
        except("sql/no_such_rule") + "\ny; -- " +
        except("exception/expired") + "\nz; -- " +
        except("gen/drift") + "\n")};
    const std::array<finding_site, 3> sites{{
        {"a.sql", 1, "sql/no_such_rule"},
        {"a.sql", 2, "exception/expired"},
        {"a.sql", 3, "gen/drift"}}};
    const auto e = excuse(sites, files, registry(), today);
    CHECK(e.excused == std::vector<std::uint8_t>{0, 0, 1});
    CHECK(rules_of(e) == std::vector<std::string>{
        "a.sql:1:exception/malformed", "a.sql:2:exception/malformed"});
}

TEST_CASE("an exception of file scope excuses its rule anywhere in its "
          "file", "[ops_exceptions]") {
    const std::array files{parse("a.sql",
        "-- compass:except-file(sql/duplicate_index)" +
        std::string(valid_fields) + "\n")};
    const std::array<finding_site, 3> sites{{
        {"a.sql", 7, "sql/duplicate_index"},
        {"a.sql", 9, "sql/duplicate_index"},
        {"a.sql", 9, "sql/not_re_runnable"}}};
    const auto e = excuse(sites, files, registry(), today);
    CHECK(e.excused == std::vector<std::uint8_t>{1, 1, 0});
    CHECK(e.findings.empty());
}

TEST_CASE("a malformed comment is reported on its line",
    "[ops_exceptions]") {
    const std::array files{parse("a.sql", "x;\n-- compass:except\n")};
    const auto e = excuse({}, files, registry(), today);
    CHECK(rules_of(e) ==
        std::vector<std::string>{"a.sql:2:exception/malformed"});
}
