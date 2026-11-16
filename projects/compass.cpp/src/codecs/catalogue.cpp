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
#include "compass.cpp/codecs/catalogue.hpp"

#include <array>
#include "compass.cpp/codecs/bff_list_requests.hpp"
#include "compass.cpp/codecs/enum_attributes.hpp"
#include "compass.cpp/codecs/exported_types.hpp"
#include "compass.cpp/codecs/forward_declarable_includes.hpp"
#include "compass.cpp/codecs/portable_stop.hpp"
#include "compass.cpp/codecs/registrars.hpp"
#include "compass.cpp/codecs/route_auth.hpp"
#include "compass.cpp/codecs/shell.hpp"
#include "compass.cpp/codecs/test_case_reachability.hpp"
#include "compass.cpp/codecs/translation_unit_budget.hpp"
#include "compass.cpp/codecs/unique_type_names.hpp"
#include "compass.cpp/codecs/unity_collisions.hpp"
#include "compass.cpp/codecs/xml_family_census.hpp"

namespace compass::cpp::codecs {

std::span<const compass::codec::types::descriptor> catalogue() noexcept {
    static const std::array<compass::codec::types::descriptor, 17> all{
        bff_list_requests, enum_attributes, event_registrars,
        exported_types, forward_declarable_includes, handler_registrars,
        history_registrars, portable_stop, route_auth,
        shell_command_composition, shell_command_units, shell_menus,
        test_case_reachability, translation_unit_budget, unique_type_names,
        unity_collisions, xml_family_census};
    return all;
}

}
