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
#include "compass.db/ops/migrations.hpp"

#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <catch2/catch_test_macros.hpp>
#include "compass.db/io/schema.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "database.hpp"

namespace {

using namespace compass::db;

/**
 * @brief The migrations of each released compass, by version, and the
 * blob ID of the SQL each released with. A released migration is never
 * edited, since a database may already have applied it; a change to the
 * schema is a new migration. A release adds the migrations it ships here.
 */
const std::map<std::uint32_t, std::string> released{
    {1, "2041f9dd93535db458d9539aca852c864b1a6fda"},
    {2, "93311482ee2f7160c89ae77e54066b26214eac02"}};

std::string schema_sql() {
    std::ifstream is(COMPASS_DB_SCHEMA_SQL, std::ios::binary);
    REQUIRE(is);
    return {std::istreambuf_iterator<char>(is), {}};
}

constexpr std::string_view user_schemas =
    "nspname NOT IN ('pg_catalog', 'information_schema') AND "
    "nspname NOT LIKE 'pg\\_%'";

/**
 * @brief Every object of the database's own schemas as a line of text:
 * schemas, relations, columns in their order with type, nullability,
 * identity, generation and default, constraints, indexes and functions.
 * Two databases with the same lines have the same catalog.
 */
std::set<std::string> catalog(io::connection& c) {
    std::set<std::string> out;
    const auto add = [&](std::string_view sql) {
        const auto r = c.exec(sql);
        for (std::size_t row = 0; row < r.rows(); ++row) {
            std::string line;
            for (std::size_t col = 0; col < r.columns(); ++col)
                line += (col ? " | " : "") + std::string(r.value(row, col));
            out.insert(std::move(line));
        }
    };
    const std::string where(user_schemas);
    add("SELECT 'schema', nspname FROM pg_namespace WHERE " + where);
    add("SELECT 'relation', nspname, relname, relkind FROM pg_class c "
        "JOIN pg_namespace n ON n.oid = c.relnamespace WHERE " + where);
    add("SELECT 'column', nspname || '.' || relname, row_number() OVER "
        "(PARTITION BY c.oid ORDER BY a.attnum), attname, "
        "format_type(atttypid, atttypmod), attnotnull, attidentity, "
        "attgenerated, coalesce(pg_get_expr(d.adbin, d.adrelid), '') "
        "FROM pg_attribute a JOIN pg_class c ON c.oid = a.attrelid "
        "JOIN pg_namespace n ON n.oid = c.relnamespace "
        "LEFT JOIN pg_attrdef d ON d.adrelid = a.attrelid "
        "AND d.adnum = a.attnum WHERE c.relkind = 'r' AND a.attnum > 0 "
        "AND NOT a.attisdropped AND " + where);
    add("SELECT 'constraint', conrelid::regclass::text, conname, contype, "
        "pg_get_constraintdef(o.oid) FROM pg_constraint o "
        "JOIN pg_namespace n ON n.oid = o.connamespace WHERE " + where);
    add("SELECT 'index', schemaname, indexname, indexdef FROM pg_indexes "
        "WHERE schemaname NOT IN ('pg_catalog', 'information_schema')");
    add("SELECT 'function', nspname, proname, pg_get_functiondef(p.oid) "
        "FROM pg_proc p JOIN pg_namespace n ON n.oid = p.pronamespace "
        "WHERE " + where);
    return out;
}

std::set<std::string> migrated_catalog() {
    io::connection c(tests::fresh_database());
    io::migrate(c);
    return catalog(c);
}

std::set<std::string> generated_catalog(const std::string& sql) {
    io::connection c(tests::fresh_database());
    c.run_script(ops::bookkeeping_sql());
    c.run_script(sql);
    return catalog(c);
}

std::string only_in(const std::set<std::string>& a,
    const std::set<std::string>& b) {
    std::string out;
    for (const auto& line : a)
        if (!b.contains(line))
            out += "\n  " + line;
    return out;
}

/**
 * @brief The tables schema.sql says are retained.
 */
std::set<std::string> retained_tables(const std::string& sql) {
    constexpr std::string_view opening = "-- ";
    constexpr std::string_view marker = " is retained:";
    std::set<std::string> out;
    std::size_t start = 0;
    while (start < sql.size()) {
        auto end = sql.find('\n', start);
        if (end == std::string::npos)
            end = sql.size();
        const std::string_view line(sql.data() + start, end - start);
        const auto at = line.find(marker);
        if (line.starts_with(opening) && at != std::string_view::npos)
            out.emplace(line.substr(opening.size(), at - opening.size()));
        start = end + 1;
    }
    return out;
}

/**
 * @brief The columns of @p table in @p catalog, each name with its type.
 */
std::map<std::string, std::string> columns_of(
    const std::set<std::string>& catalog, const std::string& table) {
    std::map<std::string, std::string> out;
    const auto prefix = "column | " + table + " | ";
    for (const auto& line : catalog) {
        if (!line.starts_with(prefix))
            continue;
        const auto rest = line.substr(prefix.size());
        const auto name_at = rest.find(" | ") + 3;
        const auto type_at = rest.find(" | ", name_at) + 3;
        const auto type_end = rest.find(" | ", type_at);
        out[rest.substr(name_at, type_at - 3 - name_at)] =
            rest.substr(type_at, type_end - type_at);
    }
    return out;
}

}

TEST_CASE("a released migration is never edited", "[ops][migrations]") {
    for (const auto& m : ops::migrations()) {
        const auto it = released.find(m.version);
        if (it == released.end())
            continue;
        INFO("migration " << m.version << " shipped in a release; a change "
            "to the schema is a new migration");
        CHECK(compass::store::ops::to_hex(
            compass::store::ops::blob_id_of(m.sql)) == it->second);
    }
}

TEST_CASE("the migrations build the catalog the generated schema.sql builds",
    "[ops][migrations][database]") {
    const auto migrated = migrated_catalog();
    const auto generated = generated_catalog(schema_sql());
    INFO("only the migrations build:" << only_in(migrated, generated));
    INFO("only schema.sql builds:" << only_in(generated, migrated));
    INFO("fix: a model change needs a new migration in src/ops/migrations.cpp "
        "that takes the last released schema to the one schema.sql states");
    CHECK(migrated == generated);
}

TEST_CASE("a retained table loses or retypes a column only through a "
    "migration", "[ops][migrations][database]") {
    const auto sql = schema_sql();
    const auto retained = retained_tables(sql);
    REQUIRE_FALSE(retained.empty());
    const auto migrated = migrated_catalog();
    const auto generated = generated_catalog(sql);
    for (const auto& table : retained) {
        const auto before = columns_of(migrated, table);
        const auto after = columns_of(generated, table);
        for (const auto& [column, type] : before) {
            const auto it = after.find(column);
            INFO(table << " is retained: its rows exist nowhere else; fix: "
                "add a migration that says what becomes of " << column);
            CHECK(it != after.end());
            if (it != after.end())
                CHECK(it->second == type);
        }
    }
}
