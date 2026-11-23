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
 * Template: masd_cpp_http_routes.hpp.mustache
 * To modify, update the template and regenerate.
 */
#ifndef COMPASS_SERVE_ROUTES_DOCUMENT_QUERIES_ROUTES_HPP
#define COMPASS_SERVE_ROUTES_DOCUMENT_QUERIES_ROUTES_HPP

#include <functional>
#include "compass.serve/domain/caller.hpp"
#include "compass.serve/messages/document_queries_protocol.hpp"
#include "compass.serve/net/router.hpp"

namespace compass::serve::routes {

/**
 * @brief What answers search_documents: Search the documents.
 */
using search_documents_handler = std::function<compass::serve::messages::search_response(
    const domain::caller&, const compass::serve::messages::search_request&)>;

/**
 * @brief Adds GET /v1/{environment}/search, the tool search_documents, to @p r, answered by
 * @p h.
 */
void add_search_documents(net::router& r, search_documents_handler h);

/**
 * @brief What answers list_facets: Count the filter values.
 */
using list_facets_handler = std::function<compass::serve::messages::facets_response(
    const domain::caller&, const compass::serve::messages::facets_request&)>;

/**
 * @brief Adds GET /v1/{environment}/facets, the tool list_facets, to @p r, answered by
 * @p h.
 */
void add_list_facets(net::router& r, list_facets_handler h);

/**
 * @brief What answers list_documents: List the documents.
 */
using list_documents_handler = std::function<compass::serve::messages::list_documents_response(
    const domain::caller&, const compass::serve::messages::list_documents_request&)>;

/**
 * @brief Adds GET /v1/{environment}/documents, the tool list_documents, to @p r, answered by
 * @p h.
 */
void add_list_documents(net::router& r, list_documents_handler h);

/**
 * @brief What answers get_document: Read a document.
 */
using get_document_handler = std::function<compass::serve::messages::get_document_response(
    const domain::caller&, const compass::serve::messages::get_document_request&)>;

/**
 * @brief Adds GET /v1/{environment}/documents/{id}, the tool get_document, to @p r, answered by
 * @p h.
 */
void add_get_document(net::router& r, get_document_handler h);

/**
 * @brief What answers list_backlinks: Find what links to a document.
 */
using list_backlinks_handler = std::function<compass::serve::messages::backlinks_response(
    const domain::caller&, const compass::serve::messages::backlinks_request&)>;

/**
 * @brief Adds GET /v1/{environment}/documents/{id}/backlinks, the tool list_backlinks, to @p r, answered by
 * @p h.
 */
void add_list_backlinks(net::router& r, list_backlinks_handler h);

/**
 * @brief What answers list_environments: List the environments.
 */
using list_environments_handler = std::function<compass::serve::messages::list_environments_response(
    const domain::caller&, const compass::serve::messages::list_environments_request&)>;

/**
 * @brief Adds GET /v1/environments, the tool list_environments, to @p r, answered by
 * @p h.
 */
void add_list_environments(net::router& r, list_environments_handler h);

/**
 * @brief What answers get_version: Show the service's version.
 */
using get_version_handler = std::function<compass::serve::messages::version_response(
    const domain::caller&, const compass::serve::messages::version_request&)>;

/**
 * @brief Adds GET /v1/version, the tool get_version, to @p r, answered by
 * @p h.
 */
void add_get_version(net::router& r, get_version_handler h);

/**
 * @brief What answers sync_environment: Sync an environment.
 */
using sync_environment_handler = std::function<compass::serve::messages::sync_response(
    const domain::caller&, const compass::serve::messages::sync_request&)>;

/**
 * @brief Adds POST /v1/{environment}/sync, the tool sync_environment, to @p r, answered by
 * @p h.
 */
void add_sync_environment(net::router& r, sync_environment_handler h);

}

#endif
