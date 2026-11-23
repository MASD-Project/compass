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
#ifndef COMPASS_SERVE_QUERY_READS_HPP
#define COMPASS_SERVE_QUERY_READS_HPP

#include <optional>
#include <string>
#include <unordered_map>
#include "compass.db/io/connection.hpp"
#include "compass.db/types/search.hpp"
#include "compass.serve/messages/document_queries_protocol.hpp"
#include "compass.serve/messages/testing_queries_protocol.hpp"

namespace compass::serve::query {

/**
 * @brief The documents of @p r's environment inside @p v and the request's
 * filter, by path, a page at a time.
 * @throws db::query::unknown_environment when the environment does not exist.
 */
[[nodiscard]] messages::list_documents_response list_documents(
    db::io::connection& c, const db::types::view_filter& v,
    const messages::list_documents_request& r);

/**
 * @brief A document of an environment as the renderer needs it: its
 * metadata, its source, and for each :ID: its links name, the :ID: of the
 * document placed in the environment that holds it and whether the view
 * sees that document.
 */
struct document_source {
    messages::document_summary summary;
    std::string source;
    std::string synced_at;
    std::string environment_id;
    std::unordered_map<std::string, std::string> document_of;
    std::unordered_map<std::string, bool> visible;
};

/**
 * @brief The document of @p r's environment whose :ID: is @p r.id, inside
 * @p v, at its first path; nothing when there is none.
 * @throws db::query::unknown_environment when the environment does not exist.
 */
[[nodiscard]] std::optional<document_source> read_document(
    db::io::connection& c, const db::types::view_filter& v,
    const messages::get_document_request& r);

/**
 * @brief The :ID: of the document at @p path in the environment
 * @p environment_id and whether @p v sees it; nothing when none is placed
 * there.
 */
[[nodiscard]] std::optional<std::pair<std::string, bool>> document_at(
    db::io::connection& c, const db::types::view_filter& v,
    const std::string& environment_id, const std::string& path);

/**
 * @brief The documents inside @p v that link to @p r.id, each link with the
 * heading it sits under; nothing when the document itself is not inside
 * @p v in the environment.
 * @throws db::query::unknown_environment when the environment does not exist.
 */
[[nodiscard]] std::optional<messages::backlinks_response> backlinks(
    db::io::connection& c, const db::types::view_filter& v,
    const messages::backlinks_request& r);

/**
 * @brief Every environment the database holds, by name, with how many
 * paths each places.
 */
[[nodiscard]] messages::list_environments_response list_environments(
    db::io::connection& c);

/**
 * @brief The run @p r.run of a campaign of @p r's environment, with every
 * step of the scenario it tests and the result recorded for each; nothing
 * when the environment has no such run.
 * @throws db::query::unknown_environment when the environment does not exist.
 */
[[nodiscard]] std::optional<messages::get_run_response> get_run(
    db::io::connection& c, const messages::get_run_request& r);

/**
 * @brief The notes of the campaign @p r.campaign of @p r's environment, in
 * the order they were made; nothing when the environment has no such
 * campaign.
 * @throws db::query::unknown_environment when the environment does not exist.
 */
[[nodiscard]] std::optional<messages::list_notes_response> list_notes(
    db::io::connection& c, const messages::list_notes_request& r);

}

#endif
