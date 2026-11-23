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
#include "compass.db/query/campaign_export.hpp"

#include <charconv>
#include <map>
#include <string_view>
#include <utility>
#include "compass.db/query/search.hpp"

namespace compass::db::query {

namespace {

constexpr std::string_view utc_time =
    "'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"'";

std::string utc(std::string_view column) {
    return "to_char(" + std::string(column) + " AT TIME ZONE 'UTC', " +
        std::string(utc_time) + ")";
}

std::int64_t to_id(std::string_view text) {
    std::int64_t v = 0;
    std::from_chars(text.data(), text.data() + text.size(), v);
    return v;
}

std::optional<std::string> optional_text(const io::result& r,
    std::size_t row, std::size_t column) {
    if (r.is_null(row, column))
        return std::nullopt;
    return std::string(r.value(row, column));
}

std::optional<std::int32_t> optional_ord(const io::result& r,
    std::size_t row, std::size_t column) {
    if (r.is_null(row, column))
        return std::nullopt;
    return static_cast<std::int32_t>(to_id(r.value(row, column)));
}

/**
 * @brief The bytes of @p hex, two lower- or upper-case digits a byte.
 */
std::string from_hex(std::string_view hex) {
    const auto digit = [](char ch) {
        if (ch >= '0' && ch <= '9')
            return ch - '0';
        return (ch | 0x20) - 'a' + 10;
    };
    std::string out;
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2)
        out += static_cast<char>((digit(hex[i]) << 4) | digit(hex[i + 1]));
    return out;
}

void read_runs(io::connection& c, const std::string& campaign,
    const std::string& environment_id, types::campaign_rows& out) {
    const auto scenarios = c.exec(R"sql(
WITH ids AS (
    SELECT scenario_id AS id FROM activity.run WHERE campaign = $1::bigint
    UNION
    SELECT scenario_id FROM activity.note
    WHERE campaign = $1::bigint AND scenario_id IS NOT NULL)
SELECT ids.id, coalesce(
    (SELECT d.title FROM corpus.document d
     JOIN corpus.placement p ON p.hash = d.hash
         AND p.environment = $2::bigint
     WHERE d.org_id = ids.id AND d.type = 'test_scenario'
     ORDER BY p.path LIMIT 1),
    (SELECT d.title FROM activity.run r
     JOIN corpus.document d ON d.hash = r.scenario_hash
     WHERE r.campaign = $1::bigint AND r.scenario_id = ids.id
     ORDER BY r.id DESC LIMIT 1),
    ids.id) AS title
FROM ids ORDER BY title, ids.id
)sql", {campaign, environment_id});
    std::map<std::string, std::size_t, std::less<>> scenario_at;
    for (std::size_t row = 0; row < scenarios.rows(); ++row) {
        types::exported_scenario s;
        s.org_id = scenarios.value(row, 0);
        s.title = scenarios.value(row, 1);
        scenario_at.emplace(s.org_id, out.scenarios.size());
        out.scenarios.push_back(std::move(s));
    }

    const auto runs = c.exec("SELECT id, scenario_id, tester, " +
        utc("started_at") + ", " + utc("finished_at") + ", outcome, "
        "encode(definition, 'hex') FROM activity.run "
        "WHERE campaign = $1::bigint ORDER BY id", {campaign});
    std::map<std::int64_t, types::exported_run*> run_at;
    for (std::size_t row = 0; row < runs.rows(); ++row) {
        types::exported_run r;
        r.id = to_id(runs.value(row, 0));
        r.tester = runs.value(row, 2);
        r.started_at = runs.value(row, 3);
        if (!runs.is_null(row, 4)) {
            types::run_finish f;
            f.at = runs.value(row, 4);
            f.outcome = runs.value(row, 5);
            r.finished = std::move(f);
        }
        r.definition = runs.value(row, 6);
        auto& scenario =
            out.scenarios[scenario_at.find(runs.value(row, 1))->second];
        scenario.runs.push_back(std::move(r));
    }
    for (auto& s : out.scenarios)
        for (auto& r : s.runs)
            run_at.emplace(r.id, &r);

    const auto steps = c.exec(R"sql(
SELECT r.id, st.ord, st.title, sr.outcome, sr.notes
FROM activity.run r
JOIN test.step st ON st.hash = r.scenario_hash
LEFT JOIN activity.step_result sr ON sr.run = r.id AND sr.step_ord = st.ord
WHERE r.campaign = $1::bigint
ORDER BY r.id, st.ord
)sql", {campaign});
    for (std::size_t row = 0; row < steps.rows(); ++row) {
        types::exported_step s;
        s.ord = static_cast<std::int32_t>(to_id(steps.value(row, 1)));
        s.title = steps.value(row, 2);
        s.outcome = optional_text(steps, row, 3);
        s.notes = optional_text(steps, row, 4);
        run_at.at(to_id(steps.value(row, 0)))->steps.push_back(std::move(s));
    }

    const auto shots = c.exec(R"sql(
SELECT a.run, encode(a.attachment, 'hex'), a.step_ord, a.caption
FROM activity.attached a
JOIN activity.run r ON r.id = a.run
WHERE r.campaign = $1::bigint
ORDER BY a.id
)sql", {campaign});
    for (std::size_t row = 0; row < shots.rows(); ++row) {
        types::exported_screenshot s;
        s.attachment = shots.value(row, 1);
        s.step_ord = optional_ord(shots, row, 2);
        s.caption = optional_text(shots, row, 3);
        run_at.at(to_id(shots.value(row, 0)))->screenshots.push_back(
            std::move(s));
    }
}

void read_notes(io::connection& c, const std::string& campaign,
    types::campaign_rows& out) {
    const auto notes = c.exec("SELECT id, scenario_id, step_ord, kind, "
        "text, author, " + utc("created_at") + ", state = 'addressed', "
        "addressed_by, " + utc("addressed_at") + ", reply "
        "FROM activity.note WHERE campaign = $1::bigint ORDER BY id",
        {campaign});
    std::map<std::int64_t, std::size_t> note_at;
    for (std::size_t row = 0; row < notes.rows(); ++row) {
        types::exported_note n;
        n.id = to_id(notes.value(row, 0));
        n.scenario_id = optional_text(notes, row, 1);
        n.step_ord = optional_ord(notes, row, 2);
        n.kind = notes.value(row, 3);
        n.text = notes.value(row, 4);
        n.author = notes.value(row, 5);
        n.created_at = notes.value(row, 6);
        if (notes.value(row, 7) == "t") {
            types::note_address a;
            a.by = notes.value(row, 8);
            a.at = notes.value(row, 9);
            a.reply = optional_text(notes, row, 10);
            n.addressed = std::move(a);
        }
        note_at.emplace(n.id, out.notes.size());
        out.notes.push_back(std::move(n));
    }

    const auto shots = c.exec(R"sql(
SELECT a.note, encode(a.attachment, 'hex'), a.caption
FROM activity.attached a
JOIN activity.note n ON n.id = a.note
WHERE n.campaign = $1::bigint
ORDER BY a.id
)sql", {campaign});
    for (std::size_t row = 0; row < shots.rows(); ++row) {
        types::exported_screenshot s;
        s.attachment = shots.value(row, 1);
        s.caption = optional_text(shots, row, 2);
        out.notes[note_at.at(to_id(shots.value(row, 0)))].screenshots
            .push_back(std::move(s));
    }
}

void read_images(io::connection& c, const std::string& campaign,
    types::campaign_rows& out) {
    const auto images = c.exec(R"sql(
SELECT encode(m.hash, 'hex'), m.media_type, encode(m.bytes, 'hex')
FROM activity.attachment m
WHERE m.hash IN (
    SELECT a.attachment FROM activity.attached a
    JOIN activity.run r ON r.id = a.run WHERE r.campaign = $1::bigint
    UNION
    SELECT a.attachment FROM activity.attached a
    JOIN activity.note n ON n.id = a.note WHERE n.campaign = $1::bigint)
ORDER BY m.hash
)sql", {campaign});
    for (std::size_t row = 0; row < images.rows(); ++row) {
        types::exported_image i;
        i.attachment = images.value(row, 0);
        i.media_type = images.value(row, 1);
        i.bytes = from_hex(images.value(row, 2));
        out.images.push_back(std::move(i));
    }
}

}

std::vector<types::campaign_rows> read_campaigns(io::connection& c,
    const std::string& environment, const std::optional<std::string>& story) {
    io::transaction tx(c);
    c.exec("SET TRANSACTION ISOLATION LEVEL REPEATABLE READ, READ ONLY");
    const auto e = c.exec("SELECT id FROM corpus.environment WHERE name = $1",
        {environment});
    if (e.rows() == 0)
        throw unknown_environment(environment);
    const std::string environment_id(e.value(0, 0));

    const auto campaigns = c.exec("SELECT c.id, c.story_id, c.sprint_id, " +
        utc("c.opened_at") + R"sql(, coalesce(st.title, ''),
    coalesce(st.path, ''), coalesce((
        SELECT d.title FROM corpus.document d
        JOIN corpus.placement p ON p.hash = d.hash
            AND p.environment = $2::bigint
        WHERE d.org_id = c.sprint_id AND d.type = 'sprint'
        ORDER BY p.path LIMIT 1), c.sprint_id)
FROM activity.campaign c
LEFT JOIN LATERAL (
    SELECT d.title, p.path FROM corpus.document d
    JOIN corpus.placement p ON p.hash = d.hash
        AND p.environment = $2::bigint
    WHERE d.org_id = c.story_id AND d.type = 'story'
    ORDER BY p.path LIMIT 1) st ON true
WHERE c.environment = $1 AND ($3::text IS NULL OR c.story_id = $3)
ORDER BY c.id
)sql", {environment, environment_id, story});
    std::vector<types::campaign_rows> out;
    for (std::size_t row = 0; row < campaigns.rows(); ++row) {
        types::campaign_rows r;
        r.id = to_id(campaigns.value(row, 0));
        r.environment = environment;
        r.story_id = campaigns.value(row, 1);
        if (!campaigns.is_null(row, 2)) {
            types::page_ref sprint;
            sprint.org_id = campaigns.value(row, 2);
            sprint.title = campaigns.value(row, 6);
            r.sprint = std::move(sprint);
        }
        r.opened_at = campaigns.value(row, 3);
        r.story_title = campaigns.value(row, 4);
        r.story_path = campaigns.value(row, 5);
        const auto id = std::to_string(r.id);
        read_runs(c, id, environment_id, r);
        read_notes(c, id, r);
        read_images(c, id, r);
        out.push_back(std::move(r));
    }
    tx.commit();
    return out;
}

void mark_exported(io::connection& c, std::span<const std::int64_t> ids) {
    if (ids.empty())
        return;
    std::string array = "{";
    for (const auto id : ids)
        array += (array.size() > 1 ? "," : "") + std::to_string(id);
    array += "}";
    c.exec("UPDATE activity.campaign SET exported_at = now() "
        "WHERE id = ANY($1::bigint[])", {array});
}

}
