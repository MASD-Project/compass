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
#include "compass.serve/messages/campaign_writes_protocol.hpp"

#include "compass.db/codec/json_schema.hpp"
#include "compass.db/codec/struct_codec.hpp"

namespace compass::db::codec {

std::string wire_traits<compass::serve::messages::start_run_request>::write_json(const compass::serve::messages::start_run_request& v) {
    return object::write_json(v);
}

compass::serve::messages::start_run_request wire_traits<compass::serve::messages::start_run_request>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::start_run_request>(text);
}

compass::serve::messages::start_run_request wire_traits<compass::serve::messages::start_run_request>::read_body_json(std::string_view text) {
    return object::read_json(text, body_fields(std::type_identity<compass::serve::messages::start_run_request>{}));
}

std::string wire_traits<compass::serve::messages::start_run_request>::body_json_schema() {
    return json_schema(body_fields(std::type_identity<compass::serve::messages::start_run_request>{}));
}

std::string wire_traits<compass::serve::messages::run_step>::write_json(const compass::serve::messages::run_step& v) {
    return object::write_json(v);
}

compass::serve::messages::run_step wire_traits<compass::serve::messages::run_step>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::run_step>(text);
}

std::string wire_traits<compass::serve::messages::start_run_response>::write_json(const compass::serve::messages::start_run_response& v) {
    return object::write_json(v);
}

compass::serve::messages::start_run_response wire_traits<compass::serve::messages::start_run_response>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::start_run_response>(text);
}

std::string wire_traits<compass::serve::messages::step_result_request>::write_json(const compass::serve::messages::step_result_request& v) {
    return object::write_json(v);
}

compass::serve::messages::step_result_request wire_traits<compass::serve::messages::step_result_request>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::step_result_request>(text);
}

compass::serve::messages::step_result_request wire_traits<compass::serve::messages::step_result_request>::read_body_json(std::string_view text) {
    return object::read_json(text, body_fields(std::type_identity<compass::serve::messages::step_result_request>{}));
}

std::string wire_traits<compass::serve::messages::step_result_request>::body_json_schema() {
    return json_schema(body_fields(std::type_identity<compass::serve::messages::step_result_request>{}));
}

std::string wire_traits<compass::serve::messages::finish_run_request>::write_json(const compass::serve::messages::finish_run_request& v) {
    return object::write_json(v);
}

compass::serve::messages::finish_run_request wire_traits<compass::serve::messages::finish_run_request>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::finish_run_request>(text);
}

std::string wire_traits<compass::serve::messages::finish_run_response>::write_json(const compass::serve::messages::finish_run_response& v) {
    return object::write_json(v);
}

compass::serve::messages::finish_run_response wire_traits<compass::serve::messages::finish_run_response>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::finish_run_response>(text);
}

std::string wire_traits<compass::serve::messages::attach_request>::write_json(const compass::serve::messages::attach_request& v) {
    return object::write_json(v);
}

compass::serve::messages::attach_request wire_traits<compass::serve::messages::attach_request>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::attach_request>(text);
}

compass::serve::messages::attach_request wire_traits<compass::serve::messages::attach_request>::read_body_json(std::string_view text) {
    return object::read_json(text, body_fields(std::type_identity<compass::serve::messages::attach_request>{}));
}

std::string wire_traits<compass::serve::messages::attach_request>::body_json_schema() {
    return json_schema(body_fields(std::type_identity<compass::serve::messages::attach_request>{}));
}

std::string wire_traits<compass::serve::messages::attach_response>::write_json(const compass::serve::messages::attach_response& v) {
    return object::write_json(v);
}

compass::serve::messages::attach_response wire_traits<compass::serve::messages::attach_response>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::attach_response>(text);
}

std::string wire_traits<compass::serve::messages::add_note_request>::write_json(const compass::serve::messages::add_note_request& v) {
    return object::write_json(v);
}

compass::serve::messages::add_note_request wire_traits<compass::serve::messages::add_note_request>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::add_note_request>(text);
}

compass::serve::messages::add_note_request wire_traits<compass::serve::messages::add_note_request>::read_body_json(std::string_view text) {
    return object::read_json(text, body_fields(std::type_identity<compass::serve::messages::add_note_request>{}));
}

std::string wire_traits<compass::serve::messages::add_note_request>::body_json_schema() {
    return json_schema(body_fields(std::type_identity<compass::serve::messages::add_note_request>{}));
}

std::string wire_traits<compass::serve::messages::add_note_response>::write_json(const compass::serve::messages::add_note_response& v) {
    return object::write_json(v);
}

compass::serve::messages::add_note_response wire_traits<compass::serve::messages::add_note_response>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::add_note_response>(text);
}

std::string wire_traits<compass::serve::messages::address_note_request>::write_json(const compass::serve::messages::address_note_request& v) {
    return object::write_json(v);
}

compass::serve::messages::address_note_request wire_traits<compass::serve::messages::address_note_request>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::address_note_request>(text);
}

compass::serve::messages::address_note_request wire_traits<compass::serve::messages::address_note_request>::read_body_json(std::string_view text) {
    return object::read_json(text, body_fields(std::type_identity<compass::serve::messages::address_note_request>{}));
}

std::string wire_traits<compass::serve::messages::address_note_request>::body_json_schema() {
    return json_schema(body_fields(std::type_identity<compass::serve::messages::address_note_request>{}));
}

}
