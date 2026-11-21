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
#include "compass.crosscheck/codecs/service_integrity.hpp"

#include <string>
#include <utility>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.codec/ops/data_tables.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using Catch::Matchers::ContainsSubstring;
using tests::files;

namespace cops = compass::codec::ops;

const std::string registry = "model/services.org";

/**
 * @brief A made-up project: three services in its registry, billing and
 * ledger built from parts of kind daemon, and portal running on jvm.
 * ledger is disabled and has no database user.
 */
files project() {
    return {
        {registry,
         "#+title: Services\n"
         "#+type: ores.codegen.service_registry\n"
         "\n"
         "* acme.billing.server\n"
         ":PROPERTIES:\n"
         ":db_user:  billing_user\n"
         ":replicas: 1\n"
         ":END:\n"
         "\n"
         "* acme.ledger.server\n"
         ":PROPERTIES:\n"
         ":replicas: 1\n"
         ":enabled:  false\n"
         ":END:\n"
         "\n"
         "* acme.portal.web\n"
         ":PROPERTIES:\n"
         ":replicas: 1\n"
         ":runtime:  jvm\n"
         ":END:\n"},
        {"src/acme.billing/server/modeling/component_overview.org",
         "#+title: acme.billing.server\n"
         "#+type: ores.codegen.component\n"
         "#+component_kind: daemon\n"
         "#+full_name: acme.billing.server\n"},
        {"src/acme.billing/server/build.cmake", "x\n"},
        {"src/acme.billing/server/app.cpp", "int main() { serve(); }\n"},
        {"src/acme.ledger/server/modeling/component_overview.org",
         "#+title: acme.ledger.server\n"
         "#+type: ores.codegen.component\n"
         "#+component_kind: daemon\n"
         "#+full_name: acme.ledger.server\n"},
        {"src/acme.ledger/server/build.cmake", "x\n"},
        {"src/acme.ledger/server/app.cpp", "// TODO stub\n"},
        {"src/acme.tools/modeling/component_overview.org",
         "#+title: acme.tools\n"
         "#+type: ores.codegen.component\n"
         "#+component_kind: flat\n"
         "#+full_name: acme.tools\n"},
        {"db/users.sql", "create user billing_user;\n"},
        {"deploy/list.txt", "  billing\n  ledger\n  portal\n"},
        {"tools/run.py", "SERVICES = registry()\n"}};
}

using row = std::vector<std::string>;

cops::data_table table(std::string name, std::vector<std::string> columns,
    std::vector<row> rows) {
    cops::data_table t{.name = std::move(name), .line = 1,
        .columns = std::move(columns), .rows = std::move(rows),
        .row_lines = {}};
    for (std::size_t k = 0; k < t.rows.size(); ++k)
        t.row_lines.push_back(static_cast<std::uint32_t>(10 + k));
    return t;
}

std::vector<row> obligations() {
    return {
        {"Buildable", "exists", "{part}/build.cmake", "", "part",
         "a part builds"},
        {"Database user", "contains", "db/users.sql", "user {db_user};", "",
         "the service signs in"},
        {"Deployed", "has_line", "deploy/list.txt", "{name[1]}", "",
         "the fleet lists it"},
        {"Not a stub", "lacks", "{part}/app.cpp", "TODO stub", "enabled",
         "a stub never becomes ready"}};
}

cops::data_tables tables_with(std::vector<row> rows) {
    cops::data_tables t{.source = "compass/checks/service_integrity.org",
        .tables = {}};
    t.tables.push_back(table("Settings", {"setting", "value"},
        {{"part_kind", "daemon"}, {"default_runtime", "native"},
         {"single_source_minimum", "3"}}));
    t.tables.push_back(table("Runtimes", {"runtime", "has part"},
        {{"native", "yes"}, {"jvm", "no"}}));
    t.tables.push_back(table("Obligations", {"obligation", "test", "path",
        "value", "condition", "reason"}, std::move(rows)));
    t.tables.push_back(table("Single source files", {"glob", "reason"},
        {{"*.py", "scripts"}, {"deploy/*.sh", "deployment"}}));
    t.tables.push_back(table("Single source exceptions", {"glob", "reason"},
        {{"**/tests/**", "tests may name services"}}));
    return t;
}

tests::outcome run_with(const files& t, const cops::data_tables& data) {
    return tests::run(codecs::service_integrity, t,
        {{std::string(cops::data_option), cops::encode_data_tables(data)}});
}

tests::outcome run(const files& t) {
    return run_with(t, tables_with(obligations()));
}

}

TEST_CASE("a project whose services are whole has no finding",
    "[codecs][service_integrity]") {
    CHECK(run(project()).findings.empty());
}

TEST_CASE("a service part with no registry entry is named at its overview",
    "[codecs][service_integrity]") {
    auto t = project();
    t["src/acme.audit/server/modeling/component_overview.org"] =
        "#+title: acme.audit.server\n"
        "#+type: ores.codegen.component\n"
        "#+component_kind: daemon\n"
        "#+full_name: acme.audit.server\n";
    const auto r = run(t);
    REQUIRE(r.findings.size() == 1);
    CHECK(tests::places(r.findings) == std::vector<std::pair<std::string,
        std::uint32_t>>{{"src/acme.audit/server/modeling/"
        "component_overview.org", 4}});
    CHECK_THAT(std::get<2>(r.findings[0]),
        ContainsSubstring("acme.audit.server is a service part with no "
            "entry"));
}

TEST_CASE("a native entry with no part is named at its entry, and an entry "
    "of a runtime without parts is not", "[codecs][service_integrity]") {
    auto t = project();
    t.erase("src/acme.ledger/server/modeling/component_overview.org");
    const auto r = run(t);
    REQUIRE(r.findings.size() == 1);
    CHECK(tests::places(r.findings) == std::vector<std::pair<std::string,
        std::uint32_t>>{{registry, 10}});
    CHECK_THAT(std::get<2>(r.findings[0]), ContainsSubstring(
        "acme.ledger.server runs on native"));
}

TEST_CASE("the part kind is the check document's, so a part of another kind "
    "is no service part", "[codecs][service_integrity]") {
    auto t = project();
    t["src/acme.audit/modeling/component_overview.org"] =
        "#+title: acme.audit\n#+type: ores.codegen.component\n"
        "#+component_kind: service\n#+full_name: acme.audit\n";
    CHECK(run(t).findings.empty());
}

TEST_CASE("an entry whose runtime the Runtimes table lacks is reported",
    "[codecs][service_integrity]") {
    auto t = project();
    auto& text = t[registry];
    text.replace(text.find(":runtime:  jvm"), 14, ":runtime:  wasm");
    const auto r = run(t);
    REQUIRE(r.findings.size() == 1);
    CHECK_THAT(std::get<2>(r.findings[0]), ContainsSubstring(
        "acme.portal.web has the runtime wasm"));
}

TEST_CASE("a missing file breaks an exists obligation, named at the entry",
    "[codecs][service_integrity]") {
    auto t = project();
    t.erase("src/acme.billing/server/build.cmake");
    const auto r = run(t);
    REQUIRE(r.findings.size() == 1);
    CHECK(tests::places(r.findings) == std::vector<std::pair<std::string,
        std::uint32_t>>{{registry, 4}});
    CHECK(std::get<2>(r.findings[0]) == "acme.billing.server misses the "
        "obligation \"Buildable\": src/acme.billing/server/build.cmake does "
        "not exist; a part builds");
}

TEST_CASE("a file without the service's value breaks a contains obligation, "
    "and a service without the field owes nothing",
    "[codecs][service_integrity]") {
    auto t = project();
    t["db/users.sql"] = "create user other_user;\n";
    const auto r = run(t);
    REQUIRE(r.findings.size() == 1);
    CHECK(std::get<2>(r.findings[0]) == "acme.billing.server misses the "
        "obligation \"Database user\": db/users.sql does not hold 'user "
        "billing_user;'; the service signs in");
    t.erase("db/users.sql");
    CHECK(run(t).findings.size() == 1);
}

TEST_CASE("has_line needs a whole line, and a field's piece is taken at its "
    "dots", "[codecs][service_integrity]") {
    auto t = project();
    t["deploy/list.txt"] = "  billing_old\n  ledger\n  portal\n";
    const auto r = run(t);
    REQUIRE(r.findings.size() == 1);
    CHECK_THAT(std::get<2>(r.findings[0]), ContainsSubstring(
        "acme.billing.server misses the obligation \"Deployed\": "
        "deploy/list.txt has no line 'billing'"));
}

TEST_CASE("a lacks obligation conditioned on enabled holds only for enabled "
    "services", "[codecs][service_integrity]") {
    auto t = project();
    CHECK(run(t).findings.empty());
    t["src/acme.billing/server/app.cpp"] = "// TODO stub\n";
    const auto r = run(t);
    REQUIRE(r.findings.size() == 1);
    CHECK_THAT(std::get<2>(r.findings[0]), ContainsSubstring(
        "acme.billing.server misses the obligation \"Not a stub\": "
        "src/acme.billing/server/app.cpp holds 'TODO stub'"));
}

TEST_CASE("a negated condition holds for the services without the field",
    "[codecs][service_integrity]") {
    auto rows = obligations();
    rows.push_back({"No account", "exists", "{part}/no_db.txt", "",
        "part !db_user", ""});
    const auto r = run_with(project(), tables_with(rows));
    REQUIRE(r.findings.size() == 1);
    CHECK(std::get<2>(r.findings[0]) == "acme.ledger.server misses the "
        "obligation \"No account\": src/acme.ledger/server/no_db.txt does "
        "not exist");
}

TEST_CASE("a literal list of service names outside the registry is a second "
    "list", "[codecs][service_integrity]") {
    auto t = project();
    t["tools/run.py"] = "import os\n\nSERVICES = [\n"
        "    \"acme.billing.server\",\n    \"acme.ledger.server\",\n"
        "    \"acme.portal.web\",\n]\n";
    const auto r = run(t);
    REQUIRE(r.findings.size() == 1);
    CHECK(tests::places(r.findings) == std::vector<std::pair<std::string,
        std::uint32_t>>{{"tools/run.py", 4}});
    CHECK_THAT(std::get<2>(r.findings[0]), ContainsSubstring(
        "tools/run.py lists 3 registry services as a literal "
        "(acme.billing.server, acme.ledger.server, acme.portal.web)"));
}

TEST_CASE("a second list is reported only in a scanned file, unexcepted, of "
    "enough distinct names separated by punctuation alone",
    "[codecs][service_integrity]") {
    const std::string list =
        "(acme.billing.server acme.ledger.server acme.portal.web)\n";
    auto t = project();
    t["deploy/up.sh"] = "UNITS=" + list;
    t["deploy/up.txt"] = list;
    t["src/acme.billing/tests/fleet.py"] = list;
    t["tools/two.py"] = "x = ['acme.billing.server', 'acme.ledger.server']\n";
    t["tools/again.py"] = "x = ['acme.billing.server', 'acme.billing.server', "
        "'acme.ledger.server']\n";
    t["tools/prose.py"] = "# acme.billing.server calls acme.ledger.server "
        "and acme.portal.web\n";
    t["tools/longer.py"] = "x = ['acme.billing.server.crt', "
        "'acme.ledger.server.crt', 'acme.portal.web.crt']\n";
    const auto r = run(t);
    REQUIRE(r.findings.size() == 1);
    CHECK(std::get<0>(r.findings[0]) == "deploy/up.sh");
}

TEST_CASE("without its check document the codec says what it lacks",
    "[codecs][service_integrity]") {
    const auto r = tests::run(codecs::service_integrity, project());
    REQUIRE(r.findings.size() == 1);
    CHECK(std::get<0>(r.findings[0]) ==
        "compass/checks/service_integrity.org");
}

TEST_CASE("a missing table or column is unreadable",
    "[codecs][service_integrity]") {
    auto data = tables_with(obligations());
    data.tables.pop_back();
    auto r = run_with(project(), data);
    REQUIRE(r.findings.size() == 1);
    CHECK_THAT(std::get<2>(r.findings[0]),
        ContainsSubstring("Single source exceptions"));

    data = tables_with(obligations());
    data.tables[2].columns[5] = "why";
    r = run_with(project(), data);
    REQUIRE(r.findings.size() == 1);
    CHECK_THAT(std::get<2>(r.findings[0]), ContainsSubstring("reason"));
}

TEST_CASE("a row that does not read is unreadable and checks nothing",
    "[codecs][service_integrity]") {
    auto t = project();
    t.erase("src/acme.billing/server/build.cmake");
    const auto unreadable = [&](std::vector<row> rows) {
        const auto r = run_with(t, tables_with(std::move(rows)));
        REQUIRE(r.findings.size() == 1);
        return std::get<2>(r.findings[0]);
    };
    CHECK_THAT(unreadable({{"Odd", "matches", "a", "b", "", ""}}),
        ContainsSubstring("has the test 'matches'"));
    CHECK_THAT(unreadable({{"Open", "exists", "{part/x", "", "", ""}}),
        ContainsSubstring("a '{' is not closed"));
    CHECK_THAT(unreadable({{"Index", "has_line", "a", "{name[x]}", "", ""}}),
        ContainsSubstring("an index that is not a number"));
    CHECK_THAT(unreadable({{"Valueless", "contains", "a", "", "", ""}}),
        ContainsSubstring("tests a value and names none"));
    CHECK_THAT(unreadable({{"Typo", "exists", "{prat}/x", "", "", ""}}),
        ContainsSubstring("names the field prat, which no registry entry "
            "has"));
    CHECK_THAT(unreadable({{"Twice", "exists", "a", "", "", ""},
        {"Twice", "exists", "b", "", "", ""}}),
        ContainsSubstring("stated twice"));

    auto data = tables_with(obligations());
    data.tables[0].rows[2][1] = "one";
    const auto r = run_with(t, data);
    REQUIRE(r.findings.size() == 1);
    CHECK_THAT(std::get<2>(r.findings[0]), ContainsSubstring(
        "compass/checks/service_integrity.org:12: the setting "
        "'single_source_minimum' is 'one'"));
}

TEST_CASE("a checkout without a service registry is unreadable",
    "[codecs][service_integrity]") {
    auto t = project();
    t.erase(registry);
    const auto r = run(t);
    REQUIRE(r.findings.size() == 1);
    CHECK_THAT(std::get<2>(r.findings[0]), ContainsSubstring(
        "no document of type ores.codegen.service_registry"));
}
