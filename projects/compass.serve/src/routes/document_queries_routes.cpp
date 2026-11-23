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
#include "compass.serve/routes/document_queries_routes.hpp"

#include <utility>
#include "compass.serve/net/answer.hpp"
#include "compass.serve/net/binding.hpp"

namespace compass::serve::routes {

namespace {

compass::serve::messages::search_request bind_search_documents(const domain::http_request& req) {
    net::binding b(req);
    compass::serve::messages::search_request r;
    b.path("environment", r.environment);
    b.query("q", r.q, true);
    b.query("type", r.filter.type, false);
    b.query("tag", r.filter.tags, false);
    b.query("state", r.filter.state, false);
    b.query("sprint", r.filter.sprint, false);
    b.query("level", r.filter.level, false);
    b.query("audience", r.filter.audience, false);
    b.query("keyword", r.filter.keywords, false);
    b.query("property", r.filter.properties, false);
    b.query("limit", r.limit, false);
    b.query("offset", r.offset, false);
    b.finish();
    return r;
}

}

void add_search_documents(net::router& r, search_documents_handler h) {
    auto route = r.get("/v1/{environment}/search")
        .summary("Search the documents")
        .description("Sections of an environment's documents matching a web-style query, best first, inside the caller's view.")
        .catalogue("search_documents", "documents", "document", false)
        .caller(domain::caller_need::view)
        .query_param("q", "string", "", true, "The query, in websearch_to_tsquery syntax: words, \"phrases\", or, -exclusions. An empty query matches nothing.")
        .query_param("type", "string", "", false, "Only documents of this #+type:.")
        .query_param("tag", "string", "", false, "Only documents carrying every one of these file tags.")
        .query_param("state", "string", "", false, "Only documents in this state.")
        .query_param("sprint", "string", "", false, "Only documents of this sprint.")
        .query_param("level", "string", "", false, "Only documents of this #+level:.")
        .query_param("audience", "string", "", false, "Only documents for this audience.")
        .query_param("keyword", "string", "", false, "Only documents whose keyword holds the value, each written name:value and split at the first colon.")
        .query_param("property", "string", "", false, "Only documents whose top-level property holds the value, each written NAME:value and split at the first colon.")
        .query_param("limit", "integer", "", false, "The most hits to answer.")
        .query_param("offset", "integer", "", false, "How many hits to skip, for the next page.")
        .input<compass::serve::messages::search_request>()
        .response<compass::serve::messages::search_response>()
        .handler([h = std::move(h)](const domain::http_request& req) {
            return net::ready(net::answer(req, domain::caller_need::view,
                [&](const domain::caller& c) { return h(c, bind_search_documents(req)); }));
        })
        .build();
    r.add_route(route);
}

namespace {

compass::serve::messages::facets_request bind_list_facets(const domain::http_request& req) {
    net::binding b(req);
    compass::serve::messages::facets_request r;
    b.path("environment", r.environment);
    b.query("q", r.q, false);
    b.query("type", r.filter.type, false);
    b.query("tag", r.filter.tags, false);
    b.query("state", r.filter.state, false);
    b.query("sprint", r.filter.sprint, false);
    b.query("level", r.filter.level, false);
    b.query("audience", r.filter.audience, false);
    b.query("keyword", r.filter.keywords, false);
    b.query("property", r.filter.properties, false);
    b.query("facet_keyword", r.keywords, false);
    b.query("facet_property", r.properties, false);
    b.finish();
    return r;
}

}

void add_list_facets(net::router& r, list_facets_handler h) {
    auto route = r.get("/v1/{environment}/facets")
        .summary("Count the filter values")
        .description("The values of each filter over the documents a query matches, with how many documents hold each, for a search page's filters.")
        .catalogue("list_facets", "documents", "document", false)
        .caller(domain::caller_need::view)
        .query_param("q", "string", "", false, "The query whose documents are counted, as search takes it; every document of the environment inside the view when absent or empty.")
        .query_param("type", "string", "", false, "Only documents of this #+type:.")
        .query_param("tag", "string", "", false, "Only documents carrying every one of these file tags.")
        .query_param("state", "string", "", false, "Only documents in this state.")
        .query_param("sprint", "string", "", false, "Only documents of this sprint.")
        .query_param("level", "string", "", false, "Only documents of this #+level:.")
        .query_param("audience", "string", "", false, "Only documents for this audience.")
        .query_param("keyword", "string", "", false, "Only documents whose keyword holds the value, each written name:value and split at the first colon.")
        .query_param("property", "string", "", false, "Only documents whose top-level property holds the value, each written NAME:value and split at the first colon.")
        .query_param("facet_keyword", "string", "", false, "The keywords whose values to count, besides the fixed facets.")
        .query_param("facet_property", "string", "", false, "The top-level properties whose values to count.")
        .input<compass::serve::messages::facets_request>()
        .response<compass::serve::messages::facets_response>()
        .handler([h = std::move(h)](const domain::http_request& req) {
            return net::ready(net::answer(req, domain::caller_need::view,
                [&](const domain::caller& c) { return h(c, bind_list_facets(req)); }));
        })
        .build();
    r.add_route(route);
}

namespace {

compass::serve::messages::list_documents_request bind_list_documents(const domain::http_request& req) {
    net::binding b(req);
    compass::serve::messages::list_documents_request r;
    b.path("environment", r.environment);
    b.query("type", r.filter.type, false);
    b.query("tag", r.filter.tags, false);
    b.query("state", r.filter.state, false);
    b.query("sprint", r.filter.sprint, false);
    b.query("level", r.filter.level, false);
    b.query("audience", r.filter.audience, false);
    b.query("keyword", r.filter.keywords, false);
    b.query("property", r.filter.properties, false);
    b.query("limit", r.limit, false);
    b.query("offset", r.offset, false);
    b.finish();
    return r;
}

}

void add_list_documents(net::router& r, list_documents_handler h) {
    auto route = r.get("/v1/{environment}/documents")
        .summary("List the documents")
        .description("The documents of an environment matching a filter, by path, a page at a time, inside the caller's view.")
        .catalogue("list_documents", "documents", "document", false)
        .caller(domain::caller_need::view)
        .query_param("type", "string", "", false, "Only documents of this #+type:.")
        .query_param("tag", "string", "", false, "Only documents carrying every one of these file tags.")
        .query_param("state", "string", "", false, "Only documents in this state.")
        .query_param("sprint", "string", "", false, "Only documents of this sprint.")
        .query_param("level", "string", "", false, "Only documents of this #+level:.")
        .query_param("audience", "string", "", false, "Only documents for this audience.")
        .query_param("keyword", "string", "", false, "Only documents whose keyword holds the value, each written name:value and split at the first colon.")
        .query_param("property", "string", "", false, "Only documents whose top-level property holds the value, each written NAME:value and split at the first colon.")
        .query_param("limit", "integer", "", false, "The most documents to answer.")
        .query_param("offset", "integer", "", false, "How many documents to skip, for the next page.")
        .input<compass::serve::messages::list_documents_request>()
        .response<compass::serve::messages::list_documents_response>()
        .handler([h = std::move(h)](const domain::http_request& req) {
            return net::ready(net::answer(req, domain::caller_need::view,
                [&](const domain::caller& c) { return h(c, bind_list_documents(req)); }));
        })
        .build();
    r.add_route(route);
}

namespace {

compass::serve::messages::get_document_request bind_get_document(const domain::http_request& req) {
    net::binding b(req);
    compass::serve::messages::get_document_request r;
    b.path("environment", r.environment);
    b.path("id", r.id);
    b.query("format", r.format, false);
    net::binding::one_of("format", r.format, { "html", "org" });
    b.finish();
    return r;
}

}

void add_get_document(net::router& r, get_document_handler h) {
    auto route = r.get("/v1/{environment}/documents/{id}")
        .summary("Read a document")
        .description("One document of an environment: its metadata and its body, as HTML or as its org source. The HTML is compass.html's rendering, with links to documents outside the caller's view as plain text.")
        .catalogue("get_document", "documents", "document", false)
        .caller(domain::caller_need::view)
        .query_param("format", "string", "", false, "The body's form: html or the org source.")
        .input<compass::serve::messages::get_document_request>()
        .response<compass::serve::messages::get_document_response>()
        .handler([h = std::move(h)](const domain::http_request& req) {
            return net::ready(net::answer(req, domain::caller_need::view,
                [&](const domain::caller& c) { return h(c, bind_get_document(req)); }));
        })
        .build();
    r.add_route(route);
}

namespace {

compass::serve::messages::backlinks_request bind_list_backlinks(const domain::http_request& req) {
    net::binding b(req);
    compass::serve::messages::backlinks_request r;
    b.path("environment", r.environment);
    b.path("id", r.id);
    b.finish();
    return r;
}

}

void add_list_backlinks(net::router& r, list_backlinks_handler h) {
    auto route = r.get("/v1/{environment}/documents/{id}/backlinks")
        .summary("Find what links to a document")
        .description("The documents of an environment that link to a document by its :ID:, each with the heading the link sits under, inside the caller's view.")
        .catalogue("list_backlinks", "documents", "document", false)
        .caller(domain::caller_need::view)
        .input<compass::serve::messages::backlinks_request>()
        .response<compass::serve::messages::backlinks_response>()
        .handler([h = std::move(h)](const domain::http_request& req) {
            return net::ready(net::answer(req, domain::caller_need::view,
                [&](const domain::caller& c) { return h(c, bind_list_backlinks(req)); }));
        })
        .build();
    r.add_route(route);
}

namespace {

compass::serve::messages::list_environments_request bind_list_environments(const domain::http_request& req) {
    net::binding b(req);
    compass::serve::messages::list_environments_request r;
    b.finish();
    return r;
}

}

void add_list_environments(net::router& r, list_environments_handler h) {
    auto route = r.get("/v1/environments")
        .summary("List the environments")
        .description("Every environment of the repository, by name, with when each was last synced.")
        .catalogue("list_environments", "documents", "environment", false)
        .caller(domain::caller_need::view)
        .input<compass::serve::messages::list_environments_request>()
        .response<compass::serve::messages::list_environments_response>()
        .handler([h = std::move(h)](const domain::http_request& req) {
            return net::ready(net::answer(req, domain::caller_need::view,
                [&](const domain::caller& c) { return h(c, bind_list_environments(req)); }));
        })
        .build();
    r.add_route(route);
}

namespace {

compass::serve::messages::version_request bind_get_version(const domain::http_request& req) {
    net::binding b(req);
    compass::serve::messages::version_request r;
    b.finish();
    return r;
}

}

void add_get_version(net::router& r, get_version_handler h) {
    auto route = r.get("/v1/version")
        .summary("Show the service's version")
        .description("The API version the service answers and the version of compass serving it.")
        .catalogue("get_version", "documents", "service", false)
        .caller(domain::caller_need::view)
        .input<compass::serve::messages::version_request>()
        .response<compass::serve::messages::version_response>()
        .handler([h = std::move(h)](const domain::http_request& req) {
            return net::ready(net::answer(req, domain::caller_need::view,
                [&](const domain::caller& c) { return h(c, bind_get_version(req)); }));
        })
        .build();
    r.add_route(route);
}

namespace {

compass::serve::messages::sync_request bind_sync_environment(const domain::http_request& req) {
    net::binding b(req);
    compass::serve::messages::sync_request r;
    b.path("environment", r.environment);
    b.finish();
    return r;
}

}

void add_sync_environment(net::router& r, sync_environment_handler h) {
    auto route = r.post("/v1/{environment}/sync")
        .summary("Sync an environment")
        .description("Brings an environment's documents in the database up to date with its files, so the next read sees an edit made since the last sync.")
        .catalogue("sync_environment", "documents", "environment", true)
        .caller(domain::caller_need::subject)
        .no_body()
        .input<compass::serve::messages::sync_request>()
        .response<compass::serve::messages::sync_response>()
        .handler([h = std::move(h)](const domain::http_request& req) {
            return net::ready(net::answer(req, domain::caller_need::subject,
                [&](const domain::caller& c) { return h(c, bind_sync_environment(req)); }));
        })
        .build();
    r.add_route(route);
}

}
