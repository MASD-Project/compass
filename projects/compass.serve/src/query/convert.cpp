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
#include "compass.serve/query/convert.hpp"

#include <utility>

namespace compass::serve::query {

namespace {

std::vector<messages::facet_count> counts(const std::vector<db::types::facet_count>& in) {
    std::vector<messages::facet_count> out;
    out.reserve(in.size());
    for (const auto& c : in)
        out.push_back({c.value, c.label, c.count});
    return out;
}

std::vector<messages::named_facet> named(const std::vector<db::types::named_facet>& in) {
    std::vector<messages::named_facet> out;
    for (const auto& f : in)
        out.push_back({f.name, counts(f.counts)});
    return out;
}

messages::sprint_summary sprint(const db::types::sprint_summary& s) {
    return {s.org_id, s.title, s.state};
}

messages::work_item item(const db::types::work_item& w) {
    return {w.org_id, w.title, w.state, w.path};
}

}

db::types::document_filter to_db(const messages::document_filter& f) {
    db::types::document_filter out;
    out.type = f.type;
    out.tags = f.tags;
    out.state = f.state;
    out.sprint = f.sprint;
    out.level = f.level;
    out.audience = f.audience;
    for (const auto& k : f.keywords)
        out.keywords.push_back({k.name, k.value});
    for (const auto& p : f.properties)
        out.properties.push_back({p.name, p.value});
    return out;
}

db::types::search_request to_db(const messages::search_request& r) {
    return {r.environment, r.q, to_db(r.filter), r.limit, r.offset};
}

db::types::facets_request to_db(const messages::facets_request& r) {
    return {r.environment, r.q, to_db(r.filter), r.keywords, r.properties};
}

db::types::sprint_request to_db(const messages::sprint_state_request& r) {
    return {r.environment, r.sprint};
}

db::types::sprint_request to_db(const messages::what_needs_testing_request& r) {
    return {r.environment, r.sprint};
}

messages::search_response to_message(const db::types::search_response& r) {
    messages::search_response out;
    out.total = r.total;
    out.synced_at = r.synced_at;
    for (const auto& h : r.hits)
        out.hits.push_back({h.org_id, h.title, h.type, h.description, h.path, h.heading_path,
                            h.anchor, h.snippet, h.rank});
    return out;
}

messages::facets_response to_message(const db::types::facets_response& r) {
    messages::facets_response out;
    out.documents = r.documents;
    out.types = counts(r.types);
    out.tags = counts(r.tags);
    out.states = counts(r.states);
    out.sprints = counts(r.sprints);
    out.levels = counts(r.levels);
    out.audiences = counts(r.audiences);
    out.keywords = named(r.keywords);
    out.properties = named(r.properties);
    return out;
}

messages::sprint_state_response to_message(const db::types::sprint_state_response& r) {
    messages::sprint_state_response out;
    out.sprint = sprint(r.sprint);
    out.synced_at = r.synced_at;
    for (const auto& s : r.stories) {
        messages::story_state story;
        story.story = item(s.story);
        for (const auto& t : s.tasks)
            story.tasks.push_back(item(t));
        out.stories.push_back(std::move(story));
    }
    return out;
}

messages::testing_response to_message(const db::types::testing_response& r) {
    messages::testing_response out;
    out.sprint = sprint(r.sprint);
    out.synced_at = r.synced_at;
    for (const auto& s : r.stories) {
        messages::story_testing story;
        story.org_id = s.org_id;
        story.title = s.title;
        story.state = s.state;
        story.path = s.path;
        story.campaign = s.campaign;
        story.open_notes = s.open_notes;
        for (const auto& sc : s.scenarios) {
            messages::scenario_status status;
            status.org_id = sc.org_id;
            status.title = sc.title;
            status.path = sc.path;
            status.task_id = sc.task_id;
            assign_text(status.state, db::types::to_string(sc.state));
            status.open_notes = sc.open_notes;
            if (sc.latest_run) {
                messages::run_summary run;
                run.id = sc.latest_run->id;
                run.tester = sc.latest_run->tester;
                run.finished_at = sc.latest_run->finished_at;
                assign_text(run.outcome, sc.latest_run->outcome);
                run.current_definition = sc.latest_run->current_definition;
                status.latest_run = std::move(run);
            }
            story.scenarios.push_back(std::move(status));
        }
        out.stories.push_back(std::move(story));
    }
    return out;
}

}
