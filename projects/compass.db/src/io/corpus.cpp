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
#include "compass.db/io/corpus.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <string_view>
#include "compass.db/io/document_copy.hpp"
#include "compass.db/io/heading_copy.hpp"
#include "compass.db/io/link_copy.hpp"
#include "compass.db/io/placement_copy.hpp"
#include "compass.db/io/scenario_copy.hpp"
#include "compass.db/io/section_copy.hpp"
#include "compass.db/io/step_copy.hpp"
#include "compass.db/ops/copy_text.hpp"
#include "compass.store/ops/blob_hash.hpp"

namespace compass::db::io {

namespace {

using store::types::blob_id;

/**
 * @brief The key of the advisory lock a collection takes, so two
 * collections never sweep at once.
 */
constexpr std::string_view collection_lock = "4370616373737302";

blob_id from_hex(std::string_view hex) {
    blob_id id;
    for (std::size_t i = 0; i < id.bytes.size() && 2 * i + 1 < hex.size();
         ++i)
        std::from_chars(hex.data() + 2 * i, hex.data() + 2 * i + 2,
            id.bytes[i], 16);
    return id;
}

/**
 * @brief @p ids as a text[] literal of hex strings, which SQL decodes.
 */
std::string hex_array(std::span<const blob_id> ids) {
    std::string out = "{";
    for (const auto& id : ids) {
        if (out.size() > 1)
            out += ',';
        out += store::ops::to_hex(id);
    }
    return out + "}";
}

std::string text_array(std::span<const std::string> values) {
    std::string out = "{";
    for (const auto& v : values) {
        if (out.size() > 1)
            out += ',';
        out += '"';
        for (const char c : v) {
            if (c == '"' || c == '\\')
                out += '\\';
            out += c;
        }
        out += '"';
    }
    return out + "}";
}

std::int64_t to_id(std::string_view text) {
    std::int64_t v = 0;
    std::from_chars(text.data(), text.data() + text.size(), v);
    return v;
}

std::uint64_t to_number(std::string_view text) {
    std::uint64_t v = 0;
    std::from_chars(text.data(), text.data() + text.size(), v);
    return v;
}

/**
 * @brief The COPY text of every table a batch of documents fills.
 */
struct staged {
    std::string documents;
    std::string headings;
    std::string sections;
    std::string links;
    std::string scenarios;
    std::string steps;
};

staged stage(std::span<const types::corpus_document> documents) {
    staged s;
    ops::copy_writer document(s.documents);
    ops::copy_writer heading(s.headings);
    ops::copy_writer section(s.sections);
    ops::copy_writer link(s.links);
    ops::copy_writer scenario(s.scenarios);
    ops::copy_writer step(s.steps);
    for (const auto& d : documents) {
        write_copy(document, d.hash, d.document);
        for (std::size_t ord = 0; ord < d.headings.size(); ++ord)
            write_copy(heading, d.hash, ord, d.headings[ord]);
        for (std::size_t ord = 0; ord < d.sections.size(); ++ord)
            write_copy(section, d.hash, ord, d.sections[ord]);
        for (std::size_t ord = 0; ord < d.links.size(); ++ord)
            write_copy(link, d.hash, ord, d.links[ord]);
        if (!d.scenario)
            continue;
        write_copy(scenario, d.hash, *d.scenario);
        for (std::size_t ord = 0; ord < d.scenario->steps.size(); ++ord)
            write_copy(step, d.hash, ord, d.scenario->steps[ord]);
    }
    return s;
}

/**
 * @brief COPY of @p columns of @p table from standard input.
 */
std::string copy_into(std::string_view table, std::string_view columns) {
    return std::format("COPY {} ({}) FROM STDIN", table, columns);
}

/**
 * @brief Inserts the staged rows of a batch of documents, keeping every
 * row already present, and returns how many documents were new.
 */
std::uint64_t insert_documents(connection& c, const staged& s,
    const std::string& text_search) {
    c.run_script(R"sql(
CREATE TEMP TABLE stage_document (LIKE corpus.document INCLUDING DEFAULTS)
    ON COMMIT DROP;
CREATE TEMP TABLE stage_heading (LIKE corpus.heading) ON COMMIT DROP;
CREATE TEMP TABLE stage_section (hash bytea, ord integer, heading_ord integer,
    text text) ON COMMIT DROP;
CREATE TEMP TABLE stage_link (LIKE corpus.link) ON COMMIT DROP;
CREATE TEMP TABLE stage_scenario (LIKE test.scenario) ON COMMIT DROP;
CREATE TEMP TABLE stage_step (LIKE test.step) ON COMMIT DROP;
)sql");
    c.copy_in(copy_into("stage_document", document_copy_columns),
        s.documents);
    c.copy_in(copy_into("stage_heading", heading_copy_columns), s.headings);
    c.copy_in(copy_into("stage_section", section_copy_columns), s.sections);
    c.copy_in(copy_into("stage_link", link_copy_columns), s.links);
    c.copy_in(copy_into("stage_scenario", scenario_copy_columns),
        s.scenarios);
    c.copy_in(copy_into("stage_step", step_copy_columns), s.steps);
    const auto inserted = c.exec(R"sql(
INSERT INTO corpus.document (hash, org_id, title, description, type, level,
    audience, created, updated, tags, keywords, properties, source)
SELECT hash, org_id, title, description, type, level, audience, created,
       updated, tags, keywords, properties, source
FROM stage_document ON CONFLICT DO NOTHING
)sql").affected();
    c.exec("INSERT INTO corpus.heading SELECT * FROM stage_heading "
        "ON CONFLICT DO NOTHING");
    c.exec(R"sql(
INSERT INTO corpus.section (hash, ord, heading_ord, text, tsv)
SELECT s.hash, s.ord, s.heading_ord, s.text,
       setweight(to_tsvector($1::regconfig, coalesce(d.title, '')), 'A') ||
       setweight(to_tsvector($1::regconfig,
           coalesce(array_to_string(h.path, ' / '), '')), 'B') ||
       setweight(to_tsvector($1::regconfig, s.text), 'D')
FROM stage_section s JOIN stage_document d ON d.hash = s.hash
LEFT JOIN stage_heading h ON h.hash = s.hash AND h.ord = s.heading_ord
ON CONFLICT DO NOTHING
)sql", {text_search});
    c.exec("INSERT INTO corpus.link SELECT * FROM stage_link "
        "ON CONFLICT DO NOTHING");
    c.exec("INSERT INTO test.scenario SELECT * FROM stage_scenario "
        "ON CONFLICT DO NOTHING");
    c.exec("INSERT INTO test.step SELECT * FROM stage_step "
        "ON CONFLICT DO NOTHING");
    return inserted;
}

std::string text_search_of(connection& c) {
    const auto r = c.exec(
        "SELECT value FROM compass.setting WHERE key = 'text_search'");
    if (r.rows() == 0)
        throw error("the database has no text search configuration; fix: "
            "run compass db init", "");
    return std::string(r.value(0, 0));
}

}

std::vector<blob_id> present(connection& c, std::span<const blob_id> wanted) {
    std::vector<blob_id> out;
    if (wanted.empty())
        return out;
    const auto r = c.exec("SELECT encode(hash, 'hex') FROM corpus.document "
        "WHERE hash = ANY (SELECT decode(h, 'hex') FROM unnest($1::text[]) h)",
        {hex_array(wanted)});
    out.reserve(r.rows());
    for (std::size_t row = 0; row < r.rows(); ++row)
        out.push_back(from_hex(r.value(row, 0)));
    return out;
}

write_result write_environment(connection& c, const types::environment& e,
    std::span<const types::corpus_document> documents,
    std::span<const types::placement> placements,
    std::span<const std::string> scope) {
    write_result out;
    const auto text_search = text_search_of(c);
    transaction t(c);
    const auto id = std::string(c.exec(R"sql(
INSERT INTO corpus.environment (name, kind, root, head) VALUES ($1, $2, $3, $4)
ON CONFLICT (name) DO UPDATE SET kind = EXCLUDED.kind, root = EXCLUDED.root
RETURNING id
)sql", {e.name, std::string(types::to_string(e.kind)), e.root,
        e.head}).value(0, 0));

    std::string rows;
    {
        ops::copy_writer w(rows);
        const auto environment = to_id(id);
        for (const auto& p : placements)
            write_copy(w, environment, p);
    }
    c.run_script("CREATE TEMP TABLE stage_placement "
        "(LIKE corpus.placement) ON COMMIT DROP");
    c.copy_in(copy_into("stage_placement", placement_copy_columns), rows);

    std::vector<blob_id> given;
    given.reserve(documents.size());
    for (const auto& d : documents)
        given.push_back(d.hash);
    std::ranges::sort(given);
    const auto locked = c.exec("SELECT encode(hash, 'hex') FROM "
        "corpus.document WHERE hash IN (SELECT hash FROM stage_placement) "
        "FOR KEY SHARE");
    std::vector<blob_id> held;
    held.reserve(locked.rows());
    for (std::size_t row = 0; row < locked.rows(); ++row)
        held.push_back(from_hex(locked.value(row, 0)));
    std::ranges::sort(held);
    for (const auto& p : placements) {
        if (!std::ranges::binary_search(held, p.hash) &&
            !std::ranges::binary_search(given, p.hash) &&
            !std::ranges::contains(out.missing, p.hash))
            out.missing.push_back(p.hash);
    }
    if (!out.missing.empty()) {
        out.retry = true;
        return out;
    }

    if (!documents.empty())
        out.inserted = insert_documents(c, stage(documents), text_search);

    const bool scoped = !scope.empty();
    const auto paths = text_array(scope);
    std::string remove = "DELETE FROM corpus.placement p WHERE "
        "p.environment = $1 AND NOT EXISTS (SELECT 1 FROM stage_placement s "
        "WHERE s.path = p.path)";
    std::vector<parameter> with_scope{id};
    if (scoped) {
        remove += " AND p.path = ANY ($2::text[])";
        with_scope.emplace_back(paths);
    }
    out.removed = c.exec(remove, with_scope).affected();
    out.changed = c.exec(R"sql(
UPDATE corpus.placement p SET hash = s.hash FROM stage_placement s
WHERE p.environment = $1 AND p.path = s.path AND p.hash <> s.hash
)sql", {id}).affected();
    out.added = c.exec(R"sql(
INSERT INTO corpus.placement (environment, path, hash)
SELECT $1, s.path, s.hash FROM stage_placement s
ON CONFLICT (environment, path) DO NOTHING
)sql", {id}).affected();
    c.exec("UPDATE corpus.environment SET head = coalesce($2, head), "
        "synced_at = now() WHERE id = $1", {id, e.head});
    t.commit();
    return out;
}

collection_result collect(connection& c) {
    collection_result out;
    transaction t(c);
    const auto got = c.exec(std::format(
        "SELECT pg_try_advisory_xact_lock(x'{}'::bigint)", collection_lock));
    if (got.value(0, 0) != "t") {
        out.skipped = true;
        return out;
    }
    out.ran = true;
    out.removed = c.exec(R"sql(
WITH doomed AS (
    SELECT d.hash FROM corpus.document d
    WHERE NOT EXISTS (SELECT 1 FROM corpus.placement p WHERE p.hash = d.hash)
      AND NOT EXISTS (SELECT 1 FROM activity.run r
                      WHERE r.scenario_hash = d.hash)
    FOR UPDATE SKIP LOCKED
)
DELETE FROM corpus.document WHERE hash IN (SELECT hash FROM doomed)
)sql").affected();
    out.left = to_number(
        c.exec("SELECT count(*) FROM corpus.document").value(0, 0));
    c.exec("INSERT INTO compass.setting (key, value) VALUES "
        "('collection_left', $1) ON CONFLICT (key) DO UPDATE SET value = "
        "EXCLUDED.value", {std::to_string(out.left)});
    t.commit();
    return out;
}

collection_result collect_when_grown(connection& c) {
    const auto r = c.exec("SELECT (SELECT count(*) FROM corpus.document), "
        "(SELECT value FROM compass.setting WHERE key = 'collection_left')");
    const auto count = to_number(r.value(0, 0));
    if (count < collection_floor)
        return {};
    if (!r.is_null(0, 1) && count < 2 * to_number(r.value(0, 1)))
        return {};
    return collect(c);
}

std::vector<types::environment_summary> environments(connection& c) {
    const auto r = c.exec(R"sql(
SELECT e.name, e.kind, coalesce(e.root, ''), coalesce(e.head, ''),
       coalesce(to_char(e.synced_at AT TIME ZONE 'UTC',
                        'YYYY-MM-DD"T"HH24:MI:SS"Z"'), ''),
       (SELECT count(*) FROM corpus.placement p WHERE p.environment = e.id)
FROM corpus.environment e ORDER BY e.name
)sql");
    std::vector<types::environment_summary> out;
    for (std::size_t row = 0; row < r.rows(); ++row)
        out.push_back({std::string(r.value(row, 0)),
            std::string(r.value(row, 1)), std::string(r.value(row, 2)),
            std::string(r.value(row, 3)), std::string(r.value(row, 4)),
            to_number(r.value(row, 5))});
    return out;
}

bool drop_environment(connection& c, const std::string& name) {
    return c.exec("DELETE FROM corpus.environment WHERE name = $1", {name})
               .affected() != 0;
}

}
