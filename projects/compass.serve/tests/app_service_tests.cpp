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
#include "compass.serve/app/service.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>
#include "compass.serve/net/binding.hpp"
#include "compass.serve/ops/views.hpp"

namespace {

using namespace compass::serve;
using Catch::Matchers::ContainsSubstring;

/// A pool whose database is never reached: a test that touches it fails.
app::connection_pool& unreachable_pool() {
    static app::connection_pool pool("host=/nonexistent dbname=none");
    return pool;
}

types::settings settings() {
    types::settings s;
    s.views = ops::default_views();
    return s;
}

domain::http_response call(const net::router& r, domain::http_request req) {
    const auto path = req.target.substr(0, req.target.find('?'));
    const auto route = r.match(req.method, path, req.path_params);
    REQUIRE(route);
    for (auto& [name, value] : net::query_parameters(req.target))
        req.query_params.try_emplace(name, value);
    boost::asio::io_context io;
    auto done = boost::asio::co_spawn(io, route->handler(req), boost::asio::use_future);
    io.run();
    return done.get();
}

}

TEST_CASE("the router's start-up gates accept every read route", "[serve][app]") {
    net::router r;
    app::service s(settings(), unreachable_pool());
    s.add_read_routes(r);
    CHECK(r.uncatalogued().empty());
    const auto catalogue = app::catalogue_json(r);
    for (const auto* tool : {"search_documents", "list_facets", "list_documents",
                             "get_document", "list_backlinks", "list_environments",
                             "get_version", "get_sprint_state", "list_scenarios_to_test",
                             "get_run", "list_notes", "get_catalogue"})
        CHECK_THAT(catalogue, ContainsSubstring(std::string("\"tool\":\"") + tool + "\""));
    CHECK(r.routes().size() == 12);
}

TEST_CASE("a request is admitted only with a view the service serves and "
          "the gateway's secret", "[serve][app]") {
    auto with_secret = settings();
    with_secret.gateway_secret = "s3cret";
    const app::service guarded(with_secret, unreachable_pool());
    domain::http_request req;
    req.headers["compass-view"] = "team";
    const auto refused = guarded.admit(req);
    REQUIRE(refused);
    CHECK(refused->status == domain::http_status::forbidden);
    req.headers["compass-secret"] = "s3cret";
    CHECK_FALSE(guarded.admit(req));

    const app::service open(settings(), unreachable_pool());
    domain::http_request none;
    const auto no_view = open.admit(none);
    REQUIRE(no_view);
    CHECK(no_view->status == domain::http_status::unauthorized);
    CHECK_THAT(no_view->body, ContainsSubstring("no_view"));
    none.headers["compass-view"] = "secret-club";
    CHECK(open.admit(none));

    auto local = settings();
    local.local_view = "team";
    local.local_subject = "dev";
    const app::service at_home(local, unreachable_pool());
    domain::http_request bare;
    CHECK_FALSE(at_home.admit(bare));
    CHECK(bare.headers.at("compass-view") == "team");
    CHECK(bare.headers.at("compass-subject") == "dev");
}

TEST_CASE("a read route refuses a parameter it does not take before it "
          "reads the database", "[serve][app]") {
    net::router r;
    app::service s(settings(), unreachable_pool());
    s.add_read_routes(r);
    domain::http_request req;
    req.target = "/v1/main/search?q=x&colour=red";
    req.headers["compass-view"] = "team";
    const auto response = call(r, req);
    CHECK(response.status == domain::http_status::bad_request);
    CHECK_THAT(response.body, ContainsSubstring("invalid"));
    CHECK_THAT(response.body, ContainsSubstring("colour"));
}
