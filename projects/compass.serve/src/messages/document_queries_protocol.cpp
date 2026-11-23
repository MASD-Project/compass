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
 * Template: masd_cpp_protocol.cpp.mustache
 * To modify, update the template and regenerate.
 */
#include "compass.serve/messages/document_queries_protocol.hpp"

#include "compass.db/codec/struct_codec.hpp"

namespace compass::db::codec {

std::string wire_traits<compass::serve::messages::metadata_match>::write_json(const compass::serve::messages::metadata_match& v) {
    return object::write_json(v);
}

compass::serve::messages::metadata_match wire_traits<compass::serve::messages::metadata_match>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::metadata_match>(text);
}

std::string wire_traits<compass::serve::messages::document_filter>::write_json(const compass::serve::messages::document_filter& v) {
    return object::write_json(v);
}

compass::serve::messages::document_filter wire_traits<compass::serve::messages::document_filter>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::document_filter>(text);
}

std::string wire_traits<compass::serve::messages::search_request>::write_json(const compass::serve::messages::search_request& v) {
    return object::write_json(v);
}

compass::serve::messages::search_request wire_traits<compass::serve::messages::search_request>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::search_request>(text);
}

std::string wire_traits<compass::serve::messages::search_hit>::write_json(const compass::serve::messages::search_hit& v) {
    return object::write_json(v);
}

compass::serve::messages::search_hit wire_traits<compass::serve::messages::search_hit>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::search_hit>(text);
}

std::string wire_traits<compass::serve::messages::search_response>::write_json(const compass::serve::messages::search_response& v) {
    return object::write_json(v);
}

compass::serve::messages::search_response wire_traits<compass::serve::messages::search_response>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::search_response>(text);
}

std::string wire_traits<compass::serve::messages::facets_request>::write_json(const compass::serve::messages::facets_request& v) {
    return object::write_json(v);
}

compass::serve::messages::facets_request wire_traits<compass::serve::messages::facets_request>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::facets_request>(text);
}

std::string wire_traits<compass::serve::messages::facet_count>::write_json(const compass::serve::messages::facet_count& v) {
    return object::write_json(v);
}

compass::serve::messages::facet_count wire_traits<compass::serve::messages::facet_count>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::facet_count>(text);
}

std::string wire_traits<compass::serve::messages::named_facet>::write_json(const compass::serve::messages::named_facet& v) {
    return object::write_json(v);
}

compass::serve::messages::named_facet wire_traits<compass::serve::messages::named_facet>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::named_facet>(text);
}

std::string wire_traits<compass::serve::messages::facets_response>::write_json(const compass::serve::messages::facets_response& v) {
    return object::write_json(v);
}

compass::serve::messages::facets_response wire_traits<compass::serve::messages::facets_response>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::facets_response>(text);
}

std::string wire_traits<compass::serve::messages::document_summary>::write_json(const compass::serve::messages::document_summary& v) {
    return object::write_json(v);
}

compass::serve::messages::document_summary wire_traits<compass::serve::messages::document_summary>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::document_summary>(text);
}

std::string wire_traits<compass::serve::messages::list_documents_request>::write_json(const compass::serve::messages::list_documents_request& v) {
    return object::write_json(v);
}

compass::serve::messages::list_documents_request wire_traits<compass::serve::messages::list_documents_request>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::list_documents_request>(text);
}

std::string wire_traits<compass::serve::messages::list_documents_response>::write_json(const compass::serve::messages::list_documents_response& v) {
    return object::write_json(v);
}

compass::serve::messages::list_documents_response wire_traits<compass::serve::messages::list_documents_response>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::list_documents_response>(text);
}

std::string wire_traits<compass::serve::messages::get_document_request>::write_json(const compass::serve::messages::get_document_request& v) {
    return object::write_json(v);
}

compass::serve::messages::get_document_request wire_traits<compass::serve::messages::get_document_request>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::get_document_request>(text);
}

std::string wire_traits<compass::serve::messages::get_document_response>::write_json(const compass::serve::messages::get_document_response& v) {
    return object::write_json(v);
}

compass::serve::messages::get_document_response wire_traits<compass::serve::messages::get_document_response>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::get_document_response>(text);
}

std::string wire_traits<compass::serve::messages::backlinks_request>::write_json(const compass::serve::messages::backlinks_request& v) {
    return object::write_json(v);
}

compass::serve::messages::backlinks_request wire_traits<compass::serve::messages::backlinks_request>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::backlinks_request>(text);
}

std::string wire_traits<compass::serve::messages::backlink>::write_json(const compass::serve::messages::backlink& v) {
    return object::write_json(v);
}

compass::serve::messages::backlink wire_traits<compass::serve::messages::backlink>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::backlink>(text);
}

std::string wire_traits<compass::serve::messages::backlinks_response>::write_json(const compass::serve::messages::backlinks_response& v) {
    return object::write_json(v);
}

compass::serve::messages::backlinks_response wire_traits<compass::serve::messages::backlinks_response>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::backlinks_response>(text);
}

std::string wire_traits<compass::serve::messages::list_environments_request>::write_json(const compass::serve::messages::list_environments_request& v) {
    return object::write_json(v);
}

compass::serve::messages::list_environments_request wire_traits<compass::serve::messages::list_environments_request>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::list_environments_request>(text);
}

std::string wire_traits<compass::serve::messages::environment_summary>::write_json(const compass::serve::messages::environment_summary& v) {
    return object::write_json(v);
}

compass::serve::messages::environment_summary wire_traits<compass::serve::messages::environment_summary>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::environment_summary>(text);
}

std::string wire_traits<compass::serve::messages::list_environments_response>::write_json(const compass::serve::messages::list_environments_response& v) {
    return object::write_json(v);
}

compass::serve::messages::list_environments_response wire_traits<compass::serve::messages::list_environments_response>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::list_environments_response>(text);
}

std::string wire_traits<compass::serve::messages::version_request>::write_json(const compass::serve::messages::version_request& v) {
    return object::write_json(v);
}

compass::serve::messages::version_request wire_traits<compass::serve::messages::version_request>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::version_request>(text);
}

std::string wire_traits<compass::serve::messages::version_response>::write_json(const compass::serve::messages::version_response& v) {
    return object::write_json(v);
}

compass::serve::messages::version_response wire_traits<compass::serve::messages::version_response>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::version_response>(text);
}

std::string wire_traits<compass::serve::messages::sync_request>::write_json(const compass::serve::messages::sync_request& v) {
    return object::write_json(v);
}

compass::serve::messages::sync_request wire_traits<compass::serve::messages::sync_request>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::sync_request>(text);
}

std::string wire_traits<compass::serve::messages::sync_response>::write_json(const compass::serve::messages::sync_response& v) {
    return object::write_json(v);
}

compass::serve::messages::sync_response wire_traits<compass::serve::messages::sync_response>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::sync_response>(text);
}

}
