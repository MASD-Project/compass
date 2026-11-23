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
#include "compass.serve/routes/testing_queries_routes.hpp"

#include <utility>
#include "compass.serve/net/answer.hpp"
#include "compass.serve/net/binding.hpp"

namespace compass::serve::routes {

namespace {

compass::serve::messages::sprint_state_request bind_get_sprint_state(const domain::http_request& req) {
    net::binding b(req);
    compass::serve::messages::sprint_state_request r;
    b.path("environment", r.environment);
    b.query("sprint", r.sprint, false);
    b.finish();
    return r;
}

}

void add_get_sprint_state(net::router& r, get_sprint_state_handler h) {
    auto route = r.get("/v1/{environment}/sprint")
        .summary("Show a sprint's state")
        .description("A sprint's stories and the tasks of each, with their states, as an environment holds them.")
        .catalogue("get_sprint_state", "agile", "sprint", false)
        .caller(domain::caller_need::view)
        .query_param("sprint", "string", "", false, "The sprint's :ID: or title; the present sprint when absent.")
        .input<compass::serve::messages::sprint_state_request>()
        .response<compass::serve::messages::sprint_state_response>()
        .handler([h = std::move(h)](const domain::http_request& req) {
            return net::ready(net::answer(req, domain::caller_need::view,
                [&](const domain::caller& c) { return h(c, bind_get_sprint_state(req)); }));
        })
        .build();
    r.add_route(route);
}

namespace {

compass::serve::messages::what_needs_testing_request bind_list_scenarios_to_test(const domain::http_request& req) {
    net::binding b(req);
    compass::serve::messages::what_needs_testing_request r;
    b.path("environment", r.environment);
    b.query("sprint", r.sprint, false);
    b.finish();
    return r;
}

}

void add_list_scenarios_to_test(net::router& r, list_scenarios_to_test_handler h) {
    auto route = r.get("/v1/{environment}/testing")
        .summary("Show what needs testing")
        .description("A sprint's stories, each story's test scenarios and each scenario's state from its latest finished run in an environment: untested, failed, passed or changed, with the open note counts. Untested, failed and changed need testing.")
        .catalogue("list_scenarios_to_test", "qa", "scenario", false)
        .caller(domain::caller_need::view)
        .query_param("sprint", "string", "", false, "The sprint's :ID: or title; the present sprint when absent.")
        .input<compass::serve::messages::what_needs_testing_request>()
        .response<compass::serve::messages::testing_response>()
        .handler([h = std::move(h)](const domain::http_request& req) {
            return net::ready(net::answer(req, domain::caller_need::view,
                [&](const domain::caller& c) { return h(c, bind_list_scenarios_to_test(req)); }));
        })
        .build();
    r.add_route(route);
}

namespace {

compass::serve::messages::get_run_request bind_get_run(const domain::http_request& req) {
    net::binding b(req);
    compass::serve::messages::get_run_request r;
    b.path("environment", r.environment);
    b.path("run", r.run);
    b.finish();
    return r;
}

}

void add_get_run(net::router& r, get_run_handler h) {
    auto route = r.get("/v1/{environment}/runs/{run}")
        .summary("Read a test run")
        .description("A run, finished or not, with the steps of the definition it tests and the outcome recorded for each so far.")
        .catalogue("get_run", "qa", "run", false)
        .caller(domain::caller_need::view)
        .input<compass::serve::messages::get_run_request>()
        .response<compass::serve::messages::get_run_response>()
        .handler([h = std::move(h)](const domain::http_request& req) {
            return net::ready(net::answer(req, domain::caller_need::view,
                [&](const domain::caller& c) { return h(c, bind_get_run(req)); }));
        })
        .build();
    r.add_route(route);
}

namespace {

compass::serve::messages::list_notes_request bind_list_notes(const domain::http_request& req) {
    net::binding b(req);
    compass::serve::messages::list_notes_request r;
    b.path("environment", r.environment);
    b.path("campaign", r.campaign);
    b.finish();
    return r;
}

}

void add_list_notes(net::router& r, list_notes_handler h) {
    auto route = r.get("/v1/{environment}/campaigns/{campaign}/notes")
        .summary("List a campaign's notes")
        .description("A campaign's notes, open and addressed, oldest first.")
        .catalogue("list_notes", "qa", "note", false)
        .caller(domain::caller_need::view)
        .input<compass::serve::messages::list_notes_request>()
        .response<compass::serve::messages::list_notes_response>()
        .handler([h = std::move(h)](const domain::http_request& req) {
            return net::ready(net::answer(req, domain::caller_need::view,
                [&](const domain::caller& c) { return h(c, bind_list_notes(req)); }));
        })
        .build();
    r.add_route(route);
}

}
