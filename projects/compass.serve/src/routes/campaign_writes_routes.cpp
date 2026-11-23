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
/**
 * AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Template: masd_cpp_http_routes.cpp.mustache
 * To modify, update the template and regenerate.
 */
#include "compass.serve/routes/campaign_writes_routes.hpp"

#include <utility>
#include "compass.serve/net/answer.hpp"
#include "compass.serve/net/binding.hpp"

namespace compass::serve::routes {

namespace {

compass::serve::messages::start_run_request bind_start_run(const domain::http_request& req) {
    net::binding b(req);
    compass::serve::messages::start_run_request r;
    b.body(r, { "scenario_id" }, { "scenario_id" });
    b.path("environment", r.environment);
    b.finish();
    return r;
}

}

void add_start_run(net::router& r, start_run_handler h) {
    auto route = r.post("/v1/{environment}/runs")
        .summary("Start a test run")
        .description("Starts a run of a test scenario as an environment holds it, opening the campaign of its story there on the story's first run.")
        .catalogue("start_run", "qa", "run", true)
        .caller(domain::caller_need::subject)
        .body<compass::serve::messages::start_run_request>()
        .input<compass::serve::messages::start_run_request>()
        .response<compass::serve::messages::start_run_response>()
        .handler([h = std::move(h)](const domain::http_request& req) {
            return net::ready(net::answer(req, domain::caller_need::subject,
                [&](const domain::caller& c) { return h(c, bind_start_run(req)); }));
        })
        .build();
    route.body_schema->json_schema =
        net::body_schema(route.body_schema->json_schema, { "scenario_id" });
    r.add_route(route);
}

namespace {

compass::serve::messages::step_result_request bind_record_step_result(const domain::http_request& req) {
    net::binding b(req);
    compass::serve::messages::step_result_request r;
    b.body(r, { "outcome", "notes" }, {  });
    b.path("environment", r.environment);
    b.path("run", r.run);
    b.path("step_ord", r.step_ord);
    b.finish();
    return r;
}

}

void add_record_step_result(net::router& r, record_step_result_handler h) {
    auto route = r.put("/v1/{environment}/runs/{run}/steps/{step_ord}")
        .summary("Record a step's outcome")
        .description("Records, or replaces, the outcome of a step of an unfinished run.")
        .catalogue("record_step_result", "qa", "run", true)
        .caller(domain::caller_need::subject)
        .body<compass::serve::messages::step_result_request>()
        .input<compass::serve::messages::step_result_request>()
        .handler([h = std::move(h)](const domain::http_request& req) {
            return net::ready(net::answer(req, domain::caller_need::subject,
                [&](const domain::caller& c) { return h(c, bind_record_step_result(req)); }));
        })
        .build();
    route.body_schema->json_schema =
        net::body_schema(route.body_schema->json_schema, { "outcome", "notes" });
    r.add_route(route);
}

namespace {

compass::serve::messages::finish_run_request bind_finish_run(const domain::http_request& req) {
    net::binding b(req);
    compass::serve::messages::finish_run_request r;
    b.path("environment", r.environment);
    b.path("run", r.run);
    b.finish();
    return r;
}

}

void add_finish_run(net::router& r, finish_run_handler h) {
    auto route = r.post("/v1/{environment}/runs/{run}/finish")
        .summary("Finish a test run")
        .description("Finishes an unfinished run with the outcome its step results give: failed when a step failed, blocked when one was blocked and none failed, passed otherwise.")
        .catalogue("finish_run", "qa", "run", true)
        .caller(domain::caller_need::subject)
        .no_body()
        .input<compass::serve::messages::finish_run_request>()
        .response<compass::serve::messages::finish_run_response>()
        .handler([h = std::move(h)](const domain::http_request& req) {
            return net::ready(net::answer(req, domain::caller_need::subject,
                [&](const domain::caller& c) { return h(c, bind_finish_run(req)); }));
        })
        .build();
    r.add_route(route);
}

namespace {

compass::serve::messages::attach_request bind_attach_screenshot(const domain::http_request& req) {
    net::binding b(req);
    compass::serve::messages::attach_request r;
    b.body(r, { "run", "note", "step_ord", "media_type", "bytes", "caption" }, { "media_type", "bytes" });
    b.path("environment", r.environment);
    b.finish();
    return r;
}

}

void add_attach_screenshot(net::router& r, attach_screenshot_handler h) {
    auto route = r.post("/v1/{environment}/attachments")
        .summary("Attach a screenshot")
        .description("Attaches a screenshot to a run, at one of its steps or to the run as a whole, or to a note, storing its bytes once. Attaching the same bytes to the same place again changes only the caption.")
        .catalogue("attach_screenshot", "qa", "attachment", true)
        .caller(domain::caller_need::subject)
        .body<compass::serve::messages::attach_request>()
        .max_body(16777216)
        .input<compass::serve::messages::attach_request>()
        .response<compass::serve::messages::attach_response>()
        .handler([h = std::move(h)](const domain::http_request& req) {
            return net::ready(net::answer(req, domain::caller_need::subject,
                [&](const domain::caller& c) { return h(c, bind_attach_screenshot(req)); }));
        })
        .build();
    route.body_schema->json_schema =
        net::body_schema(route.body_schema->json_schema, { "run", "note", "step_ord", "media_type", "bytes", "caption" });
    r.add_route(route);
}

namespace {

compass::serve::messages::add_note_request bind_add_note(const domain::http_request& req) {
    net::binding b(req);
    compass::serve::messages::add_note_request r;
    b.body(r, { "scenario_id", "step_ord", "kind", "text" }, { "kind", "text" });
    b.path("environment", r.environment);
    b.path("campaign", r.campaign);
    b.finish();
    return r;
}

}

void add_add_note(net::router& r, add_note_handler h) {
    auto route = r.post("/v1/{environment}/campaigns/{campaign}/notes")
        .summary("Add a note to a campaign")
        .description("Adds an open note to a campaign, optionally naming a scenario and a step.")
        .catalogue("add_note", "qa", "note", true)
        .caller(domain::caller_need::subject)
        .body<compass::serve::messages::add_note_request>()
        .input<compass::serve::messages::add_note_request>()
        .response<compass::serve::messages::add_note_response>()
        .handler([h = std::move(h)](const domain::http_request& req) {
            return net::ready(net::answer(req, domain::caller_need::subject,
                [&](const domain::caller& c) { return h(c, bind_add_note(req)); }));
        })
        .build();
    route.body_schema->json_schema =
        net::body_schema(route.body_schema->json_schema, { "scenario_id", "step_ord", "kind", "text" });
    r.add_route(route);
}

namespace {

compass::serve::messages::address_note_request bind_address_note(const domain::http_request& req) {
    net::binding b(req);
    compass::serve::messages::address_note_request r;
    b.body(r, { "reply" }, {  });
    b.path("environment", r.environment);
    b.path("note", r.note);
    b.finish();
    return r;
}

}

void add_address_note(net::router& r, address_note_handler h) {
    auto route = r.patch("/v1/{environment}/notes/{note}")
        .summary("Address a note")
        .description("Marks an open note addressed, with an optional reply.")
        .catalogue("address_note", "qa", "note", true)
        .caller(domain::caller_need::subject)
        .body<compass::serve::messages::address_note_request>()
        .input<compass::serve::messages::address_note_request>()
        .handler([h = std::move(h)](const domain::http_request& req) {
            return net::ready(net::answer(req, domain::caller_need::subject,
                [&](const domain::caller& c) { return h(c, bind_address_note(req)); }));
        })
        .build();
    route.body_schema->json_schema =
        net::body_schema(route.body_schema->json_schema, { "reply" });
    r.add_route(route);
}

}
