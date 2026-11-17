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
#include "compass.crosscheck/codecs/subject_conformance.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using tests::files;

std::vector<std::string> messages(const tests::outcome& r) {
    std::vector<std::string> out;
    for (const auto& [path, line, message] : r.findings)
        out.push_back(path + ":" + std::to_string(line) + ": " + message);
    return out;
}

}

TEST_CASE("a declared subject outside the protocol is reported on its line",
    "[codecs][subject_conformance]") {
    const files t{
        {"projects/ores.refdata/modeling/ores.refdata.currency.org",
         ":PROPERTIES:\n"
         ":subject: refdata.v1.currencies.get\n"
         ":subject: refdata.v1.currencies.list_by_party\n"
         ":subject: refdata.v1.events.currencies.created\n"
         ":subject: refdata.v1.ops.rebuild\n"
         ":subject: {component}.v1.history.get\n"
         ":subject: refdata.v1.currencies.fetch\n"
         ":subject: refdata.v2.currencies.get\n"
         ":subject: refdata.v1.currency_events.updated\n"
         ":subject: refdata.v1.events.currencies.renamed\n"
         ":subject: refdata.v1.Currencies.get\n"
         ":subject: refdata.currencies.get\n"
         ":END:\n"},
        {"projects/ores.refdata/modeling/nested/ores.refdata.x.org",
         ":subject: refdata.v1.nested.fetch\n"}};
    CHECK(messages(tests::run(codecs::subject_conformance, t)) ==
        std::vector<std::string>{
            "projects/ores.refdata/modeling/ores.refdata.currency.org:7: "
            "refdata.v1.currencies.fetch: the verb 'fetch' is not one of the "
            "closed set of eight and is not in the reserved ops namespace",
            "projects/ores.refdata/modeling/ores.refdata.currency.org:8: "
            "refdata.v2.currencies.get: the version segment is 'v2', not "
            "'v1'",
            "projects/ores.refdata/modeling/ores.refdata.currency.org:9: "
            "refdata.v1.currency_events.updated: the four-segment event "
            "spelling; an event names the events namespace, as in "
            "component.v1.events.collection.action",
            "projects/ores.refdata/modeling/ores.refdata.currency.org:10: "
            "refdata.v1.events.currencies.renamed: the event action "
            "'renamed' is not one of created, updated, deleted",
            "projects/ores.refdata/modeling/ores.refdata.currency.org:11: "
            "refdata.v1.Currencies.get: the resource segment 'Currencies' is "
            "not lower snake_case",
            "projects/ores.refdata/modeling/ores.refdata.currency.org:12: "
            "refdata.currencies.get: 3 segments, the grammar has four, or "
            "five for an event"});
}

TEST_CASE("a written subject is read from the data-quality seed alone",
    "[codecs][subject_conformance]") {
    const files t{
        {"projects/ores.refdata/modeling/ores.refdata.currency.org",
         ":subject: refdata.v1.currencies.get\n"},
        {"projects/ores.iam/src/a.cpp",
         "constexpr auto nats_subject = \"iam.v1.users.fetch\";\n"
         "auto step_subject =\n    \"iam.v1.users.grab'\";\n"},
        {"projects/ores.sql/populate/dq/dq_artefact_types_populate.sql",
         "insert into t values ('dq.v1.datasets.load', 'x');\n"},
        {"projects/ores.sql/populate/other.sql",
         "insert into t values ('dq.v1.datasets.load');\n"}};
    CHECK(messages(tests::run(codecs::subject_conformance, t)) ==
        std::vector<std::string>{
            "projects/ores.sql/populate/dq/dq_artefact_types_populate.sql:1: "
            "dq.v1.datasets.load: the verb 'load' is not one of the closed "
            "set of eight and is not in the reserved ops namespace"});
}

TEST_CASE("no declared subject is a finding, not a pass",
    "[codecs][subject_conformance]") {
    const files t{{"projects/ores.a/modeling/ores.a.b.org", "* Nothing\n"}};
    const auto r = tests::run(codecs::subject_conformance, t);
    REQUIRE(r.findings.size() == 1);
    CHECK(std::get<0>(r.findings[0]) == "projects");
}

TEST_CASE("an edit that drops the only declared subject is a finding, and "
    "one that brings it back clears it", "[codecs][subject_conformance]") {
    const files declared{{"projects/ores.a/modeling/ores.a.b.org",
        ":subject: a.v1.books.get\n"}};
    const files none{{"projects/ores.a/modeling/ores.a.b.org", "* Nothing\n"}};

    CHECK(messages(tests::run_after_edit(codecs::subject_conformance,
        declared, none)) == std::vector<std::string>{"projects:0: no "
            ":subject: declaration found; the scan is not reading the "
            "models"});
    CHECK(tests::run_after_edit(codecs::subject_conformance, none, declared)
        .findings.empty());
}
