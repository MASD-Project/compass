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
#ifndef COMPASS_SERVE_QUERY_CONVERT_HPP
#define COMPASS_SERVE_QUERY_CONVERT_HPP

#include <string>
#include <string_view>
#include <type_traits>
#include "compass.db/codec/text_form.hpp"
#include "compass.db/types/search.hpp"
#include "compass.db/types/testing.hpp"
#include "compass.serve/messages/document_queries_protocol.hpp"
#include "compass.serve/messages/testing_queries_protocol.hpp"

/**
 * @file convert.hpp
 * @brief The generated messages as compass.db's queries take and answer
 * them. The queries were written against hand-written structs of the same
 * members before the messages were generated, and keep them until they move
 * to the messages.
 */
namespace compass::serve::query {

/// Sets @p into from text the database holds: as is, or as its enumerator.
template <typename T>
void assign_text(T& into, std::string_view text) {
    if constexpr (std::is_enum_v<T>)
        into = compass::db::codec::text_form<T>::from_text(text);
    else
        into = std::string(text);
}

/// The text of @p v: itself, or its enumerator's name.
template <typename T>
[[nodiscard]] std::string text_of(const T& v) {
    if constexpr (std::is_enum_v<T>)
        return compass::db::codec::text_form<T>::to_text(v);
    else
        return v;
}

[[nodiscard]] db::types::document_filter to_db(const messages::document_filter& f);
[[nodiscard]] db::types::search_request to_db(const messages::search_request& r);
[[nodiscard]] db::types::facets_request to_db(const messages::facets_request& r);
[[nodiscard]] db::types::sprint_request to_db(const messages::sprint_state_request& r);
[[nodiscard]] db::types::sprint_request to_db(const messages::what_needs_testing_request& r);

[[nodiscard]] messages::search_response to_message(const db::types::search_response& r);
[[nodiscard]] messages::facets_response to_message(const db::types::facets_response& r);
[[nodiscard]] messages::sprint_state_response
to_message(const db::types::sprint_state_response& r);
[[nodiscard]] messages::testing_response to_message(const db::types::testing_response& r);

}

#endif
