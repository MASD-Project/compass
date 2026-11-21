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
#include "compass.crosscheck/codecs/single_writer.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

const std::string publish =
    "create or replace function ores_refdata_publish_calendars_from_dq_fn()"
    " returns void as $$\nbegin\n"
    "    insert into ores_refdata_calendars_tbl (code) values ('USD');\n"
    "end;\n$$ language plpgsql;\n";

}

TEST_CASE("a table a populate script and a publish function both write is "
    "reported on the script's insert", "[codecs][single_writer]") {
    const files t{
        {"projects/ores.sql/create/refdata/refdata_publish_from_dq_create.sql",
         publish},
        {"projects/ores.sql/populate/refdata/seed_populate.sql",
         "-- seeds\n"
         "INSERT INTO \"ORES_REFDATA_CALENDARS_TBL\" (code) values ('A');\n"
         "insert into ores_refdata_calendars_tbl (code) values ('B');\n"
         "insert into ores_refdata_currencies_tbl (iso_code) values ('EUR');\n"
        }};
    CHECK(tests::places(tests::run(codecs::single_writer, t).findings) ==
        places{{"projects/ores.sql/populate/refdata/seed_populate.sql", 2}});
}

TEST_CASE("an insert in a comment writes nothing", "[codecs][single_writer]") {
    const files t{
        {"projects/ores.sql/create/refdata/refdata_publish_from_dq_create.sql",
         publish +
         "-- insert into ores_refdata_countries_tbl (code) values ('ZZ');\n"},
        {"projects/ores.sql/populate/seed_populate.sql",
         "/* insert into ores_refdata_calendars_tbl\n values ('A'); */\n"
         "insert into ores_refdata_countries_tbl (code) values ('ZZ');\n"
         "select 1; -- insert into ores_refdata_calendars_tbl"}};
    CHECK(tests::run(codecs::single_writer, t).findings.empty());
}

TEST_CASE("only publish functions and populate scripts count as writers",
    "[codecs][single_writer]") {
    const files t{
        {"projects/ores.sql/create/refdata/refdata_calendars_create.sql",
         "insert into ores_refdata_calendars_tbl (code) values ('X');\n"},
        {"projects/ores.sql/populate/seed_populate.sql",
         "insert into ores_refdata_calendars_tbl (code) values ('A');\n"}};
    CHECK(tests::run(codecs::single_writer, t).findings.empty());
}

TEST_CASE("a second writer is found after an edit makes the publish "
    "function write the table", "[codecs][single_writer]") {
    const files before{
        {"projects/ores.sql/create/refdata/refdata_publish_from_dq_create.sql",
         "select 1;\n"},
        {"projects/ores.sql/populate/seed_populate.sql",
         "insert into ores_refdata_calendars_tbl (code) values ('A');\n"}};
    auto after = before;
    after["projects/ores.sql/create/refdata/refdata_publish_from_dq_create.sql"]
        = publish;
    CHECK(tests::places(tests::run_after_edit(codecs::single_writer, before,
        after).findings) ==
        places{{"projects/ores.sql/populate/seed_populate.sql", 1}});
}
