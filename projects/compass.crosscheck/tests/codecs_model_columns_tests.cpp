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
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using tests::files;

}

TEST_CASE("a row binds each column in its table's binary format",
    "[codecs][entity_binary_types]") {
    const files t{
        {"projects/ores.sql/create/iam/iam_roles_create.sql",
         "create table \"ores_iam_roles_tbl\" (\n"
         "  \"id\" uuid not null,\n  \"version\" integer not null,\n"
         "  \"amount\" numeric(38,12),\n  \"kind\" ores_kind_t,\n"
         "  \"size\" bigint\n);\n"
         "create or replace view ores_iam_roles_vw as select 1;\n"},
        {"projects/ores.iam/core/include/ores.iam.core/repository/role_entity.hpp",
         "// AUTO-GENERATED FILE\nstruct role_entity {\n"
         "    constexpr static const char* tablename = \"ores_iam_roles_tbl\";\n"
         "    static constexpr auto columns() {\n"
         "        query_builder::key(\"id\", &e::id),\n"
         "        query_builder::bind(\"version\", &e::version),\n"
         "        query_builder::bind(\"amount\",\n            &e::amount),\n"
         "        query_builder::bind_text(\"kind\", &e::kind),\n"
         "        query_builder::bind(\"size\", &e::size),\n"
         "        query_builder::bind(\"missing\", &e::id),\n"
         "    }\n"
         "    boost::uuids::uuid id;\n    std::int64_t version = 0;\n"
         "    std::optional<double> amount;\n    std::string kind;\n"
         "    int size{};\n};\n"},
        {"projects/ores.iam/core/include/ores.iam.core/repository/view_entity.hpp",
         "// AUTO-GENERATED FILE\n"
         "constexpr static const char* tablename = \"ores_iam_roles_vw\";\n"
         "static constexpr auto columns()\n"},
        {"projects/ores.iam/core/include/ores.iam.core/repository/lost_entity.hpp",
         "// AUTO-GENERATED FILE\n"
         "constexpr static const char* tablename = \"ores_iam_lost_tbl\";\n"
         "static constexpr auto columns()\n"}};
    CHECK(tests::messages_of(tests::run(codecs::entity_binary_types, t,
            {{"min_compared", "1"}})) == std::vector<std::string>{
        "no create script creates ores_iam_lost_tbl",
        "'version' holds std::int64_t (integer 64) but ores_iam_roles_tbl "
        "creates it integer (integer 32)",
        "'amount' holds std::optional<double> (double precision) but "
        "ores_iam_roles_tbl creates it numeric(38,12) (numeric)",
        "'size' holds int (integer 32) but ores_iam_roles_tbl creates it "
        "bigint (integer 64)",
        "the row binds 'missing', which ores_iam_roles_tbl does not create"});
}
