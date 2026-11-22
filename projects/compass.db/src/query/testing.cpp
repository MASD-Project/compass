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
#include "compass.db/query/testing.hpp"

#include <charconv>
#include <cstdint>
#include <string_view>
#include <utility>

namespace compass::db::types {

std::string_view to_string(scenario_state s) noexcept {
    switch (s) {
    case scenario_state::untested: return "untested";
    case scenario_state::failed: return "failed";
    case scenario_state::passed: return "passed";
    case scenario_state::changed: return "changed";
    }
    return "untested";
}

}

namespace compass::db::query {

namespace {

constexpr std::string_view utc_time =
    "'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"'";

struct sprint_scope {
    std::string environment_id;
    std::string synced_at;
    types::sprint_summary sprint;
};

/**
 * @brief The environment and the sprint @p r names inside @p v: by :ID:,
 * else by title, else by the name of its directory; or, when it names
 * none, the latest created of the STARTED sprints.
 */
sprint_scope find_sprint(io::connection& c, const types::view_filter& v,
    const types::sprint_request& r) {
    const auto e = c.exec(std::string(R"sql(
SELECT id, coalesce(to_char(synced_at AT TIME ZONE 'UTC', )sql") +
        std::string(utc_time) + R"sql(), '')
FROM corpus.environment WHERE name = $1
)sql", {r.environment});
    if (e.rows() == 0)
        throw unknown_environment(r.environment);
    sprint_scope out;
    out.environment_id = e.value(0, 0);
    out.synced_at = e.value(0, 1);

    const auto s = c.exec(R"sql(
SELECT d.org_id, coalesce(d.title, ''), coalesce(d.state, '')
FROM corpus.placement p JOIN corpus.document d ON d.hash = p.hash
WHERE p.environment = $1::bigint AND d.type = 'sprint'
  AND d.org_id IS NOT NULL
  AND ()sql" + v.condition + R"sql()
  AND CASE WHEN $2::text IS NULL THEN d.state = 'STARTED'
      ELSE d.org_id = $2 OR d.title = $2
        OR regexp_replace(p.path, '^(.*/)?([^/]+)/[^/]+$', '\2') = $2 END
ORDER BY d.org_id = $2 DESC NULLS LAST, d.title = $2 DESC NULLS LAST,
         d.created DESC NULLS LAST, d.title DESC, p.path
LIMIT 1
)sql", {out.environment_id, r.sprint});
    if (s.rows() == 0)
        throw unknown_sprint(r.sprint
            ? "no sprint named " + *r.sprint + " in " + r.environment
            : "no sprint is STARTED in " + r.environment);
    out.sprint.org_id = s.value(0, 0);
    out.sprint.title = s.value(0, 1);
    out.sprint.state = s.value(0, 2);
    return out;
}

/**
 * @brief The stories of the sprint $2 in the environment $1 inside the
 * view, as a WITH clause named story.
 */
std::string stories_of_sprint(const types::view_filter& v) {
    return R"sql(
story AS (
    SELECT DISTINCT ON (d.org_id) d.org_id, coalesce(d.title, '') AS title,
           coalesce(d.state, '') AS state, p.path
    FROM corpus.sprint_members($1::bigint, $2) m(hash)
    JOIN corpus.document d ON d.hash = m.hash
    JOIN corpus.placement p ON p.hash = d.hash AND p.environment = $1::bigint
    WHERE d.type = 'story' AND d.org_id IS NOT NULL AND ()sql" +
        v.condition + R"sql()
    ORDER BY d.org_id, p.path
))sql";
}

std::uint64_t to_number(std::string_view text) {
    std::uint64_t v = 0;
    std::from_chars(text.data(), text.data() + text.size(), v);
    return v;
}

std::int64_t to_id(std::string_view text) {
    std::int64_t v = 0;
    std::from_chars(text.data(), text.data() + text.size(), v);
    return v;
}

types::scenario_state state_of(const std::optional<types::run_summary>& r) {
    if (!r)
        return types::scenario_state::untested;
    if (r->outcome != "passed")
        return types::scenario_state::failed;
    return r->current_definition ? types::scenario_state::passed
                                 : types::scenario_state::changed;
}

}

types::testing_response what_needs_testing(io::connection& c,
    const types::view_filter& v, const types::sprint_request& r) {
    const auto scope = find_sprint(c, v, r);
    const auto rows = c.exec("WITH" + stories_of_sprint(v) + R"sql(,
scenario AS (
    SELECT DISTINCT ON (s.org_id) s.org_id, coalesce(s.title, '') AS title,
           s.story_id, coalesce(s.task_id, '') AS task_id, s.definition,
           p.path
    FROM test.scenario s
    JOIN corpus.placement p ON p.hash = s.hash AND p.environment = $1::bigint
    JOIN corpus.document d ON d.hash = s.hash
    WHERE s.org_id IS NOT NULL AND s.story_id IN (SELECT org_id FROM story)
      AND ()sql" + v.condition + R"sql()
    ORDER BY s.org_id, p.path
),
campaign AS (
    SELECT ca.id, ca.story_id,
           (SELECT count(*) FROM activity.note n
            WHERE n.campaign = ca.id AND n.state = 'open') AS open_notes
    FROM activity.campaign ca
    WHERE ca.environment = $3 AND ca.story_id IN (SELECT org_id FROM story)
),
latest AS (
    SELECT DISTINCT ON (r.scenario_id) r.scenario_id, r.id, r.tester,
           to_char(r.finished_at AT TIME ZONE 'UTC', )sql" +
        std::string(utc_time) + R"sql() AS finished_at,
           r.outcome, r.definition
    FROM activity.run r JOIN activity.campaign ca ON ca.id = r.campaign
    WHERE ca.environment = $3 AND r.outcome IS NOT NULL
      AND r.scenario_id IN (SELECT org_id FROM scenario)
    ORDER BY r.scenario_id, r.finished_at DESC, r.id DESC
),
scenario_notes AS (
    SELECT n.scenario_id, count(*) AS open_notes
    FROM activity.note n JOIN activity.campaign ca ON ca.id = n.campaign
    WHERE ca.environment = $3 AND n.state = 'open'
      AND n.scenario_id IN (SELECT org_id FROM scenario)
    GROUP BY n.scenario_id
)
SELECT st.org_id, st.title, st.state, st.path, ca.id,
       coalesce(ca.open_notes, 0), sc.org_id, sc.title, sc.path, sc.task_id,
       coalesce(sn.open_notes, 0), l.id, l.tester, l.finished_at, l.outcome,
       l.definition = sc.definition
FROM story st
LEFT JOIN campaign ca ON ca.story_id = st.org_id
LEFT JOIN scenario sc ON sc.story_id = st.org_id
LEFT JOIN latest l ON l.scenario_id = sc.org_id
LEFT JOIN scenario_notes sn ON sn.scenario_id = sc.org_id
ORDER BY st.path, st.org_id, sc.path, sc.org_id
)sql", {scope.environment_id, scope.sprint.org_id, r.environment});

    types::testing_response out;
    out.sprint = scope.sprint;
    out.synced_at = scope.synced_at;
    for (std::size_t row = 0; row < rows.rows(); ++row) {
        if (out.stories.empty() ||
            out.stories.back().org_id != rows.value(row, 0)) {
            types::story_testing s;
            s.org_id = rows.value(row, 0);
            s.title = rows.value(row, 1);
            s.state = rows.value(row, 2);
            s.path = rows.value(row, 3);
            if (!rows.is_null(row, 4))
                s.campaign = to_id(rows.value(row, 4));
            s.open_notes = to_number(rows.value(row, 5));
            out.stories.push_back(std::move(s));
        }
        if (rows.is_null(row, 6))
            continue;
        types::scenario_status sc;
        sc.org_id = rows.value(row, 6);
        sc.title = rows.value(row, 7);
        sc.path = rows.value(row, 8);
        sc.task_id = rows.value(row, 9);
        sc.open_notes = to_number(rows.value(row, 10));
        if (!rows.is_null(row, 11)) {
            types::run_summary run;
            run.id = to_id(rows.value(row, 11));
            run.tester = rows.value(row, 12);
            run.finished_at = rows.value(row, 13);
            run.outcome = rows.value(row, 14);
            run.current_definition = rows.value(row, 15) == "t";
            sc.latest_run = std::move(run);
        }
        sc.state = state_of(sc.latest_run);
        out.stories.back().scenarios.push_back(std::move(sc));
    }
    return out;
}

types::sprint_state_response sprint_state(io::connection& c,
    const types::view_filter& v, const types::sprint_request& r) {
    const auto scope = find_sprint(c, v, r);
    const auto rows = c.exec("WITH" + stories_of_sprint(v) + R"sql(,
task AS (
    SELECT DISTINCT ON (d.org_id) d.org_id, coalesce(d.title, '') AS title,
           coalesce(d.state, '') AS state, p.path, l.target_id AS story_id
    FROM corpus.link l
    JOIN corpus.document d ON d.hash = l.hash
    JOIN corpus.placement p ON p.hash = d.hash AND p.environment = $1::bigint
    WHERE l.heading_ord IS NULL AND l.target_id IN (SELECT org_id FROM story)
      AND d.type = 'task' AND d.org_id IS NOT NULL AND ()sql" +
        v.condition + R"sql()
    ORDER BY d.org_id, p.path, l.ord
)
SELECT st.org_id, st.title, st.state, st.path,
       t.org_id, t.title, t.state, t.path
FROM story st LEFT JOIN task t ON t.story_id = st.org_id
ORDER BY st.path, st.org_id, t.path, t.org_id
)sql", {scope.environment_id, scope.sprint.org_id});

    types::sprint_state_response out;
    out.sprint = scope.sprint;
    out.synced_at = scope.synced_at;
    const auto item = [&](std::size_t row, std::size_t at) {
        types::work_item w;
        w.org_id = rows.value(row, at);
        w.title = rows.value(row, at + 1);
        w.state = rows.value(row, at + 2);
        w.path = rows.value(row, at + 3);
        return w;
    };
    for (std::size_t row = 0; row < rows.rows(); ++row) {
        if (out.stories.empty() ||
            out.stories.back().story.org_id != rows.value(row, 0)) {
            types::story_state s;
            s.story = item(row, 0);
            out.stories.push_back(std::move(s));
        }
        if (!rows.is_null(row, 4))
            out.stories.back().tasks.push_back(item(row, 4));
    }
    return out;
}

}
