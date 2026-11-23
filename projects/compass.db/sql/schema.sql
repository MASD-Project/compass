/* -*- sql-product: postgres; tab-width: 4; indent-tabs-mode: nil -*-
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
/*
 * AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Template: masd_sql_schema.sql.mustache
 * To modify, update the template and regenerate.
 */

CREATE SCHEMA corpus;

CREATE TABLE corpus.document (
    hash bytea PRIMARY KEY,
    org_id text,
    title text,
    description text,
    type text,
    level text,
    audience text,
    created text,
    updated text,
    tags text[] NOT NULL DEFAULT '{}',
    keywords jsonb NOT NULL DEFAULT '{}',
    properties jsonb NOT NULL DEFAULT '{}',
    source text NOT NULL,
    loaded_at timestamptz NOT NULL DEFAULT now(),
    state text GENERATED ALWAYS AS (substring(source from '(?n)^[ \t]*\|[ \t]*State[ \t]*\|[ \t]*([^|[:space:]]+)')) STORED
);
CREATE INDEX document_org_id ON corpus.document (org_id);
CREATE INDEX document_type ON corpus.document (type);
CREATE INDEX document_level ON corpus.document (level);
CREATE INDEX document_audience ON corpus.document (audience);
CREATE INDEX document_tags ON corpus.document USING gin (tags);
CREATE INDEX document_keywords ON corpus.document USING gin (keywords jsonb_path_ops);
CREATE INDEX document_properties ON corpus.document USING gin (properties jsonb_path_ops);
CREATE INDEX document_state ON corpus.document (state);

CREATE TABLE corpus.environment (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name text NOT NULL UNIQUE,
    kind text NOT NULL CHECK (kind IN ('worktree', 'branch', 'release')),
    root text,
    head text,
    synced_at timestamptz
);

CREATE TABLE corpus.heading (
    hash bytea NOT NULL REFERENCES corpus.document ON DELETE CASCADE,
    ord integer NOT NULL,
    level integer NOT NULL,
    title text NOT NULL,
    path text[] NOT NULL,
    anchor text NOT NULL,
    org_id text,
    todo text,
    priority text,
    tags text[] NOT NULL DEFAULT '{}',
    properties jsonb NOT NULL DEFAULT '{}',
    PRIMARY KEY (hash, ord)
);
CREATE INDEX heading_org_id ON corpus.heading (org_id) WHERE org_id is not null;

CREATE TABLE corpus.link (
    hash bytea NOT NULL REFERENCES corpus.document ON DELETE CASCADE,
    ord integer NOT NULL,
    heading_ord integer,
    type text NOT NULL,
    path text NOT NULL,
    target_id text,
    text text NOT NULL,
    PRIMARY KEY (hash, ord)
);
CREATE INDEX link_target_id ON corpus.link (target_id) WHERE target_id is not null;

CREATE TABLE corpus.placement (
    environment bigint NOT NULL REFERENCES corpus.environment ON DELETE CASCADE,
    path text NOT NULL,
    hash bytea NOT NULL REFERENCES corpus.document,
    PRIMARY KEY (environment, path)
);
CREATE INDEX placement_hash ON corpus.placement (hash);

CREATE TABLE corpus.section (
    hash bytea NOT NULL REFERENCES corpus.document ON DELETE CASCADE,
    ord integer NOT NULL,
    heading_ord integer,
    text text NOT NULL,
    tsv tsvector NOT NULL,
    PRIMARY KEY (hash, ord)
);
CREATE INDEX section_tsv ON corpus.section USING gin (tsv);

CREATE SCHEMA test;

CREATE TABLE test.scenario (
    hash bytea PRIMARY KEY REFERENCES corpus.document ON DELETE CASCADE,
    org_id text,
    title text,
    task_id text,
    story_id text,
    target text,
    environment text,
    definition bytea NOT NULL
);
CREATE INDEX scenario_org_id ON test.scenario (org_id);
CREATE INDEX scenario_story_id ON test.scenario (story_id);

CREATE TABLE test.step (
    hash bytea NOT NULL REFERENCES test.scenario ON DELETE CASCADE,
    ord integer NOT NULL,
    title text NOT NULL,
    body text NOT NULL,
    PRIMARY KEY (hash, ord)
);

CREATE SCHEMA activity;

-- activity.attachment is retained: its rows exist nowhere else, so a change
-- to it needs a migration.
CREATE TABLE activity.attachment (
    hash bytea PRIMARY KEY,
    media_type text NOT NULL,
    bytes bytea NOT NULL
);

-- activity.campaign is retained: its rows exist nowhere else, so a change
-- to it needs a migration.
CREATE TABLE activity.campaign (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    environment text NOT NULL,
    story_id text NOT NULL,
    sprint_id text,
    opened_at timestamptz NOT NULL DEFAULT now(),
    exported_at timestamptz,
    UNIQUE (environment, story_id)
);

-- activity.note is retained: its rows exist nowhere else, so a change
-- to it needs a migration.
CREATE TABLE activity.note (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    campaign bigint NOT NULL REFERENCES activity.campaign,
    scenario_id text,
    step_ord integer,
    kind text NOT NULL,
    text text NOT NULL,
    author text NOT NULL,
    created_at timestamptz NOT NULL DEFAULT now(),
    state text NOT NULL DEFAULT 'open' CHECK (state IN ('open', 'addressed')),
    reply text,
    addressed_by text,
    addressed_at timestamptz
);

-- activity.run is retained: its rows exist nowhere else, so a change
-- to it needs a migration.
CREATE TABLE activity.run (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    campaign bigint NOT NULL REFERENCES activity.campaign,
    scenario_id text NOT NULL,
    definition bytea NOT NULL,
    scenario_hash bytea NOT NULL,
    tester text NOT NULL,
    started_at timestamptz NOT NULL DEFAULT now(),
    finished_at timestamptz,
    outcome text CHECK (outcome IN ('passed', 'failed', 'blocked'))
);
CREATE INDEX run_scenario_hash ON activity.run (scenario_hash);
CREATE INDEX run_scenario_id ON activity.run (scenario_id);

-- activity.attached is retained: its rows exist nowhere else, so a change
-- to it needs a migration.
CREATE TABLE activity.attached (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    attachment bytea NOT NULL REFERENCES activity.attachment,
    run bigint REFERENCES activity.run ON DELETE CASCADE,
    note bigint REFERENCES activity.note ON DELETE CASCADE,
    step_ord integer,
    caption text,
    CHECK ((run is null) <> (note is null))
);

-- activity.step_result is retained: its rows exist nowhere else, so a change
-- to it needs a migration.
CREATE TABLE activity.step_result (
    run bigint NOT NULL REFERENCES activity.run ON DELETE CASCADE,
    step_ord integer NOT NULL,
    outcome text NOT NULL CHECK (outcome IN ('pass', 'fail', 'blocked', 'skipped')),
    notes text,
    PRIMARY KEY (run, step_ord)
);

CREATE FUNCTION corpus.sprint_members(environment bigint, sprint text)
RETURNS SETOF bytea LANGUAGE sql STABLE AS $$
    WITH page AS (
        SELECT d.hash FROM corpus.document d
        JOIN corpus.placement p ON p.hash = d.hash
            AND p.environment = sprint_members.environment
        WHERE d.org_id = sprint AND d.type = 'sprint'
    ),
    child AS (
        SELECT l.hash FROM corpus.link l
        WHERE l.target_id = sprint AND l.heading_ord IS NULL
          AND EXISTS (SELECT 1 FROM page)
    ),
    story AS (
        SELECT d.org_id FROM child c
        JOIN corpus.document d ON d.hash = c.hash AND d.type = 'story'
        JOIN corpus.placement p ON p.hash = d.hash
            AND p.environment = sprint_members.environment
        WHERE d.org_id IS NOT NULL
    )
    SELECT hash FROM page
    UNION
    SELECT hash FROM child
    UNION
    SELECT l.hash FROM story s
    JOIN corpus.link l ON l.target_id = s.org_id AND l.heading_ord IS NULL
$$;
