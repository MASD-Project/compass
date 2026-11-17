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
#include "compass.sql/codecs/populate_refs.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::sql;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

files tree() {
    return {
        {"projects/ores.sql/create/a.sql",
         "create or replace function ores_upsert_currency_fn(p_code text, "
         "p_name text) returns void as $$\n"
         "begin\n"
         "  insert into currencies (code, name) values (p_code, p_name);\n"
         "end;\n"
         "$$ language plpgsql;\n"},
        {"projects/ores.sql/create/b.sql",
         "-- create or replace function commented_out(p_x text) as $$ $$\n"
         "create or replace function ores_upsert_book_fn(p_name text,\n"
         "    p_currency text) returns void as $body$\n"
         "begin\n"
         "  select 1 into v from currencies where code = p_currency\n"
         "    and tenant_id = p_tenant;\n"
         "  if not found then\n"
         "    raise exception 'currency % not found', p_currency;\n"
         "  end if;\n"
         "end;\n"
         "$body$ language plpgsql;\n"},
        {"projects/ores.sql/populate/1_currencies.sql",
         "select ores_upsert_currency_fn('USD', 'Dollar');\n"
         "select ores_upsert_currency_fn('O''K', 'Quoted');\n"},
        {"projects/ores.sql/populate/2_books.sql",
         "select ores_upsert_book_fn('main', 'USD');\n"
         "-- select ores_upsert_book_fn('old', 'XXX');\n"
         "select ores_upsert_book_fn('bad', 'EUR');\n"
         "select ORES_UPSERT_BOOK_FN('quoted', 'O''K');\n"
         "select ores_upsert_book_fn('dynamic', v_code);\n"}};
}

}

TEST_CASE("a lookup of a name no populate script defines is reported",
    "[codecs][populate_refs]") {
    const auto r = tests::run(codecs::populate_refs, tree());
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.sql/populate/2_books.sql", 3}});
    REQUIRE(r.findings.size() == 1);
    CHECK(std::get<2>(r.findings[0]) == "ores_upsert_book_fn(p_currency) "
        "looks up currencies (code) but no populate script defines (EUR)");
}

TEST_CASE("a call that does not close is a failure, not a pass",
    "[codecs][populate_refs]") {
    auto t = tree();
    t["projects/ores.sql/populate/2_books.sql"] =
        "select ores_upsert_book_fn('main', 'USD';\n";
    const auto r = tests::run(codecs::populate_refs, t);
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.sql/populate/2_books.sql", 1}});
}

TEST_CASE("no function definition is a failure, not a pass",
    "[codecs][populate_refs]") {
    const auto r = tests::run(codecs::populate_refs,
        {{"projects/ores.sql/populate/a.sql", "select 1;\n"}});
    CHECK(r.findings.size() == 1);
}

TEST_CASE("a lookup is resolved only by a definition psql runs before it",
    "[codecs][populate_refs]") {
    auto t = tree();
    t["projects/ores.sql/setup.sql"] =
        "\\ir populate/2_books.sql\n"
        "\\ir populate/1_currencies.sql\n";
    t["projects/ores.sql/populate/2_books.sql"] =
        "select ores_upsert_book_fn('main', 'USD');\n";
    auto r = tests::run(codecs::populate_refs, t);
    REQUIRE(r.findings.size() == 1);
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.sql/populate/2_books.sql", 1}});
    CHECK(std::get<2>(r.findings[0]) == "ores_upsert_book_fn(p_currency) "
        "looks up currencies (code) but (USD) is defined only by a populate "
        "call that psql runs after this one, or that no entry point "
        "reaches");

    t["projects/ores.sql/setup.sql"] =
        "\\ir populate/1_currencies.sql\n"
        "\\ir populate/2_books.sql\n";
    CHECK(tests::run(codecs::populate_refs, t).findings.empty());

    t["projects/ores.sql/populate/2_books.sql"] =
        "select ores_upsert_book_fn('main', 'GBP');\n"
        "select ores_upsert_currency_fn('GBP', 'Pound');\n";
    CHECK(tests::places(tests::run(codecs::populate_refs, t).findings) ==
        places{{"projects/ores.sql/populate/2_books.sql", 1}});
}

TEST_CASE("an edit to a populate script maps that script alone and finds "
    "what a full run finds", "[codecs][populate_refs]") {
    auto fixed = tree();
    fixed["projects/ores.sql/populate/1_currencies.sql"] +=
        "select ores_upsert_currency_fn('EUR', 'Euro');\n";
    const auto r = tests::run_after_edit(codecs::populate_refs, tree(),
        fixed);
    CHECK(r.findings.empty());
}

TEST_CASE("a function a lookup involves that is not named *_fn is "
    "reported, as its calls are not read", "[codecs][populate_refs]") {
    auto t = tree();
    t["projects/ores.sql/create/c.sql"] =
        "create or replace function upsert_rate(p_currency text) returns "
        "void as $$\n"
        "begin\n"
        "  select 1 into v from currencies where code = p_currency;\n"
        "  if not found then raise exception 'not found'; end if;\n"
        "end;\n"
        "$$ language plpgsql;\n";
    const auto r = tests::run(codecs::populate_refs, t);
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.sql/create/c.sql", 1},
        {"projects/ores.sql/populate/2_books.sql", 3}});
}
