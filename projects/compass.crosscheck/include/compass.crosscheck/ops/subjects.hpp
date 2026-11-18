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
#ifndef COMPASS_CROSSCHECK_OPS_SUBJECTS_HPP
#define COMPASS_CROSSCHECK_OPS_SUBJECTS_HPP

#include <string>
#include <string_view>
#include <utility>

namespace compass::crosscheck::ops {

/**
 * @brief Whether a NATS subject is inside the entity protocol, and why
 * not when it is outside, as check_subject_conformance.py's classify
 * decides: component.v1.resource.verb with a verb of the closed set or
 * list_by_..., component.v1.ops.operation, or
 * component.v1.events.collection.action.
 */
[[nodiscard]] std::pair<bool, std::string> classify_subject(
    std::string_view subject);

/**
 * @brief The fix a finding on a subject outside the protocol gives.
 */
inline constexpr std::string_view subject_fix = "A subject is "
    "{component}.v1.{resource}.{verb} with a verb from the closed set of "
    "eight, {component}.v1.events.{collection}.{action}, or "
    "{component}.v1.ops.{operation}; see "
    "doc/knowledge/architecture/nats_entity_protocol.org.";

}

#endif
