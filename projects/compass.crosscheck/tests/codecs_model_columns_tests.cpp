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
#include <catch2/catch_test_macros.hpp>
#include "compass.crosscheck/codecs/entity_binary_types.hpp"
#include "entity_model.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using tests::files;
using tests::model_column;

const std::string path = "projects/ores.iam/modeling/ores.iam.role.org";

}

TEST_CASE("a row binds each column in its table's binary format",
    "[codecs][entity_binary_types]") {
    const auto columns =
        model_column("id", ":type: bigint\n:cpp_type: std::int64_t\n"
            ":primary_key: true\n") +
        model_column("size", ":type: bigint\n:cpp_type: int\n") +
        model_column("flag", ":type: text\n:cpp_type: bool\n") +
        model_column("stamp", ":type: timestamp\n"
            ":cpp_type: std::chrono::system_clock::time_point\n") +
        model_column("owner", ":type: uuid\n:cpp_type: std::string\n") +
        model_column("amount", ":type: numeric(38,12)\n:cpp_type: double\n") +
        model_column("ratio", ":type: double precision\n:cpp_type: double\n") +
        model_column("thing", ":type: text\n:cpp_type: my::thing\n") +
        model_column("shape", ":type: geometry\n:cpp_type: int\n");
    const files t{{path, tests::entity_model("iam", "role",
        "ores_iam_roles_tbl", columns)}};
    const auto r = tests::run(codecs::entity_binary_types, t);
    CHECK(tests::messages_of(r) == std::vector<std::string>{
        "the row binds 'id' but the member is not declared in the struct",
        "'size' holds int (integer 32) but ores_iam_roles_tbl creates it "
        "bigint (integer 64)",
        "'flag' holds bool (boolean) but ores_iam_roles_tbl creates it text "
        "(text)",
        "'stamp' holds db_timestamp (timestamptz) but ores_iam_roles_tbl "
        "creates it timestamp (timestamp)",
        "'thing' holds my::thing, which this check cannot place on the wire",
        "'shape' holds int but ores_iam_roles_tbl declares it geometry, a "
        "type this check cannot place on the wire"});
    CHECK(tests::places(r.findings).front() ==
        std::pair<std::string, std::uint32_t>{path, 20});
}

TEST_CASE("a row on a table its model does not create is not compared",
    "[codecs][entity_binary_types]") {
    const auto columns = model_column("id", ":type: uuid\n"
        ":cpp_type: boost::uuids::uuid\n:primary_key: true\n") +
        model_column("size", ":type: bigint\n:cpp_type: int\n");
    const files t{
        {path, tests::entity_model("iam", "role", "ores_iam_roles_vw",
            columns, {}, {}, ":ores.sql.schema.domain_entity_create.enabled: "
            "false\n")},
        {"projects/ores.iam/modeling/ores.iam.store.org",
         tests::entity_model("iam", "store", "ores_iam_stores_tbl", columns,
             {}, ":sql_only: true\n")}};
    CHECK(tests::run(codecs::entity_binary_types, t).findings.empty());
}
