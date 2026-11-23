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
#include "compass.db/query/search.hpp"

#include "compass.db/query/scope.hpp"
#include <charconv>
#include <cstdint>
#include <format>
#include <string_view>
#include <utility>
#include <vector>

namespace compass::db::query {

namespace {

std::uint64_t to_number(std::string_view text) {
    std::uint64_t v = 0;
    std::from_chars(text.data(), text.data() + text.size(), v);
    return v;
}

std::string float_array(const std::array<float, 4>& values) {
    return std::format("{{{},{},{},{}}}", values[0], values[1], values[2],
        values[3]);
}

std::string text_array(const std::vector<std::string>& values) {
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

/**
 * @brief The heading path as the query joins it, on the unit separator,
 * which no heading title holds.
 */
std::vector<std::string> split_path(std::string_view joined) {
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

}

types::search_response search(io::connection& c, const types::view_filter& v,
    const types::search_request& r, const types::ranking& how) {
    const auto e = find_environment(c, r.environment);
    statement s;
    const auto config = s.bind(e.text_search) + "::regconfig";
    s.sql = "WITH q AS (SELECT websearch_to_tsquery(" + config + ", " +
        s.bind(r.q) + ") AS query),\nranked AS (\nSELECT d.org_id, d.title, "
        "d.type, d.description, p.path, sc.hash, sc.ord, sc.heading_ord,\n"
        "       ts_rank_cd(" + s.bind(float_array(how.weights)) +
        "::float4[], sc.tsv, q.query, " +
        s.bind(std::to_string(how.normalization)) + "::integer) AS rank,\n"
        "       coalesce(array_position(" + s.bind(text_array(how.type_order)) +
        "::text[], d.type), 2147483647) AS type_rank\n"
        "FROM q, corpus.placement p\n"
        "JOIN corpus.document d ON d.hash = p.hash\n"
        "JOIN corpus.section sc ON sc.hash = d.hash\n";
    where_documents(s, e.id, v, r.filter);
    s.sql += "  AND sc.tsv @@ q.query\n),\npage AS (\n"
        "SELECT * FROM ranked ORDER BY rank DESC, type_rank, type, path, ord\n"
        "LIMIT " + s.bind(std::to_string(r.limit)) + "::bigint OFFSET " +
        s.bind(std::to_string(r.offset)) + "::bigint\n)\n";
    s.sql += R"sql(SELECT t.total, page.hash IS NOT NULL,
       coalesce(page.org_id, ''), coalesce(page.title, ''),
       coalesce(page.type, ''), coalesce(page.description, ''), page.path,
       coalesce(array_to_string(h.path, E'\x1f'), ''), coalesce(h.anchor, ''),
       ts_headline()sql" + config + R"sql(, sc.text, q.query,
                   'MaxFragments=2, MinWords=5, MaxWords=25'),
       page.rank
FROM (SELECT count(*) AS total FROM ranked) t
CROSS JOIN q
LEFT JOIN page ON true
LEFT JOIN corpus.section sc ON sc.hash = page.hash AND sc.ord = page.ord
LEFT JOIN corpus.heading h ON h.hash = page.hash AND h.ord = page.heading_ord
ORDER BY page.rank DESC, page.type_rank, page.type, page.path, page.ord
)sql";
    const auto rows = c.exec(s.sql, s.parameters);
    types::search_response out;
    out.synced_at = e.synced_at;
    for (std::size_t row = 0; row < rows.rows(); ++row) {
        out.total = to_number(rows.value(row, 0));
        if (rows.value(row, 1) != "t")
            continue;
        types::search_hit h;
        h.org_id = rows.value(row, 2);
        h.title = rows.value(row, 3);
        h.type = rows.value(row, 4);
        h.description = rows.value(row, 5);
        h.path = rows.value(row, 6);
        h.heading_path = split_path(rows.value(row, 7));
        h.anchor = rows.value(row, 8);
        h.snippet = rows.value(row, 9);
        const auto rank = rows.value(row, 10);
        std::from_chars(rank.data(), rank.data() + rank.size(), h.rank);
        out.hits.push_back(std::move(h));
    }
    return out;
}

types::facets_response facets(io::connection& c, const types::view_filter& v,
    const types::facets_request& r) {
    const auto e = find_environment(c, r.environment);
    statement s;
    const auto query = r.q.empty() ? std::string()
        : s.bind(e.text_search) + "::regconfig, " + s.bind(r.q);
    s.sql = "WITH matched AS MATERIALIZED (\n"
        "SELECT d.hash, d.type, d.tags, d.state, d.level,\n"
        "       d.audience, d.keywords, d.properties\n"
        "FROM corpus.placement p JOIN corpus.document d ON d.hash = p.hash\n";
    where_documents(s, e.id, v, r.filter);
    if (!query.empty())
        s.sql += "  AND EXISTS (SELECT 1 FROM corpus.section sc WHERE "
            "sc.hash = d.hash AND sc.tsv @@ websearch_to_tsquery(" + query +
            "))\n";
    const auto env = s.bind(e.id) + "::bigint";
    s.sql += R"sql()
SELECT 0, '', '', '', count(*) FROM matched
UNION ALL SELECT 1, '', type, '', count(*) FROM matched
    WHERE type IS NOT NULL GROUP BY type
UNION ALL SELECT 2, '', tag, '', count(*) FROM matched, unnest(tags) tag
    GROUP BY tag
UNION ALL SELECT 3, '', state, '', count(*) FROM matched
    WHERE state IS NOT NULL GROUP BY state
UNION ALL SELECT 4, '', sprint.org_id, sprint.title, count(*)
    FROM (SELECT DISTINCT sd.org_id, coalesce(sd.title, '') AS title
          FROM corpus.document sd
          JOIN corpus.placement sp ON sp.hash = sd.hash
          WHERE sd.type = 'sprint' AND sd.org_id IS NOT NULL
            AND sp.environment = )sql" + env + R"sql() sprint
    JOIN LATERAL corpus.sprint_members()sql" + env +
        R"sql(, sprint.org_id) member(hash) ON true
    JOIN matched ON matched.hash = member.hash
    GROUP BY sprint.org_id, sprint.title
UNION ALL SELECT 5, '', level, '', count(*) FROM matched
    WHERE level IS NOT NULL GROUP BY level
UNION ALL SELECT 6, '', audience, '', count(*) FROM matched
    WHERE audience IS NOT NULL GROUP BY audience
)sql";
    for (const auto& k : r.keywords) {
        const auto name = s.bind(k);
        s.sql += std::format("UNION ALL SELECT 7, {0}::text, value, '', "
            "count(*) FROM matched, jsonb_array_elements_text(keywords -> "
            "lower({0}::text)) value GROUP BY value\n", name);
    }
    for (const auto& p : r.properties) {
        const auto name = s.bind(p);
        s.sql += std::format("UNION ALL SELECT 8, {0}::text, properties ->> "
            "{0}::text, '', count(*) FROM matched WHERE properties ? "
            "{0}::text GROUP BY 3\n", name);
    }
    s.sql += "ORDER BY 1, 2, 5 DESC, 3\n";

    const auto rows = c.exec(s.sql, s.parameters);
    types::facets_response out;
    for (const auto& k : r.keywords)
        out.keywords.push_back({k, {}});
    for (const auto& p : r.properties)
        out.properties.push_back({p, {}});
    const auto named = [](std::vector<types::named_facet>& in,
                           std::string_view name) -> auto& {
        for (auto& f : in)
            if (f.name == name)
                return f.counts;
        return in.front().counts;
    };
    for (std::size_t row = 0; row < rows.rows(); ++row) {
        const auto kind = rows.value(row, 0);
        types::facet_count f;
        f.value = rows.value(row, 2);
        f.label = rows.value(row, 3);
        f.count = to_number(rows.value(row, 4));
        if (kind == "0") {
            out.documents = f.count;
            continue;
        }
        auto& into = [&]() -> std::vector<types::facet_count>& {
            switch (kind.front()) {
            case '1': return out.types;
            case '2': return out.tags;
            case '3': return out.states;
            case '4': return out.sprints;
            case '5': return out.levels;
            case '6': return out.audiences;
            case '7': return named(out.keywords, rows.value(row, 1));
            default: return named(out.properties, rows.value(row, 1));
            }
        }();
        into.push_back(std::move(f));
    }
    return out;
}

}
