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
#include "compass.sql/ops/sql_model.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace compass::sql;
using pairs = std::vector<std::pair<std::string, std::string>>;

}

TEST_CASE("the lexer splits code from comments, literals, bodies and "
    "meta-commands", "[ops][sql_model]") {
    const std::string_view text = "a /* b\n/* c */ */ 'd''e' \"f\" $q$h$q$ "
        "-- i\n  \\ir j.sql\nk\\l";
    std::vector<ops::token_kind> kinds;
    std::vector<std::string_view> bytes;
    for (const auto& t : ops::lex(text).tokens) {
        kinds.push_back(t.kind);
        bytes.push_back(ops::text_of(text, t));
    }
    using k = ops::token_kind;
    CHECK(kinds == std::vector<k>{k::word, k::block_comment, k::string,
        k::quoted_name, k::dollar_body, k::line_comment, k::meta, k::word,
        k::symbol, k::word});
    CHECK(bytes[6] == "\\ir j.sql");
    CHECK(ops::dollar_content(bytes[4]) == "h");
    CHECK(ops::string_value(bytes[2]) == "d'e");
}

TEST_CASE("includes are the meta-commands that include, in order, and "
    "none in a comment or a literal", "[ops][sql_model]") {
    const auto m = ops::model_of(
        "\\ir ./create/a.sql\n"
        "  \\i create/b.sql\n"
        "\\if :x\n"
        "\\include_relative 'c.sql'\n"
        "/*\n\\ir hidden.sql\n*/\n"
        "select '\n\\ir quoted.sql\n';\n"
        "\\include d.sql\n");
    std::vector<std::pair<bool, std::string>> found;
    for (const auto& in : m.includes)
        found.emplace_back(in.relative, in.path);
    CHECK(found == std::vector<std::pair<bool, std::string>>{
        {true, "./create/a.sql"}, {false, "create/b.sql"}, {true, "c.sql"},
        {false, "d.sql"}});
    CHECK(m.includes[1].offset == 21);
}

TEST_CASE("a table states its columns, their types and its key",
    "[ops][sql_model]") {
    const auto m = ops::model_of(
        "create table if not exists public.\"T\" (\n"
        "    id uuid not null,\n"
        "    \"Name\" character varying(20) default 'x',\n"
        "    tenant_id uuid references ores_t_tbl (id),\n"
        "    primary key (id, tenant_id),\n"
        "    constraint c check (length(\"Name\") > 0)\n"
        ");\n"
        "-- create table ghost (x int);\n"
        "create table plain (a int primary key);\n");
    REQUIRE(m.tables.size() == 2);
    const auto& t = m.tables[0];
    CHECK(t.name == "t");
    std::vector<std::pair<std::string, std::string>> columns;
    for (const auto& c : t.columns)
        columns.emplace_back(c.name, c.type);
    CHECK(columns == pairs{{"id", "uuid"},
        {"name", "character varying(20)"}, {"tenant_id", "uuid"}});
    CHECK(t.primary_key == std::vector<std::string>{"id", "tenant_id"});
    CHECK(m.tables[1].name == "plain");
    CHECK(m.tables[1].primary_key == std::vector<std::string>{"a"});
}

TEST_CASE("policies and row-level security say what they compare",
    "[ops][sql_model]") {
    const auto m = ops::model_of(
        "alter table t enable row level security;\n"
        "alter table if exists only s.u enable row level security;\n"
        "alter table v disable row level security;\n"
        "create policy p on t as restrictive for all using (\n"
        "    party_id = any(ores_iam_visible_party_ids_fn()));\n"
        "create policy w on t using (\n"
        "    ores_utility_system_tenant_id_fn() = "
        "ores_iam_current_tenant_id_fn() or tenant_id = x);\n"
        "create policy v on t using (ores_iam_current_tenant_id_fn ( ) "
        "= ores_utility_system_tenant_id_fn());\n"
        "create policy c on t using (tenant_id = x)\n"
        "-- or ores_iam_current_tenant_id_fn() = "
        "ores_utility_system_tenant_id_fn()\n"
        ";\n"
        "do $$ begin create policy d on u using (true); end $$;\n");
    CHECK(m.row_level_security == std::vector<std::string>{"t", "u"});
    REQUIRE(m.policies.size() == 5);
    CHECK(m.policies[0].restrictive);
    CHECK(m.policies[0].names_party_id);
    CHECK_FALSE(m.policies[1].restrictive);
    CHECK(m.policies[1].widens_to_system_tenant);
    CHECK(m.policies[2].widens_to_system_tenant);
    CHECK_FALSE(m.policies[3].widens_to_system_tenant);
    CHECK(m.policies[4].name == "d");
    CHECK(m.policies[4].table == "u");
}

TEST_CASE("a function states its parameters, the lookups it guards, the "
    "rows it inserts and the tables it writes", "[ops][sql_model]") {
    const auto m = ops::model_of(
        "create or replace function ores_book_fn(p_name text,\n"
        "    p_currency text) returns void as $body$\n"
        "begin\n"
        "  select 1 into v from currencies where code = p_currency\n"
        "    and tenant_id = p_tenant and kind = 'x';\n"
        "  if not found then\n"
        "    raise exception 'currency %  NOT\n found', p_currency;\n"
        "  end if;\n"
        "  delete from books where name = p_name;\n"
        "  raise exception 'not found again';\n"
        "  -- update ores_ghost_tbl set x = 1;\n"
        "  insert into books (name, Currency) values (p_name, "
        "upper(p_currency));\n"
        "  update ores_books_tbl set x = 1;\n"
        "  insert into ores_audit_tbl select 1;\n"
        "end;\n"
        "$body$ language plpgsql;\n"
        "create function plain_fn() returns int as $$ select 1 $$;\n");
    REQUIRE(m.functions.size() == 2);
    const auto& f = m.functions[0];
    CHECK(f.name == "ores_book_fn");
    CHECK(f.offset == 0);
    CHECK(f.parameters == std::vector<std::string>{"p_name", "p_currency"});
    REQUIRE(f.guards.size() == 2);
    CHECK(f.guards[0].table == "currencies");
    CHECK(f.guards[0].columns == pairs{{"code", "p_currency"},
        {"tenant_id", "p_tenant"}});
    CHECK(f.guards[1].table == "currencies");
    REQUIRE(f.inserts.size() == 1);
    CHECK(f.inserts[0].table == "books");
    CHECK(f.inserts[0].values == pairs{{"name", "p_name"},
        {"currency", "upper(p_currency)"}});
    CHECK(f.writes == std::vector<std::string>{"ores_books_tbl",
        "ores_audit_tbl"});
    CHECK(m.functions[1].name == "plain_fn");
    CHECK(m.functions[1].writes.empty());
}

TEST_CASE("a call of a *_fn with a literal argument is kept with each "
    "argument's value, in bodies too", "[ops][sql_model]") {
    const auto m = ops::model_of(
        "do $$ begin\n"
        "  perform ores_x_fn(ores_tenant_fn(), 'a', 'O''K');\n"
        "end $$;\n"
        "-- select ores_ghost_fn('c');\n"
        "select upper('d'), ores_y_fn(1, $t$e$t$), ores_z_fn( 'f' );\n"
        "select ores_q_fn('g';\n");
    REQUIRE(m.calls.size() == 2);
    CHECK(m.calls[0].function == "ores_x_fn");
    CHECK(m.calls[0].arguments == std::vector<std::optional<std::string>>{
        std::nullopt, "a", "O'K"});
    CHECK(m.calls[1].function == "ores_z_fn");
    CHECK(m.calls[1].arguments == std::vector<std::optional<std::string>>{
        "f"});
    REQUIRE(m.unbalanced.size() == 1);
    CHECK(m.unbalanced[0].call);
}
