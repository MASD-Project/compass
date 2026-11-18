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
