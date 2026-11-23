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

#include <stdexcept>
#include <utility>
#include "compass.db/codec/json_document.hpp"
#include "compass.db/query/search.hpp"
#include "compass.db/query/testing.hpp"
#include "compass.html/ops/render.hpp"
#include "compass.ingest/org/parser.hpp"
#include "compass.org/types/tables.hpp"
#include "compass.serve/domain/refusal.hpp"
#include "compass.serve/net/answer.hpp"
#include "compass.serve/ops/views.hpp"
#include "compass.serve/query/convert.hpp"
#include "compass.serve/query/reads.hpp"
#include "compass.serve/routes/document_queries_routes.hpp"
#include "compass.serve/routes/testing_queries_routes.hpp"

namespace compass::serve::app {

namespace {

using compass::db::codec::json_document;
using domain::refusal;
using domain::http_status;

/**
 * @brief Runs @p read, answering the queries' failures as the service's
 * refusals.
 */
template <typename Read>
auto refusing(Read&& read) {
    try {
        return read();
    } catch (const db::query::unknown_environment& e) {
        throw refusal(http_status::not_found, "unknown_environment",
                      std::string("The database holds no ") + e.what() + ".");
    } catch (const db::query::unknown_sprint& e) {
        throw refusal(http_status::not_found, "unknown_sprint", e.what());
    }
}

std::string method_name(domain::http_method m) {
    switch (m) {
    case domain::http_method::get: return "GET";
    case domain::http_method::post: return "POST";
    case domain::http_method::put: return "PUT";
    case domain::http_method::patch: return "PATCH";
    case domain::http_method::delete_: return "DELETE";
    case domain::http_method::head: return "HEAD";
    case domain::http_method::options: return "OPTIONS";
    }
    return "UNKNOWN";
}

void add_schema(compass::db::codec::json_value into, std::string_view name,
                const std::string& schema) {
    if (schema.empty())
        return;
    if (auto parsed = json_document::parse(schema))
        into.add(name, parsed->root());
}

std::string render_html(const query::document_source& d, const types::settings& s,
                        db::io::connection& c, const db::types::view_filter& v) {
    org::types::tables tables;
    ingest::org::parse(d.source, tables);
    std::unordered_map<std::string, bool> visible = d.visible;
    html::types::link_resolver links;
    links.document_of = [&](std::string_view id) -> std::optional<std::string> {
        if (const auto it = d.document_of.find(std::string(id)); it != d.document_of.end())
            return it->second;
        if (id == d.summary.org_id)
            return d.summary.org_id;
        return std::nullopt;
    };
    links.document_at = [&](std::string_view path) -> std::optional<std::string> {
        const auto found = query::document_at(c, v, d.environment_id, std::string(path));
        if (!found)
            return std::nullopt;
        visible.emplace(found->first, found->second);
        return found->first;
    };
    links.visible = [&](std::string_view id) {
        const auto it = visible.find(std::string(id));
        return it != visible.end() && it->second;
    };
    return html::ops::render(d.source, org::types::view(tables), d.summary.path, s.render,
                             links)
        .body;
}

}

connection_pool::connection_pool(std::string conninfo)
    : conninfo_(std::move(conninfo)) {}

connection_pool::lease::lease(connection_pool& pool, std::unique_ptr<db::io::connection> c)
    : pool_(pool)
    , c_(std::move(c)) {}

connection_pool::lease::~lease() {
    const std::lock_guard lock(pool_.mutex_);
    pool_.idle_.push_back(std::move(c_));
}

connection_pool::lease connection_pool::acquire() {
    {
        const std::lock_guard lock(mutex_);
        if (!idle_.empty()) {
            auto c = std::move(idle_.back());
            idle_.pop_back();
            return {*this, std::move(c)};
        }
    }
    return {*this, std::make_unique<db::io::connection>(conninfo_)};
}

service::service(types::settings s, connection_pool& pool)
    : settings_(std::move(s))
    , pool_(pool) {
    auto views = ops::compile_views(settings_.views);
    if (!views)
        throw std::runtime_error(views.error());
    views_ = std::move(*views);
}

const db::types::view_filter& service::view_of(const domain::caller& c) const {
    const auto it = views_.find(c.view);
    if (it == views_.end())
        throw refusal(http_status::unauthorized, "no_view",
                      "The view " + c.view + " is not one this service serves.");
    return it->second;
}

std::optional<domain::http_response> service::admit(domain::http_request& request) const {
    if (!settings_.gateway_secret.empty() &&
        request.get_header("Compass-Secret") != settings_.gateway_secret)
        return domain::http_response::api_error(http_status::forbidden, "forbidden",
                                                "The request does not carry the gateway's secret.");
    if (settings_.local_view && request.get_header("Compass-View").empty()) {
        request.headers["compass-view"] = *settings_.local_view;
        if (request.get_header("Compass-Subject").empty())
            request.headers["compass-subject"] = settings_.local_subject;
    }
    const auto view = request.get_header("Compass-View");
    if (view.empty())
        return domain::http_response::api_error(http_status::unauthorized, "no_view",
                                                "The request names no view in Compass-View.");
    if (!views_.contains(view))
        return domain::http_response::api_error(
            http_status::unauthorized, "no_view",
            "The view " + view + " is not one this service serves.");
    return std::nullopt;
}

void service::add_read_routes(net::router& r) {
    namespace m = messages;
    routes::add_search_documents(r, [this](const domain::caller& c, const m::search_request& q) {
        return refusing([&] {
            const auto lease = pool_.acquire();
            return query::to_message(db::query::search(*lease, view_of(c), query::to_db(q)));
        });
    });
    routes::add_list_facets(r, [this](const domain::caller& c, const m::facets_request& q) {
        return refusing([&] {
            const auto lease = pool_.acquire();
            return query::to_message(db::query::facets(*lease, view_of(c), query::to_db(q)));
        });
    });
    routes::add_list_documents(
        r, [this](const domain::caller& c, const m::list_documents_request& q) {
            return refusing([&] {
                const auto lease = pool_.acquire();
                return query::list_documents(*lease, view_of(c), q);
            });
        });
    routes::add_get_document(r, [this](const domain::caller& c,
                                       const m::get_document_request& q) {
        return refusing([&] {
            const auto& view = view_of(c);
            const auto lease = pool_.acquire();
            const auto d = query::read_document(*lease, view, q);
            if (!d)
                throw refusal::not_found("The environment " + q.environment +
                                         " has no document " + q.id + " in this view.");
            m::get_document_response out;
            out.document = d->summary;
            out.format = q.format;
            out.synced_at = d->synced_at;
            out.body = query::text_of(q.format) == "org"
                ? d->source
                : render_html(*d, settings_, *lease, view);
            return out;
        });
    });
    routes::add_list_backlinks(r, [this](const domain::caller& c,
                                         const m::backlinks_request& q) {
        return refusing([&] {
            const auto lease = pool_.acquire();
            auto out = query::backlinks(*lease, view_of(c), q);
            if (!out)
                throw refusal::not_found("The environment " + q.environment +
                                         " has no document " + q.id + " in this view.");
            return std::move(*out);
        });
    });
    routes::add_list_environments(
        r, [this](const domain::caller& c, const m::list_environments_request&) {
            static_cast<void>(view_of(c));
            const auto lease = pool_.acquire();
            return query::list_environments(*lease);
        });
    routes::add_get_version(r, [this](const domain::caller& c, const m::version_request&) {
        static_cast<void>(view_of(c));
        return m::version_response{"v1", settings_.version};
    });
    routes::add_get_sprint_state(
        r, [this](const domain::caller& c, const m::sprint_state_request& q) {
            return refusing([&] {
                const auto lease = pool_.acquire();
                return query::to_message(
                    db::query::sprint_state(*lease, view_of(c), query::to_db(q)));
            });
        });
    routes::add_list_scenarios_to_test(
        r, [this](const domain::caller& c, const m::what_needs_testing_request& q) {
            return refusing([&] {
                const auto lease = pool_.acquire();
                return query::to_message(
                    db::query::what_needs_testing(*lease, view_of(c), query::to_db(q)));
            });
        });
    routes::add_get_run(r, [this](const domain::caller& c, const m::get_run_request& q) {
        return refusing([&] {
            static_cast<void>(view_of(c));
            const auto lease = pool_.acquire();
            auto out = query::get_run(*lease, q);
            if (!out)
                throw refusal::not_found("The environment " + q.environment + " has no run " +
                                         std::to_string(q.run) + ".");
            return std::move(*out);
        });
    });
    routes::add_list_notes(r, [this](const domain::caller& c, const m::list_notes_request& q) {
        return refusing([&] {
            static_cast<void>(view_of(c));
            const auto lease = pool_.acquire();
            auto out = query::list_notes(*lease, q);
            if (!out)
                throw refusal::not_found("The environment " + q.environment +
                                         " has no campaign " + std::to_string(q.campaign) + ".");
            return std::move(*out);
        });
    });

    auto catalogue = r.get("/v1/catalogue")
                         .summary("List the service's operations")
                         .description("Every route's declaration and input schema, in the "
                                      "shape of ORE Studio's route_catalogue.")
                         .catalogue("get_catalogue", "documents", "service", false)
                         .caller(domain::caller_need::view)
                         .handler([&r](const domain::http_request& req) {
                             return net::ready(net::answer(
                                 req, domain::caller_need::view,
                                 [&](const domain::caller&) {
                                     return domain::http_response::json(catalogue_json(r));
                                 }));
                         })
                         .build();
    r.add_route(catalogue);
}

std::string catalogue_json(const net::router& r) {
    auto doc = json_document::make_object();
    auto routes = doc.root().add_array("routes");
    for (const auto& route : r.routes()) {
        auto item = routes.push_object();
        const auto& c = route.catalogue;
        item.set("tool", c.tool);
        item.set("area", c.area);
        item.set("entity", c.entity);
        item.set("writes", c.writes);
        item.set("takes_no_body", c.takes_no_body);
        item.set("summary", c.summary);
        item.set("description", c.description);
        auto tags = item.add_array("tags");
        for (const auto& t : c.tags)
            tags.push(t);
        item.set("method", method_name(route.method));
        item.set("path", route.pattern);
        item.set("caller", route.caller == domain::caller_need::subject ? "subject" : "view");
        auto params = item.add_array("query_params");
        for (const auto& q : route.query_params) {
            auto p = params.push_object();
            p.set("name", q.name);
            p.set("type", q.type);
            p.set("required", q.required);
            p.set("description", q.description);
        }
        add_schema(item, "input_schema", route.input_schema);
        if (route.body_schema)
            add_schema(item, "body_schema", route.body_schema->json_schema);
        if (route.success_response_schema)
            add_schema(item, "response_schema", route.success_response_schema->json_schema);
    }
    return doc.write();
}

}
