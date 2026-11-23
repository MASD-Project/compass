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
#include "compass.serve/messages/testing_queries_protocol.hpp"

#include "compass.db/codec/json_schema.hpp"
#include "compass.db/codec/struct_codec.hpp"

namespace compass::db::codec {

std::string wire_traits<compass::serve::messages::sprint_state_request>::write_json(const compass::serve::messages::sprint_state_request& v) {
    return object::write_json(v);
}

compass::serve::messages::sprint_state_request wire_traits<compass::serve::messages::sprint_state_request>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::sprint_state_request>(text);
}

std::string wire_traits<compass::serve::messages::what_needs_testing_request>::write_json(const compass::serve::messages::what_needs_testing_request& v) {
    return object::write_json(v);
}

compass::serve::messages::what_needs_testing_request wire_traits<compass::serve::messages::what_needs_testing_request>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::what_needs_testing_request>(text);
}

std::string wire_traits<compass::serve::messages::sprint_summary>::write_json(const compass::serve::messages::sprint_summary& v) {
    return object::write_json(v);
}

compass::serve::messages::sprint_summary wire_traits<compass::serve::messages::sprint_summary>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::sprint_summary>(text);
}

std::string wire_traits<compass::serve::messages::work_item>::write_json(const compass::serve::messages::work_item& v) {
    return object::write_json(v);
}

compass::serve::messages::work_item wire_traits<compass::serve::messages::work_item>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::work_item>(text);
}

std::string wire_traits<compass::serve::messages::story_state>::write_json(const compass::serve::messages::story_state& v) {
    return object::write_json(v);
}

compass::serve::messages::story_state wire_traits<compass::serve::messages::story_state>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::story_state>(text);
}

std::string wire_traits<compass::serve::messages::sprint_state_response>::write_json(const compass::serve::messages::sprint_state_response& v) {
    return object::write_json(v);
}

compass::serve::messages::sprint_state_response wire_traits<compass::serve::messages::sprint_state_response>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::sprint_state_response>(text);
}

std::string wire_traits<compass::serve::messages::run_summary>::write_json(const compass::serve::messages::run_summary& v) {
    return object::write_json(v);
}

compass::serve::messages::run_summary wire_traits<compass::serve::messages::run_summary>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::run_summary>(text);
}

std::string wire_traits<compass::serve::messages::scenario_status>::write_json(const compass::serve::messages::scenario_status& v) {
    return object::write_json(v);
}

compass::serve::messages::scenario_status wire_traits<compass::serve::messages::scenario_status>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::scenario_status>(text);
}

std::string wire_traits<compass::serve::messages::story_testing>::write_json(const compass::serve::messages::story_testing& v) {
    return object::write_json(v);
}

compass::serve::messages::story_testing wire_traits<compass::serve::messages::story_testing>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::story_testing>(text);
}

std::string wire_traits<compass::serve::messages::testing_response>::write_json(const compass::serve::messages::testing_response& v) {
    return object::write_json(v);
}

compass::serve::messages::testing_response wire_traits<compass::serve::messages::testing_response>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::testing_response>(text);
}

std::string wire_traits<compass::serve::messages::get_run_request>::write_json(const compass::serve::messages::get_run_request& v) {
    return object::write_json(v);
}

compass::serve::messages::get_run_request wire_traits<compass::serve::messages::get_run_request>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::get_run_request>(text);
}

std::string wire_traits<compass::serve::messages::run_finish>::write_json(const compass::serve::messages::run_finish& v) {
    return object::write_json(v);
}

compass::serve::messages::run_finish wire_traits<compass::serve::messages::run_finish>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::run_finish>(text);
}

std::string wire_traits<compass::serve::messages::step_result>::write_json(const compass::serve::messages::step_result& v) {
    return object::write_json(v);
}

compass::serve::messages::step_result wire_traits<compass::serve::messages::step_result>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::step_result>(text);
}

std::string wire_traits<compass::serve::messages::get_run_response>::write_json(const compass::serve::messages::get_run_response& v) {
    return object::write_json(v);
}

compass::serve::messages::get_run_response wire_traits<compass::serve::messages::get_run_response>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::get_run_response>(text);
}

std::string wire_traits<compass::serve::messages::list_notes_request>::write_json(const compass::serve::messages::list_notes_request& v) {
    return object::write_json(v);
}

compass::serve::messages::list_notes_request wire_traits<compass::serve::messages::list_notes_request>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::list_notes_request>(text);
}

std::string wire_traits<compass::serve::messages::note_address>::write_json(const compass::serve::messages::note_address& v) {
    return object::write_json(v);
}

compass::serve::messages::note_address wire_traits<compass::serve::messages::note_address>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::note_address>(text);
}

std::string wire_traits<compass::serve::messages::campaign_note>::write_json(const compass::serve::messages::campaign_note& v) {
    return object::write_json(v);
}

compass::serve::messages::campaign_note wire_traits<compass::serve::messages::campaign_note>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::campaign_note>(text);
}

std::string wire_traits<compass::serve::messages::list_notes_response>::write_json(const compass::serve::messages::list_notes_response& v) {
    return object::write_json(v);
}

compass::serve::messages::list_notes_response wire_traits<compass::serve::messages::list_notes_response>::read_json(std::string_view text) {
    return object::read_json<compass::serve::messages::list_notes_response>(text);
}

}
