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
#include "compass.db/query/campaign.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>
#include "compass.db/ops/run_outcome.hpp"
#include "compass.store/ops/blob_hash.hpp"

namespace compass::db::query {

namespace {

using reason = rejected_write::reason;

constexpr std::string_view utc_time =
    "'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"'";

/**
 * @brief The media types a screenshot may have: those a browser shows.
 */
constexpr std::array<std::string_view, 4> image_types{
    "image/png", "image/jpeg", "image/gif", "image/webp"};

[[noreturn]] void reject(reason why, const std::string& what) {
    throw rejected_write(why, what);
}

bool blank(std::string_view s) {
    return s.find_first_not_of(" \t\r\n") == std::string_view::npos;
}

void require_tester(std::string_view tester) {
    if (blank(tester))
        reject(reason::invalid, "a write names its tester; fix: send the "
            "tester's name with the request");
}

std::string number(std::int64_t v) { return std::to_string(v); }

std::optional<std::string> number(std::optional<std::int64_t> v) {
    if (!v)
        return std::nullopt;
    return std::to_string(*v);
}

std::optional<std::string> number(std::optional<std::int32_t> v) {
    if (!v)
        return std::nullopt;
    return std::to_string(*v);
}

std::int64_t to_id(std::string_view text) {
    std::int64_t v = 0;
    std::from_chars(text.data(), text.data() + text.size(), v);
    return v;
}

/**
 * @brief @p bytes in bytea's text input form, \x and two hex digits a
 * byte.
 */
std::string bytea_text(std::string_view bytes) {
    constexpr std::string_view digits = "0123456789abcdef";
    std::string out = "\\x";
    out.reserve(2 + 2 * bytes.size());
    for (const auto b : bytes) {
        const auto u = static_cast<unsigned char>(b);
        out += digits[u >> 4];
        out += digits[u & 0xF];
    }
    return out;
}

/**
 * @brief The scenario document of run @p run, locked against a concurrent
 * write to the run, and whether the run has finished.
 */
struct locked_run {
    std::string scenario_hash;
    bool finished = false;
};

locked_run lock_run(io::connection& c, std::int64_t run) {
    const auto r = c.exec("SELECT encode(scenario_hash, 'hex'), "
        "finished_at IS NOT NULL FROM activity.run WHERE id = $1::bigint "
        "FOR UPDATE", {number(run)});
    if (r.rows() == 0)
        reject(reason::not_found, "no run " + number(run));
    return {std::string(r.value(0, 0)), r.value(0, 1) == "t"};
}

locked_run lock_unfinished_run(io::connection& c, std::int64_t run) {
    auto out = lock_run(c, run);
    if (out.finished)
        reject(reason::conflict, "run " + number(run) + " has finished");
    return out;
}

void require_step(io::connection& c, const std::string& scenario_hash,
    std::int32_t step_ord) {
    const auto r = c.exec("SELECT 1 FROM test.step WHERE hash = "
        "decode($1, 'hex') AND ord = $2::integer",
        {scenario_hash, std::to_string(step_ord)});
    if (r.rows() == 0)
        reject(reason::invalid, "the scenario has no step " +
            std::to_string(step_ord) + "; steps count from 0");
}

types::step_outcome step_outcome_of(std::string_view s) {
    if (const auto o = types::step_outcome_of(s))
        return *o;
    throw io::error("activity.step_result holds an unknown outcome " +
        std::string(s), "");
}

}

types::start_run_response start_run(io::connection& c,
    const types::start_run_request& r) {
    require_tester(r.tester);
    io::transaction tx(c);
    const auto e = c.exec("SELECT id FROM corpus.environment WHERE name = $1",
        {r.environment});
    if (e.rows() == 0)
        throw unknown_environment(r.environment);
    const std::string environment_id(e.value(0, 0));

    const auto s = c.exec(R"sql(
SELECT encode(s.hash, 'hex'), encode(s.definition, 'hex'), s.story_id
FROM test.scenario s
JOIN corpus.placement p ON p.hash = s.hash AND p.environment = $1::bigint
WHERE s.org_id = $2
ORDER BY p.path LIMIT 1
)sql", {environment_id, r.scenario_id});
    if (s.rows() == 0)
        reject(reason::not_found, "no scenario " + r.scenario_id + " in " +
            r.environment);
    if (s.is_null(0, 2))
        reject(reason::invalid, "scenario " + r.scenario_id + " names no "
            "parent story, so it has no campaign; fix: link its story in "
            "the Parent story row of its Scenario Info table");
    const std::string hash(s.value(0, 0));
    const std::string definition(s.value(0, 1));
    const std::string story(s.value(0, 2));

    c.exec(R"sql(
INSERT INTO activity.campaign (environment, story_id, sprint_id)
VALUES ($1, $2, (
    SELECT l.target_id FROM corpus.document st
    JOIN corpus.placement sp ON sp.hash = st.hash
        AND sp.environment = $3::bigint
    JOIN corpus.link l ON l.hash = st.hash AND l.heading_ord IS NULL
    JOIN corpus.document sd ON sd.org_id = l.target_id
        AND sd.type = 'sprint'
    WHERE st.org_id = $2
    ORDER BY l.ord LIMIT 1))
ON CONFLICT (environment, story_id) DO NOTHING
)sql", {r.environment, story, environment_id});
    const auto campaign = c.exec("SELECT id FROM activity.campaign "
        "WHERE environment = $1 AND story_id = $2", {r.environment, story});

    types::start_run_response out;
    out.campaign = to_id(campaign.value(0, 0));
    out.definition = definition;
    out.scenario_hash = hash;
    const auto run = c.exec(std::string(R"sql(
INSERT INTO activity.run (campaign, scenario_id, definition, scenario_hash,
    tester)
VALUES ($1::bigint, $2, decode($3, 'hex'), decode($4, 'hex'), $5)
RETURNING id, to_char(started_at AT TIME ZONE 'UTC', )sql") +
        std::string(utc_time) + ")",
        {number(out.campaign), r.scenario_id, definition, hash, r.tester});
    out.run = to_id(run.value(0, 0));
    out.started_at = run.value(0, 1);

    const auto steps = c.exec("SELECT ord, title, body FROM test.step "
        "WHERE hash = decode($1, 'hex') ORDER BY ord", {hash});
    for (std::size_t row = 0; row < steps.rows(); ++row) {
        types::run_step step;
        step.ord = static_cast<std::int32_t>(to_id(steps.value(row, 0)));
        step.title = steps.value(row, 1);
        step.body = steps.value(row, 2);
        out.steps.push_back(std::move(step));
    }
    tx.commit();
    return out;
}

void record_step_result(io::connection& c,
    const types::step_result_request& r) {
    require_tester(r.tester);
    io::transaction tx(c);
    const auto run = lock_unfinished_run(c, r.run);
    require_step(c, run.scenario_hash, r.step_ord);
    c.exec(R"sql(
INSERT INTO activity.step_result (run, step_ord, outcome, notes)
VALUES ($1::bigint, $2::integer, $3, $4)
ON CONFLICT (run, step_ord)
DO UPDATE SET outcome = excluded.outcome, notes = excluded.notes
)sql", {number(r.run), std::to_string(r.step_ord),
        std::string(types::to_string(r.outcome)), r.notes});
    tx.commit();
}

types::attach_response attach_screenshot(io::connection& c,
    const types::attach_request& r) {
    require_tester(r.tester);
    if (r.run.has_value() == r.note.has_value())
        reject(reason::invalid, "a screenshot is attached to a run or to a "
            "note, and to one only");
    if (r.step_ord && !r.run)
        reject(reason::invalid, "a screenshot names a step only on a run");
    if (!std::ranges::contains(image_types, r.media_type))
        reject(reason::invalid, "a screenshot is an image: image/png, "
            "image/jpeg, image/gif or image/webp, not '" + r.media_type +
            "'");
    if (r.bytes.empty())
        reject(reason::invalid, "the screenshot is empty");

    io::transaction tx(c);
    if (r.run) {
        const auto run = lock_run(c, *r.run);
        if (r.step_ord)
            require_step(c, run.scenario_hash, *r.step_ord);
    } else if (c.exec("SELECT 1 FROM activity.note WHERE id = $1::bigint "
                   "FOR UPDATE", {number(*r.note)}).rows() == 0) {
        reject(reason::not_found, "no note " + number(*r.note));
    }

    types::attach_response out;
    out.attachment = store::ops::to_hex(store::ops::blob_id_of(r.bytes));
    c.exec("INSERT INTO activity.attachment (hash, media_type, bytes) "
        "VALUES (decode($1, 'hex'), $2, $3::bytea) ON CONFLICT DO NOTHING",
        {out.attachment, r.media_type, bytea_text(r.bytes)});
    const std::vector<io::parameter> place{out.attachment, number(r.run),
        number(r.note), number(r.step_ord)};
    const auto found = c.exec(R"sql(
SELECT id FROM activity.attached
WHERE attachment = decode($1, 'hex') AND run IS NOT DISTINCT FROM $2::bigint
  AND note IS NOT DISTINCT FROM $3::bigint
  AND step_ord IS NOT DISTINCT FROM $4::integer
)sql", place);
    if (found.rows() != 0) {
        out.attached = to_id(found.value(0, 0));
        if (r.caption)
            c.exec("UPDATE activity.attached SET caption = $2 "
                "WHERE id = $1::bigint", {number(out.attached), r.caption});
    } else {
        auto values = place;
        values.push_back(r.caption);
        out.attached = to_id(c.exec(R"sql(
INSERT INTO activity.attached (attachment, run, note, step_ord, caption)
VALUES (decode($1, 'hex'), $2::bigint, $3::bigint, $4::integer, $5)
RETURNING id
)sql", values).value(0, 0));
    }
    tx.commit();
    return out;
}

std::int64_t add_note(io::connection& c, const types::note_kinds& kinds,
    const types::add_note_request& r) {
    require_tester(r.tester);
    if (!std::ranges::contains(kinds.names, r.kind)) {
        std::string known;
        for (const auto& k : kinds.names)
            known += (known.empty() ? "" : ", ") + k;
        reject(reason::invalid, "no note kind '" + r.kind + "'; the "
            "project's kinds are: " + (known.empty() ? "none" : known));
    }
    if (blank(r.text))
        reject(reason::invalid, "a note has text");
    if (r.step_ord && !r.scenario_id)
        reject(reason::invalid, "a note names a step only with its "
            "scenario");

    io::transaction tx(c);
    const auto campaign = c.exec("SELECT environment, story_id FROM "
        "activity.campaign WHERE id = $1::bigint", {number(r.campaign)});
    if (campaign.rows() == 0)
        reject(reason::not_found, "no campaign " + number(r.campaign));
    if (r.scenario_id) {
        const auto s = c.exec(R"sql(
SELECT encode(s.hash, 'hex') FROM test.scenario s
JOIN corpus.placement p ON p.hash = s.hash
JOIN corpus.environment e ON e.id = p.environment AND e.name = $1
WHERE s.org_id = $3 AND s.story_id = $2
ORDER BY p.path LIMIT 1
)sql", {std::string(campaign.value(0, 0)), std::string(campaign.value(0, 1)),
            *r.scenario_id});
        if (s.rows() == 0)
            reject(reason::invalid, "scenario " + *r.scenario_id + " is not "
                "a scenario of the campaign's story " +
                std::string(campaign.value(0, 1)) + " in " +
                std::string(campaign.value(0, 0)));
        if (r.step_ord)
            require_step(c, std::string(s.value(0, 0)), *r.step_ord);
    }
    const auto id = c.exec(R"sql(
INSERT INTO activity.note (campaign, scenario_id, step_ord, kind, text,
    author)
VALUES ($1::bigint, $2, $3::integer, $4, $5, $6)
RETURNING id
)sql", {number(r.campaign), r.scenario_id, number(r.step_ord), r.kind,
        r.text, r.tester});
    tx.commit();
    return to_id(id.value(0, 0));
}

void address_note(io::connection& c, const types::address_note_request& r) {
    require_tester(r.tester);
    io::transaction tx(c);
    const auto updated = c.exec(R"sql(
UPDATE activity.note
SET state = 'addressed', reply = $2, addressed_by = $3, addressed_at = now()
WHERE id = $1::bigint AND state = 'open'
)sql", {number(r.note), r.reply, r.tester});
    if (updated.affected() == 0) {
        if (c.exec("SELECT 1 FROM activity.note WHERE id = $1::bigint",
                {number(r.note)}).rows() == 0)
            reject(reason::not_found, "no note " + number(r.note));
        reject(reason::conflict, "note " + number(r.note) +
            " is already addressed");
    }
    tx.commit();
}

types::finish_run_response finish_run(io::connection& c,
    const types::finish_run_request& r) {
    require_tester(r.tester);
    io::transaction tx(c);
    const auto run = lock_unfinished_run(c, r.run);
    const auto steps = c.exec(R"sql(
SELECT st.ord, sr.outcome FROM test.step st
LEFT JOIN activity.step_result sr ON sr.run = $2::bigint
    AND sr.step_ord = st.ord
WHERE st.hash = decode($1, 'hex')
ORDER BY st.ord
)sql", {run.scenario_hash, number(r.run)});
    std::vector<std::int32_t> ords;
    std::vector<std::optional<types::step_outcome>> outcomes;
    for (std::size_t row = 0; row < steps.rows(); ++row) {
        ords.push_back(static_cast<std::int32_t>(to_id(steps.value(row, 0))));
        if (steps.is_null(row, 1))
            outcomes.emplace_back();
        else
            outcomes.emplace_back(step_outcome_of(steps.value(row, 1)));
    }
    const auto outcome = ops::derive_run_outcome(outcomes);
    if (!outcome) {
        std::string missing;
        for (const auto i : outcome.error())
            missing += (missing.empty() ? "" : ", ") +
                std::to_string(ords[static_cast<std::size_t>(i)]);
        reject(reason::invalid, "run " + number(r.run) + " has steps with no "
            "result and none failed or blocked: " + missing);
    }

    types::finish_run_response out;
    out.outcome = *outcome;
    const auto done = c.exec(std::string(R"sql(
UPDATE activity.run SET finished_at = now(), outcome = $2
WHERE id = $1::bigint
RETURNING to_char(finished_at AT TIME ZONE 'UTC', )sql") +
        std::string(utc_time) + ")",
        {number(r.run), std::string(types::to_string(out.outcome))});
    out.finished_at = done.value(0, 0);
    tx.commit();
    return out;
}

}
