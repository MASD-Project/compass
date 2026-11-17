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
#include "compass.crosscheck/codecs/entity_writes.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

}

TEST_CASE("a SQL function that writes an entity table is reported, the "
    "purge is not", "[codecs][entity_writes]") {
    const files t{
        {"projects/ores.sql/create/iam/a.sql",
         "-- header\n"
         "create or replace function ores_iam_grant_fn(x int)\n"
         "returns void as $$\nbegin\n"
         "  INSERT INTO ores_iam_roles_tbl values (x);\nend;\n$$;\n"
         "CREATE FUNCTION ores_iam_purge_tenant_fn() returns void as $$\n"
         "  delete from ores_refdata_currencies_tbl;\n$$;\n"
         "create function ores_iam_read_fn() returns int as $$\n"
         "  select 1 from ores_iam_roles_tbl;\n$$;\n"
         "create function ores_iam_body_fn() returns int as $$ select 1; $$;\n"
         "-- update ores_iam_roles_tbl outside a body\n"},
        {"projects/ores.sql/populate/b.sql",
         "create function ores_x_fn() as $$ update ores_x_tbl set a=1; $$;\n"}};
    CHECK(tests::places(tests::run(codecs::entity_writes, t).findings) ==
        places{{"projects/ores.sql/create/iam/a.sql", 2}});
}

TEST_CASE("a C++ file is excused only as the repository of every table it "
    "writes", "[codecs][entity_writes]") {
    const files t{
        {"projects/ores.iam/src/repository/role_repository.cpp",
         "auto q = \"insert into ores_iam_roles_tbl\";\n"},
        {"projects/ores.iam/src/repository/account_repository.cpp",
         "auto q = \"update ores_iam_accounts_tbl\";\n"
         "auto r = \"delete from ores_iam_roles_tbl\";\n"},
        {"projects/ores.iam/src/service/role_service.cpp",
         "// nothing here\nauto q = \"UPDATE ORES_IAM_ROLES_TBL set\";\n"},
        {"projects/ores.iam/tests/role_tests.cpp",
         "auto q = \"insert into ores_iam_roles_tbl\";\n"},
        {"projects/ores.testing/src/fixture.hpp",
         "auto q = \"insert into ores_iam_roles_tbl\";\n"},
        {"projects/ores.iam/src/service/reader.cpp",
         "auto q = \"select * from ores_iam_roles_tbl\";\n"}};
    CHECK(tests::places(tests::run(codecs::entity_writes, t).findings) ==
        places{{"projects/ores.iam/src/repository/account_repository.cpp", 1},
            {"projects/ores.iam/src/service/role_service.cpp", 2}});
}
