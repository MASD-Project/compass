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

#include <algorithm>
#include <deque>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.db/io/corpus.hpp"
#include "compass.db/io/schema.hpp"
#include "compass.db/ops/extract.hpp"
#include "compass.ingest/org/parser.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "database.hpp"

namespace {

using namespace compass::db;

/**
 * @brief Documents parsed and extracted as the sync does, kept alive with
 * the sources and tables their rows point into.
 */
class documents final {
public:
    types::corpus_document add(std::string source) {
        auto& text = sources_.emplace_back(std::move(source));
        auto& t = tables_.emplace_back();
        compass::ingest::org::parse(text, t);
        return ops::extract(compass::store::ops::blob_id_of(text), text,
            compass::org::types::view(t));
    }

private:
    std::deque<std::string> sources_;
    std::deque<compass::org::types::tables> tables_;
};

types::environment worktree(std::string name) {
    types::environment e;
    e.name = std::move(name);
    e.root = "/work/" + e.name;
    return e;
}

io::connection fresh() {
    io::connection c(tests::fresh_database());
    io::init(c, "english");
    return c;
}

void place(io::connection& c, const std::string& environment,
    const std::vector<types::corpus_document>& batch) {
    std::vector<types::placement> placed;
    for (std::size_t i = 0; i < batch.size(); ++i)
        placed.push_back({"doc" + std::to_string(i) + ".org", batch[i].hash});
    io::write_environment(c, worktree(environment), batch, placed);
}

const types::view_filter everything{"true"};

types::search_request request(std::string q, std::string environment = "main") {
    types::search_request r;
    r.environment = std::move(environment);
    r.q = std::move(q);
    return r;
}

std::vector<std::string> ids_of(const types::search_response& r) {
    std::vector<std::string> out;
    for (const auto& h : r.hits)
        out.push_back(h.org_id);
    return out;
}

/*
 * A sprint, a story in it, a task of the story, and two reference pages,
 * every one of which mentions a widget.
 */
std::vector<types::corpus_document> work_corpus(documents& docs) {
    return {
        docs.add(R"(:PROPERTIES:
:ID: SPRINT-1
:END:
#+title: Sprint 1
#+type: sprint
#+todo: STARTED | DONE

This page documents a sprint.

* Status

| Field | Value   |
|-------+---------|
| State | STARTED |

* Mission

Ship the widget.
)"),
        docs.add(R"(:PROPERTIES:
:ID: STORY-1
:END:
#+title: Story: Build the widget
#+type: story
#+filetags: :widget:sprint_01:
#+todo: BACKLOG STARTED | DONE

This page documents a story in [[id:SPRINT-1][Sprint 1]].

* Status

| Field | Value   |
|-------+---------|
| State | STARTED |

* Goal

A widget people use.
)"),
        docs.add(R"(:PROPERTIES:
:ID: TASK-1
:END:
#+title: Task: Paint the widget
#+type: task
#+level: s1
#+owner: marco
#+filetags: :widget:paint:
#+todo: BACKLOG STARTED | DONE

This page documents a task in the [[id:STORY-1][Build the widget]] story.

* Status

| Field | Value |
|-------+-------|
| State | DONE  |

* Goal

The widget is painted.
)"),
        docs.add(R"(:PROPERTIES:
:ID: KNOW-1
:AREA: tools
:END:
#+title: How widgets work
#+type: knowledge
#+audience: user
#+owner: ana

* Overview
:PROPERTIES:
:ID: OVERVIEW-ID
:END:

A widget turns a crank.
)"),
        docs.add(R"(:PROPERTIES:
:ID: MAN-1
:AREA: screens
:END:
#+title: Using the widget screen
#+type: manual
#+audience: user

* Open the screen

The widget screen lists every widget.
)"),
    };
}

io::connection work_database() {
    auto c = fresh();
    documents docs;
    place(c, "main", work_corpus(docs));
    return c;
}

std::vector<std::string> sorted(std::vector<std::string> v) {
    std::ranges::sort(v);
    v.erase(std::unique(v.begin(), v.end()), v.end());
    return v;
}

std::vector<std::string> found(io::connection& c,
    const types::document_filter& f,
    const types::view_filter& v = everything) {
    auto r = request("widget");
    r.filter = f;
    r.limit = 100;
    return sorted(ids_of(query::search(c, v, r)));
}

}

TEST_CASE("a title match outranks a heading match, which outranks text",
    "[query][search]") {
    auto c = fresh();
    documents docs;
    place(c, "main", {
        docs.add("#+title: Harbour notes\n\n"
                 "* Boats\n\nA lighthouse stands on the hill.\n"),
        docs.add("#+title: Lighthouse keeping\n\n* Duties\n\n"
                 "Keep the lamp lit.\n"),
        docs.add("#+title: Coastal guide\n\n* Lighthouse\n\n"
                 "Visit at dawn.\n"),
    });
    const auto r = query::search(c, everything, request("lighthouse"));
    REQUIRE(r.hits.size() == 3);
    CHECK(r.hits[0].title == "Lighthouse keeping");
    CHECK(r.hits[1].title == "Coastal guide");
    CHECK(r.hits[2].title == "Harbour notes");
    CHECK(r.hits[0].rank > r.hits[1].rank);
    CHECK(r.hits[1].rank > r.hits[2].rank);
}

TEST_CASE("a tie of rank is broken by the order of document types",
    "[query][search]") {
    auto c = fresh();
    documents docs;
    place(c, "main", {
        docs.add("#+title: One\n#+type: knowledge\n\n* Notes\n\nThe gizmo.\n"),
        docs.add("#+title: Two\n#+type: manual\n\n* Notes\n\nThe gizmo.\n"),
    });
    types::ranking how;
    how.type_order = {"manual", "knowledge"};
    auto r = query::search(c, everything, request("gizmo"), how);
    REQUIRE(r.hits.size() == 2);
    CHECK(r.hits[0].rank == r.hits[1].rank);
    CHECK(r.hits[0].type == "manual");
    how.type_order = {"knowledge", "manual"};
    r = query::search(c, everything, request("gizmo"), how);
    REQUIRE(r.hits.size() == 2);
    CHECK(r.hits[0].type == "knowledge");
}

TEST_CASE("a hit carries its document, heading, anchor and snippet",
    "[query][search]") {
    auto c = work_database();
    auto r = request("crank");
    const auto s = query::search(c, everything, r);
    REQUIRE(s.hits.size() == 1);
    const auto& h = s.hits[0];
    CHECK(h.org_id == "KNOW-1");
    CHECK(h.title == "How widgets work");
    CHECK(h.type == "knowledge");
    CHECK(h.path == "doc3.org");
    CHECK(h.heading_path == std::vector<std::string>{"Overview"});
    CHECK(h.anchor == "OVERVIEW-ID");
    CHECK(h.snippet == "widget turns a <b>crank</b>");
    CHECK(s.total == 1);
    CHECK_FALSE(s.synced_at.empty());

    const auto pre = query::search(c, everything, request("documents story"));
    REQUIRE_FALSE(pre.hits.empty());
    CHECK(pre.hits[0].org_id == "STORY-1");
    CHECK(pre.hits[0].heading_path.empty());
    CHECK(pre.hits[0].anchor.empty());
    CHECK(pre.hits[0].snippet.find("<b>story</b>") != std::string::npos);
}

TEST_CASE("each filter keeps only the documents it names",
    "[query][search]") {
    auto c = work_database();
    CHECK(found(c, {}) == std::vector<std::string>{"KNOW-1", "MAN-1",
        "SPRINT-1", "STORY-1", "TASK-1"});

    types::document_filter f;
    f.type = "manual";
    CHECK(found(c, f) == std::vector<std::string>{"MAN-1"});

    f = {};
    f.tags = {"widget"};
    CHECK(found(c, f) == std::vector<std::string>{"STORY-1", "TASK-1"});
    f.tags = {"widget", "paint"};
    CHECK(found(c, f) == std::vector<std::string>{"TASK-1"});

    f = {};
    f.state = "STARTED";
    CHECK(found(c, f) == std::vector<std::string>{"SPRINT-1", "STORY-1"});
    f.state = "DONE";
    CHECK(found(c, f) == std::vector<std::string>{"TASK-1"});

    f = {};
    f.sprint = "SPRINT-1";
    CHECK(found(c, f) == std::vector<std::string>{"SPRINT-1", "STORY-1",
        "TASK-1"});

    f = {};
    f.level = "s1";
    CHECK(found(c, f) == std::vector<std::string>{"TASK-1"});

    f = {};
    f.audience = "user";
    CHECK(found(c, f) == std::vector<std::string>{"KNOW-1", "MAN-1"});

    f = {};
    f.keywords = {{"OWNER", "ana"}};
    CHECK(found(c, f) == std::vector<std::string>{"KNOW-1"});

    f = {};
    f.properties = {{"AREA", "screens"}};
    CHECK(found(c, f) == std::vector<std::string>{"MAN-1"});
    f.properties = {{"AREA", "nowhere"}};
    CHECK(found(c, f).empty());
}

TEST_CASE("the view hides documents before any filter",
    "[query][search]") {
    auto c = work_database();
    const types::view_filter knowledge{"d.type IN ('knowledge', 'manual')"};
    CHECK(found(c, {}, knowledge) == std::vector<std::string>{"KNOW-1",
        "MAN-1"});
    types::document_filter f;
    f.sprint = "SPRINT-1";
    CHECK(found(c, f, knowledge).empty());

    types::facets_request fr;
    fr.environment = "main";
    const auto counts = query::facets(c, knowledge, fr);
    CHECK(counts.documents == 2);
}

TEST_CASE("each environment sees its own placements", "[query][search]") {
    auto c = fresh();
    documents docs;
    const auto first = docs.add(":PROPERTIES:\n:ID: PLAN\n:END:\n"
                                "#+title: Plan\n\nThe old route.\n");
    const auto second = docs.add(":PROPERTIES:\n:ID: PLAN\n:END:\n"
                                 "#+title: Plan\n\nThe new route.\n");
    place(c, "main", {first});
    place(c, "branch", {second});

    CHECK(query::search(c, everything, request("old")).hits.size() == 1);
    CHECK(query::search(c, everything, request("new")).hits.empty());
    CHECK(query::search(c, everything, request("old", "branch")).hits.empty());
    CHECK(query::search(c, everything, request("new", "branch")).hits.size() ==
        1);
    CHECK_THROWS_AS(query::search(c, everything, request("old", "nowhere")),
        query::unknown_environment);
}

TEST_CASE("facets count each filter's values over the result",
    "[query][search]") {
    auto c = work_database();
    types::facets_request r;
    r.environment = "main";
    r.q = "widget";
    r.keywords = {"owner"};
    r.properties = {"AREA"};
    const auto f = query::facets(c, everything, r);
    CHECK(f.documents == 5);
    const auto counts = [](const std::vector<types::facet_count>& v) {
        std::vector<std::string> out;
        for (const auto& x : v)
            out.push_back(x.value + "=" + std::to_string(x.count) +
                (x.label.empty() ? "" : " " + x.label));
        return out;
    };
    CHECK(counts(f.types) == std::vector<std::string>{"knowledge=1",
        "manual=1", "sprint=1", "story=1", "task=1"});
    CHECK(counts(f.tags) == std::vector<std::string>{"widget=2", "paint=1",
        "sprint_01=1"});
    CHECK(counts(f.states) == std::vector<std::string>{"STARTED=2",
        "DONE=1"});
    CHECK(counts(f.sprints) == std::vector<std::string>{
        "SPRINT-1=3 Sprint 1"});
    CHECK(counts(f.levels) == std::vector<std::string>{"s1=1"});
    CHECK(counts(f.audiences) == std::vector<std::string>{"user=2"});
    REQUIRE(f.keywords.size() == 1);
    CHECK(counts(f.keywords[0].counts) == std::vector<std::string>{"ana=1",
        "marco=1"});
    REQUIRE(f.properties.size() == 1);
    CHECK(counts(f.properties[0].counts) == std::vector<std::string>{
        "screens=1", "tools=1"});

    r.q = "crank";
    r.filter.type = "knowledge";
    const auto narrow = query::facets(c, everything, r);
    CHECK(narrow.documents == 1);
    CHECK(counts(narrow.types) == std::vector<std::string>{"knowledge=1"});

    r.q.clear();
    r.filter = {};
    CHECK(query::facets(c, everything, r).documents == 5);
}

TEST_CASE("pages of hits follow one another without overlap",
    "[query][search]") {
    auto c = work_database();
    auto r = request("widget");
    r.limit = 100;
    const auto all = query::search(c, everything, r);
    REQUIRE(all.total >= 5);
    CHECK(all.hits.size() == all.total);

    std::vector<std::string> paged;
    r.limit = 2;
    for (r.offset = 0; r.offset < all.total + 2; r.offset += 2) {
        const auto page = query::search(c, everything, r);
        CHECK(page.total == all.total);
        CHECK(page.hits.size() <= 2);
        for (const auto& h : page.hits)
            paged.push_back(h.org_id + "#" + h.anchor);
    }
    std::vector<std::string> whole;
    for (const auto& h : all.hits)
        whole.push_back(h.org_id + "#" + h.anchor);
    CHECK(paged == whole);
}
