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
#include "compass.serve/query/reads.hpp"

#include <charconv>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>
#include "compass.db/query/scope.hpp"
#include "compass.serve/query/convert.hpp"

namespace compass::serve::query {

namespace {

using db::io::result;
using db::query::statement;

/// The columns of a document_summary of a document d at a placement p.
constexpr std::string_view summary_columns =
    "coalesce(d.org_id, ''), coalesce(d.title, ''), coalesce(d.type, ''), "
    "coalesce(d.description, ''), p.path, array_to_string(d.tags, E'\\x1f'), "
    "coalesce(d.updated, '')";

std::vector<std::string> split_units(std::string_view joined) {
    std::vector<std::string> out;
    if (joined.empty())
        return out;
    for (;;) {
        const auto at = joined.find('\x1f');
        out.emplace_back(joined.substr(0, at));
        if (at == std::string_view::npos)
            return out;
        joined.remove_prefix(at + 1);
    }
}

/// The summary of the document whose columns start at @p first.
messages::document_summary summary_at(const result& r, std::size_t row, std::size_t first) {
    messages::document_summary s;
    s.org_id = r.value(row, first);
    s.title = r.value(row, first + 1);
    s.type = r.value(row, first + 2);
    s.description = r.value(row, first + 3);
    s.path = r.value(row, first + 4);
    s.tags = split_units(r.value(row, first + 5));
    s.updated = r.value(row, first + 6);
    return s;
}

template <typename T>
T to_number(std::string_view text) {
    T v{};
    std::from_chars(text.data(), text.data() + text.size(), v);
    return v;
}

std::optional<std::string> optional_text(const result& r, std::size_t row, std::size_t col) {
    if (r.is_null(row, col))
        return std::nullopt;
    return std::string(r.value(row, col));
}

std::string utc(std::string_view column) {
    return "coalesce(to_char(" + std::string(column) +
        " AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"'), '')";
}

}

messages::list_documents_response list_documents(db::io::connection& c,
                                                 const db::types::view_filter& v,
                                                 const messages::list_documents_request& r) {
    const auto e = db::query::find_environment(c, r.environment);
    statement s;
    s.sql = "WITH listed AS (\nSELECT " + std::string(summary_columns) +
        "\nFROM corpus.placement p JOIN corpus.document d ON d.hash = p.hash\n";
    db::query::where_documents(s, e.id, v, to_db(r.filter));
    s.sql += ")\nSELECT (SELECT count(*) FROM listed), listed.* FROM listed\n"
        "ORDER BY 6 LIMIT " + s.bind(std::to_string(r.limit)) + "::bigint OFFSET " +
        s.bind(std::to_string(r.offset)) + "::bigint";
    const auto rows = c.exec(s.sql, s.parameters);
    messages::list_documents_response out;
    out.synced_at = e.synced_at;
    for (std::size_t row = 0; row < rows.rows(); ++row) {
        out.total = to_number<std::uint64_t>(rows.value(row, 0));
        out.documents.push_back(summary_at(rows, row, 1));
    }
    if (rows.rows() == 0 && r.offset > 0) {
        statement count;
        count.sql = "SELECT count(*) FROM corpus.placement p "
            "JOIN corpus.document d ON d.hash = p.hash\n";
        db::query::where_documents(count, e.id, v, to_db(r.filter));
        out.total = to_number<std::uint64_t>(c.exec(count.sql, count.parameters).value(0, 0));
    }
    return out;
}

std::optional<document_source> read_document(db::io::connection& c,
                                             const db::types::view_filter& v,
                                             const messages::get_document_request& r) {
    const auto e = db::query::find_environment(c, r.environment);
    statement s;
    s.sql = "SELECT " + std::string(summary_columns) + ", d.source, encode(d.hash, 'hex')\n"
        "FROM corpus.placement p JOIN corpus.document d ON d.hash = p.hash\n";
    db::query::where_documents(s, e.id, v, {});
    s.sql += "  AND d.org_id = " + s.bind(r.id) + "\nORDER BY p.path LIMIT 1";
    const auto found = c.exec(s.sql, s.parameters);
    if (found.rows() == 0)
        return std::nullopt;
    document_source out;
    out.summary = summary_at(found, 0, 0);
    out.source = found.value(0, 7);
    out.synced_at = e.synced_at;
    out.environment_id = e.id;
    const std::string hash(found.value(0, 8));

    // The documents the links of this one name, each once, with whether the
    // view sees the document that holds the :ID:.
    const auto targets = c.exec(R"sql(
SELECT DISTINCT l.target_id, t.org_id, t.visible
FROM corpus.link l
JOIN LATERAL (
    SELECT d.org_id, ()sql" + v.condition + R"sql() AS visible
    FROM corpus.placement p
    JOIN corpus.document d ON d.hash = p.hash
    WHERE p.environment = $2::bigint AND d.org_id IS NOT NULL
      AND (d.org_id = l.target_id OR EXISTS (
          SELECT 1 FROM corpus.heading h
          WHERE h.hash = d.hash AND h.org_id = l.target_id))
    ORDER BY p.path LIMIT 1) t ON true
WHERE l.hash = decode($1, 'hex') AND l.target_id IS NOT NULL
)sql", {hash, e.id});
    for (std::size_t row = 0; row < targets.rows(); ++row) {
        const std::string document(targets.value(row, 1));
        out.document_of.emplace(targets.value(row, 0), document);
        out.visible.emplace(document, targets.value(row, 2) == "t");
    }
    out.visible.emplace(out.summary.org_id, true);
    return out;
}

std::optional<std::pair<std::string, bool>> document_at(db::io::connection& c,
                                                        const db::types::view_filter& v,
                                                        const std::string& environment_id,
                                                        const std::string& path) {
    const auto r = c.exec("SELECT d.org_id, (" + v.condition + ")\n"
        "FROM corpus.placement p JOIN corpus.document d ON d.hash = p.hash\n"
        "WHERE p.environment = $1::bigint AND p.path = $2 AND d.org_id IS NOT NULL",
        {environment_id, path});
    if (r.rows() == 0)
        return std::nullopt;
    return std::pair{std::string(r.value(0, 0)), r.value(0, 1) == "t"};
}

std::optional<messages::backlinks_response> backlinks(db::io::connection& c,
                                                      const db::types::view_filter& v,
                                                      const messages::backlinks_request& r) {
    const auto e = db::query::find_environment(c, r.environment);
    statement target;
    target.sql = "SELECT 1 FROM corpus.placement p JOIN corpus.document d ON d.hash = p.hash\n";
    db::query::where_documents(target, e.id, v, {});
    target.sql += "  AND d.org_id = " + target.bind(r.id) + " LIMIT 1";
    if (c.exec(target.sql, target.parameters).rows() == 0)
        return std::nullopt;

    statement s;
    s.sql = "SELECT " + std::string(summary_columns) + ", coalesce(h.anchor, ''),\n"
        "       CASE WHEN l.text <> '' THEN l.text ELSE l.path END\n"
        "FROM corpus.link l\n"
        "JOIN corpus.placement p ON p.hash = l.hash\n"
        "JOIN corpus.document d ON d.hash = l.hash\n"
        "LEFT JOIN corpus.heading h ON h.hash = l.hash AND h.ord = l.heading_ord\n";
    db::query::where_documents(s, e.id, v, {});
    s.sql += "  AND l.target_id = " + s.bind(r.id) + "\nORDER BY p.path, l.ord";
    const auto rows = c.exec(s.sql, s.parameters);
    messages::backlinks_response out;
    out.synced_at = e.synced_at;
    for (std::size_t row = 0; row < rows.rows(); ++row) {
        messages::backlink b;
        b.document = summary_at(rows, row, 0);
        b.anchor = rows.value(row, 7);
        b.text = rows.value(row, 8);
        out.backlinks.push_back(std::move(b));
    }
    return out;
}

messages::list_environments_response list_environments(db::io::connection& c) {
    const auto rows = c.exec("SELECT e.name, e.kind, coalesce(e.root, ''), "
        "coalesce(e.head, ''), " + utc("e.synced_at") + ",\n"
        "       (SELECT count(*) FROM corpus.placement p WHERE p.environment = e.id)\n"
        "FROM corpus.environment e ORDER BY e.name");
    messages::list_environments_response out;
    for (std::size_t row = 0; row < rows.rows(); ++row) {
        messages::environment_summary s;
        s.name = rows.value(row, 0);
        assign_text(s.kind, rows.value(row, 1));
        s.root = rows.value(row, 2);
        s.head = rows.value(row, 3);
        s.synced_at = rows.value(row, 4);
        s.documents = to_number<std::uint64_t>(rows.value(row, 5));
        out.environments.push_back(std::move(s));
    }
    return out;
}

std::optional<messages::get_run_response> get_run(db::io::connection& c,
                                                  const messages::get_run_request& r) {
    static_cast<void>(db::query::find_environment(c, r.environment));
    const auto id = std::to_string(r.run);
    const auto run = c.exec("SELECT r.id, r.campaign, r.scenario_id, r.tester, "
        "encode(r.definition, 'hex'), " + utc("r.started_at") + ", " +
        utc("r.finished_at") + ", r.outcome\n"
        "FROM activity.run r JOIN activity.campaign ca ON ca.id = r.campaign\n"
        "WHERE r.id = $1::bigint AND ca.environment = $2", {id, r.environment});
    if (run.rows() == 0)
        return std::nullopt;
    messages::get_run_response out;
    out.id = to_number<std::int64_t>(run.value(0, 0));
    out.campaign = to_number<std::int64_t>(run.value(0, 1));
    out.scenario_id = run.value(0, 2);
    out.tester = run.value(0, 3);
    out.definition = run.value(0, 4);
    out.started_at = run.value(0, 5);
    if (!run.is_null(0, 7)) {
        messages::run_finish f;
        f.at = run.value(0, 6);
        assign_text(f.outcome, run.value(0, 7));
        out.finished = std::move(f);
    }
    const auto steps = c.exec(R"sql(
SELECT st.ord, st.title, st.body, sr.outcome, sr.notes
FROM activity.run r
JOIN test.step st ON st.hash = r.scenario_hash
LEFT JOIN activity.step_result sr ON sr.run = r.id AND sr.step_ord = st.ord
WHERE r.id = $1::bigint
ORDER BY st.ord
)sql", {id});
    for (std::size_t row = 0; row < steps.rows(); ++row) {
        messages::step_result s;
        s.ord = to_number<std::int32_t>(steps.value(row, 0));
        s.title = steps.value(row, 1);
        s.body = steps.value(row, 2);
        if (!steps.is_null(row, 3)) {
            s.outcome.emplace();
            assign_text(*s.outcome, steps.value(row, 3));
        }
        s.notes = optional_text(steps, row, 4);
        out.steps.push_back(std::move(s));
    }
    return out;
}

std::optional<messages::list_notes_response> list_notes(db::io::connection& c,
                                                        const messages::list_notes_request& r) {
    static_cast<void>(db::query::find_environment(c, r.environment));
    const auto id = std::to_string(r.campaign);
    if (c.exec("SELECT 1 FROM activity.campaign WHERE id = $1::bigint AND environment = $2",
               {id, r.environment})
            .rows() == 0)
        return std::nullopt;
    const auto rows = c.exec("SELECT n.id, n.scenario_id, n.step_ord, n.kind, n.text, "
        "n.author, " + utc("n.created_at") + ", n.addressed_by, " + utc("n.addressed_at") +
        ", n.reply\nFROM activity.note n WHERE n.campaign = $1::bigint ORDER BY n.id", {id});
    messages::list_notes_response out;
    for (std::size_t row = 0; row < rows.rows(); ++row) {
        messages::campaign_note n;
        n.id = to_number<std::int64_t>(rows.value(row, 0));
        n.scenario_id = optional_text(rows, row, 1);
        if (!rows.is_null(row, 2))
            n.step_ord = to_number<std::int32_t>(rows.value(row, 2));
        n.kind = rows.value(row, 3);
        n.text = rows.value(row, 4);
        n.author = rows.value(row, 5);
        n.created_at = rows.value(row, 6);
        if (!rows.is_null(row, 7)) {
            messages::note_address a;
            a.by = rows.value(row, 7);
            a.at = rows.value(row, 8);
            a.reply = optional_text(rows, row, 9);
            n.addressed = std::move(a);
        }
        out.notes.push_back(std::move(n));
    }
    return out;
}

}
