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

#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.db/io/schema.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "database.hpp"

namespace {

using namespace compass::db;

/**
 * @brief The migrations of each released compass, by version, and the
 * blob ID of the SQL each released with. A released migration is never
 * edited or removed, since a database may already have applied it; a
 * change to the schema is a new migration. A release adds the migrations
 * it ships here.
 */
const std::map<std::uint32_t, std::string> released{
    {1, "2041f9dd93535db458d9539aca852c864b1a6fda"},
    {2, "93311482ee2f7160c89ae77e54066b26214eac02"}};

/**
 * @brief The blob ID of bookkeeping_sql() as v0.0.13 released it. Every
 * database has run it, and it creates its tables only when they are
 * missing, so an edit to it reaches no existing database.
 */
constexpr std::string_view released_bookkeeping =
    "65f12634324d78166a2199825c4e183306d8456c";

/**
 * @brief The tables the last release retains: their rows exist nowhere
 * else. Pinned here rather than read from schema.sql, so a model that
 * drops the retained profile does not drop its table from the check too.
 * A release adds the retained tables it ships.
 */
const std::set<std::string> released_retained{
    "activity.attached", "activity.attachment", "activity.campaign",
    "activity.note", "activity.run", "activity.step_result"};

/**
 * @brief The columns of retained tables that a migration since the last
 * release may drop or retype, each as table.column with the :ID: of the
 * document that records the decision to lose its values.
 */
const std::map<std::string, std::string> allowed_losses{};

/**
 * @brief One row in every table of the last released schema, every
 * column holding a value, so the migrations since the release run as they
 * would on a database in use.
 */
constexpr std::string_view released_rows = R"sql(
INSERT INTO compass.setting (key, value) VALUES ('text_search', 'simple');
INSERT INTO corpus.document (hash, org_id, title, description, type, level,
    audience, created, updated, tags, keywords, properties, source)
VALUES ('\x01', 'D1', 'Title', 'Description', 'story', 'component',
    'developer', '2026-01-01', '2026-01-02', '{a}', '{"k": 1}', '{"p": 2}',
    '| State | done |');
INSERT INTO corpus.heading (hash, ord, level, title, path, anchor, org_id,
    todo, priority, tags, properties)
VALUES ('\x01', 0, 1, 'Heading', '{Heading}', 'heading', 'H1', 'TODO', 'A',
    '{t}', '{"p": 3}');
INSERT INTO corpus.section (hash, ord, heading_ord, text, tsv)
VALUES ('\x01', 0, 0, 'words', to_tsvector('simple', 'words'));
INSERT INTO corpus.link (hash, ord, heading_ord, type, path, target_id, text)
VALUES ('\x01', 0, 0, 'id', 'D1', 'D1', 'link');
INSERT INTO corpus.environment (name, kind, root, head, synced_at)
VALUES ('main', 'worktree', '/checkout', 'abc', '2026-01-03');
INSERT INTO corpus.placement (environment, path, hash)
SELECT id, 'doc/a.org', '\x01' FROM corpus.environment;
INSERT INTO test.scenario (hash, org_id, title, task_id, story_id, target,
    environment, definition)
VALUES ('\x01', 'SC1', 'Scenario', 'T1', 'S1', 'cli', 'main', '\x02');
INSERT INTO test.step (hash, ord, title, body)
VALUES ('\x01', 0, 'Step', 'Body');
INSERT INTO activity.campaign (environment, story_id, sprint_id, opened_at,
    exported_at)
VALUES ('main', 'S1', 'SP1', '2026-01-04', '2026-01-05');
INSERT INTO activity.run (campaign, scenario_id, definition, scenario_hash,
    tester, started_at, finished_at, outcome)
SELECT id, 'SC1', '\x02', '\x01', 'tester', '2026-01-06', '2026-01-07',
    'passed' FROM activity.campaign;
INSERT INTO activity.step_result (run, step_ord, outcome, notes)
SELECT id, 0, 'pass', 'notes' FROM activity.run;
INSERT INTO activity.note (campaign, scenario_id, step_ord, kind, text,
    author, created_at, state, reply, addressed_by, addressed_at)
SELECT id, 'SC1', 0, 'bug', 'text', 'author', '2026-01-08', 'addressed',
    'reply', 'fixer', '2026-01-09' FROM activity.campaign;
INSERT INTO activity.attachment (hash, media_type, bytes)
VALUES ('\x03', 'text/plain', 'bytes');
INSERT INTO activity.attached (attachment, run, step_ord, caption)
SELECT '\x03', id, 0, 'caption' FROM activity.run;
)sql";

std::string schema_sql() {
    std::ifstream is(COMPASS_DB_SCHEMA_SQL, std::ios::binary);
    REQUIRE(is);
    return {std::istreambuf_iterator<char>(is), {}};
}

std::string hex_of(std::string_view sql) {
    return compass::store::ops::to_hex(compass::store::ops::blob_id_of(sql));
}

constexpr std::string_view user_schemas =
    "nspname NOT IN ('pg_catalog', 'information_schema') AND "
    "nspname NOT LIKE 'pg\\_%'";

/**
 * @brief The lowest object ID PostgreSQL gives an object a user creates.
 */
constexpr std::string_view first_user_oid = "16384";

/**
 * @brief The tables of the user schemas, schema-qualified, but for
 * compass.migration, whose rows say when each migration ran.
 */
std::vector<std::string> tables_of(io::connection& c) {
    const auto r = c.exec("SELECT nspname || '.' || relname FROM pg_class c "
        "JOIN pg_namespace n ON n.oid = c.relnamespace "
        "WHERE relkind IN ('r', 'p') AND "
        "NOT (nspname = 'compass' AND relname = 'migration') AND " +
        std::string(user_schemas) + " ORDER BY 1");
    std::vector<std::string> out;
    for (std::size_t row = 0; row < r.rows(); ++row)
        out.emplace_back(r.value(row, 0));
    return out;
}

/**
 * @brief Every object of the database's own schemas as a line of text:
 * schemas; relations with their persistence, row security, storage
 * parameters, privileges and view definition; the columns of every kind
 * of relation in their order with type, collation, nullability,
 * identity, generation, default and privileges; sequences with their
 * options; constraints, indexes, functions, triggers, policies, types
 * with their enum labels, comments, default privileges and extensions;
 * and the rows of every table. Two databases with the same lines have
 * the same catalog and the same data.
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
    const std::string user(first_user_oid);
    add("SELECT 'schema', nspname, coalesce(nspacl::text, '') "
        "FROM pg_namespace WHERE " + where);
    add("SELECT 'relation', nspname, relname, relkind, relpersistence, "
        "relrowsecurity, relforcerowsecurity, "
        "coalesce(reloptions::text, ''), coalesce(relacl::text, ''), "
        "CASE WHEN relkind IN ('v', 'm') THEN pg_get_viewdef(c.oid) "
        "ELSE '' END FROM pg_class c "
        "JOIN pg_namespace n ON n.oid = c.relnamespace WHERE " + where);
    add("SELECT 'column', nspname || '.' || relname, row_number() OVER "
        "(PARTITION BY c.oid ORDER BY a.attnum), attname, "
        "format_type(atttypid, atttypmod), coalesce(co.collname, ''), "
        "attnotnull, attidentity, attgenerated, "
        "coalesce(pg_get_expr(d.adbin, d.adrelid), ''), "
        "coalesce(attacl::text, '') "
        "FROM pg_attribute a JOIN pg_class c ON c.oid = a.attrelid "
        "JOIN pg_namespace n ON n.oid = c.relnamespace "
        "LEFT JOIN pg_attrdef d ON d.adrelid = a.attrelid "
        "AND d.adnum = a.attnum "
        "LEFT JOIN pg_collation co ON co.oid = a.attcollation "
        "WHERE c.relkind IN ('r', 'p', 'v', 'm', 'f', 'c') "
        "AND a.attnum > 0 AND NOT a.attisdropped AND " + where);
    add("SELECT 'sequence', nspname || '.' || relname, "
        "format_type(seqtypid, NULL), seqstart, seqincrement, seqmax, "
        "seqmin, seqcache, seqcycle FROM pg_sequence s "
        "JOIN pg_class c ON c.oid = s.seqrelid "
        "JOIN pg_namespace n ON n.oid = c.relnamespace WHERE " + where);
    add("SELECT 'constraint', conrelid::regclass::text, "
        "contypid::regtype::text, conname, contype, "
        "pg_get_constraintdef(o.oid) FROM pg_constraint o "
        "JOIN pg_namespace n ON n.oid = o.connamespace WHERE " + where);
    add("SELECT 'index', schemaname, indexname, indexdef FROM pg_indexes "
        "WHERE schemaname NOT IN ('pg_catalog', 'information_schema')");
    add("SELECT 'function', nspname, proname, pg_get_functiondef(p.oid), "
        "coalesce(proacl::text, '') FROM pg_proc p "
        "JOIN pg_namespace n ON n.oid = p.pronamespace WHERE " + where);
    add("SELECT 'trigger', t.tgrelid::regclass::text, tgname, tgenabled, "
        "pg_get_triggerdef(t.oid) FROM pg_trigger t "
        "JOIN pg_class c ON c.oid = t.tgrelid "
        "JOIN pg_namespace n ON n.oid = c.relnamespace "
        "WHERE NOT t.tgisinternal AND " + where);
    add("SELECT 'policy', schemaname, tablename, policyname, permissive, "
        "roles::text, cmd, coalesce(qual, ''), coalesce(with_check, '') "
        "FROM pg_policies WHERE schemaname NOT IN ('pg_catalog', "
        "'information_schema')");
    add("SELECT 'type', nspname, typname, typtype, "
        "coalesce(format_type(nullif(typbasetype, 0), typtypmod), ''), "
        "typnotnull, coalesce(typdefault, ''), coalesce(typacl::text, ''), "
        "coalesce((SELECT string_agg(enumlabel, ',' ORDER BY enumsortorder) "
        "FROM pg_enum e WHERE e.enumtypid = t.oid), '') FROM pg_type t "
        "JOIN pg_namespace n ON n.oid = t.typnamespace "
        "WHERE typtype IN ('b', 'd', 'e', 'm', 'r') AND " + where);
    add("SELECT 'comment', pg_describe_object(classoid, objoid, objsubid), "
        "description FROM pg_description WHERE objoid >= " + user);
    add("SELECT 'default privileges', defaclrole::regrole::text, "
        "defaclnamespace::regnamespace::text, defaclobjtype, "
        "defaclacl::text FROM pg_default_acl");
    add("SELECT 'extension', extname, extversion, nspname FROM pg_extension "
        "x JOIN pg_namespace n ON n.oid = x.extnamespace "
        "WHERE x.oid >= " + user);
    for (const auto& table : tables_of(c))
        add("SELECT 'row', '" + table + "', row_number() OVER "
            "(ORDER BY t::text), t::text FROM " + table + " t");
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
 * @brief Takes the new database of @p c to the last released schema as
 * that release did, recording each migration as migrate does, so a later
 * migrate applies only the migrations since.
 */
void migrate_to_release(io::connection& c) {
    const auto last = released.rbegin()->first;
    c.run_script(ops::bookkeeping_sql());
    for (const auto& m : ops::migrations()) {
        if (m.version > last)
            break;
        c.run_script(m.sql);
        c.exec("INSERT INTO compass.migration (version, name) "
            "VALUES ($1, $2)", {std::to_string(m.version),
                std::string(m.name)});
    }
}

/**
 * @brief The columns of @p table, by name, each with its type and
 * collation.
 */
std::map<std::string, std::string> columns_of(io::connection& c,
    const std::string& table) {
    const auto r = c.exec("SELECT attname, format_type(atttypid, atttypmod) "
        "|| ' ' || coalesce(co.collname, '') FROM pg_attribute a "
        "LEFT JOIN pg_collation co ON co.oid = a.attcollation "
        "WHERE attrelid = to_regclass($1) AND attnum > 0 AND "
        "NOT attisdropped", {table});
    std::map<std::string, std::string> out;
    for (std::size_t row = 0; row < r.rows(); ++row)
        out.emplace(r.value(row, 0), r.value(row, 1));
    return out;
}

/**
 * @brief The rows of @p table, sorted, each as the text of the values of
 * @p columns.
 */
std::vector<std::string> rows_of(io::connection& c, const std::string& table,
    const std::vector<std::string>& columns) {
    std::string values;
    for (const auto& column : columns)
        values += (values.empty() ? "\"" : ", \"") + column + "\"::text";
    const auto r = c.exec("SELECT row(" + values + ")::text FROM " + table +
        " ORDER BY 1");
    std::vector<std::string> out;
    for (std::size_t row = 0; row < r.rows(); ++row)
        out.emplace_back(r.value(row, 0));
    return out;
}

}

TEST_CASE("a released migration is never edited or removed",
    "[ops][migrations]") {
    const auto all = ops::migrations();
    for (const auto& [version, blob] : released) {
        INFO("migration " << version << " shipped in a release; a change "
            "to the schema is a new migration");
        const auto it = std::ranges::find(all, version,
            &ops::migration::version);
        REQUIRE(it != all.end());
        CHECK(hex_of(it->sql) == blob);
    }
    INFO("bookkeeping_sql() shipped in v0.0.13 and every database has run "
        "it; a change to the compass schema is a new migration");
    CHECK(hex_of(ops::bookkeeping_sql()) == released_bookkeeping);
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

TEST_CASE("the catalog comparison sees each kind of object a migration can "
    "make", "[ops][migrations][database]") {
    struct probe {
        std::string_view kind;
        std::string_view both;
        std::string_view extra;
    };
    constexpr probe probes[] = {
        {"trigger", "", "CREATE TRIGGER run_quiet BEFORE UPDATE ON "
            "activity.run FOR EACH ROW EXECUTE FUNCTION "
            "suppress_redundant_updates_trigger();"},
        {"row security", "",
            "ALTER TABLE activity.note ENABLE ROW LEVEL SECURITY;"},
        {"forced row security", "",
            "ALTER TABLE activity.note FORCE ROW LEVEL SECURITY;"},
        {"policy", "ALTER TABLE activity.note ENABLE ROW LEVEL SECURITY;",
            "CREATE POLICY note_none ON activity.note USING (false);"},
        {"collation", "", "ALTER TABLE activity.run ALTER COLUMN tester "
            "TYPE text COLLATE \"C\";"},
        {"identity option", "",
            "ALTER TABLE activity.run ALTER COLUMN id SET INCREMENT BY 7;"},
        {"storage parameter", "",
            "ALTER TABLE activity.campaign SET (fillfactor = 50);"},
        {"persistence", "CREATE TABLE activity.probe (x integer);",
            "ALTER TABLE activity.probe SET UNLOGGED;"},
        {"comment", "",
            "COMMENT ON COLUMN activity.run.tester IS 'drifted';"},
        {"enum type", "", "CREATE TYPE activity.mood AS ENUM ('fine');"},
        {"enum label", "CREATE TYPE activity.mood AS ENUM ('fine');",
            "ALTER TYPE activity.mood ADD VALUE 'poor';"},
        {"domain default", "CREATE DOMAIN activity.positive AS integer;",
            "ALTER DOMAIN activity.positive SET DEFAULT 1;"},
        {"seed row", "", "INSERT INTO compass.setting (key, value) "
            "VALUES ('text_search', 'simple');"},
        {"table privilege", "", "REVOKE ALL ON activity.run FROM PUBLIC;"},
        {"column privilege", "",
            "GRANT SELECT (tester) ON activity.run TO PUBLIC;"},
        {"function privilege", "", "REVOKE EXECUTE ON FUNCTION "
            "corpus.sprint_members FROM PUBLIC;"},
        {"schema privilege", "",
            "GRANT USAGE ON SCHEMA activity TO PUBLIC;"},
        {"default privilege", "", "ALTER DEFAULT PRIVILEGES IN SCHEMA "
            "activity GRANT SELECT ON TABLES TO PUBLIC;"},
        {"view definition", "CREATE VIEW activity.probe AS SELECT 1 AS a;",
            "CREATE OR REPLACE VIEW activity.probe AS SELECT 2 AS a;"},
        {"view column", "CREATE VIEW activity.probe AS SELECT 1 AS a;",
            "ALTER VIEW activity.probe ALTER COLUMN a SET DEFAULT 2;"},
        {"partitioned table column", "CREATE TABLE activity.probe "
            "(x integer) PARTITION BY RANGE (x);",
            "ALTER TABLE activity.probe ALTER COLUMN x SET DEFAULT 1;"},
    };
    const auto sql = schema_sql();
    for (const auto& p : probes) {
        INFO("a " << p.kind << " only one side has: " << p.extra);
        const auto base = sql + "\n" + std::string(p.both) + "\n";
        CHECK(generated_catalog(base + std::string(p.extra)) !=
            generated_catalog(base));
    }
}

TEST_CASE("the migrations since the last release apply to a database that "
    "holds rows", "[ops][migrations][database]") {
    io::connection c(tests::fresh_database());
    migrate_to_release(c);
    c.run_script(released_rows);
    for (const auto& table : tables_of(c)) {
        INFO(table << " has no row; fix: add one to released_rows");
        CHECK(c.exec("SELECT count(*) FROM " + table).value(0, 0) != "0");
    }
    INFO("fix: a migration that adds a NOT NULL column gives it a default "
        "or fills the rows already there");
    REQUIRE_NOTHROW(io::migrate(c));
}

TEST_CASE("a migration since the last release keeps every row and column "
    "of a retained table, unless a recorded decision allows the loss",
    "[ops][migrations][database]") {
    io::connection c(tests::fresh_database());
    migrate_to_release(c);
    c.run_script(released_rows);

    for (const auto& table : retained_tables(schema_sql())) {
        if (columns_of(c, table).empty())
            continue;
        INFO(table << " is retained and was released; fix: add it to "
            "released_retained");
        CHECK(released_retained.contains(table));
    }

    std::map<std::string, std::map<std::string, std::string>> columns;
    std::map<std::string, std::vector<std::string>> kept;
    std::map<std::string, std::vector<std::string>> rows;
    for (const auto& table : released_retained) {
        columns[table] = columns_of(c, table);
        REQUIRE_FALSE(columns[table].empty());
        for (const auto& column : columns[table] | std::views::keys)
            if (!allowed_losses.contains(table + "." + column))
                kept[table].push_back(column);
        rows[table] = rows_of(c, table, kept[table]);
    }
    io::migrate(c);

    for (const auto& table : released_retained) {
        const auto after = columns_of(c, table);
        for (const auto& column : kept[table]) {
            INFO(table << " is retained: its rows exist nowhere else; fix: "
                "keep " << column << ", or record the decision to lose it "
                "and name it in allowed_losses");
            const auto it = after.find(column);
            REQUIRE(it != after.end());
            CHECK(it->second == columns[table][column]);
        }
        INFO(table << " is retained: a migration keeps each of its rows "
            "and their values");
        CHECK(rows_of(c, table, kept[table]) == rows[table]);
    }
}
