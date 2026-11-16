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
#include "compass.cpp/codecs/enum_attributes.hpp"
#include "compass.cpp/codecs/exported_types.hpp"
#include "compass.cpp/codecs/portable_stop.hpp"
#include "compass.cpp/codecs/test_case_reachability.hpp"
#include "compass.cpp/codecs/unity_collisions.hpp"

namespace compass::cpp::codecs {

std::span<const compass::codec::types::descriptor> catalogue() noexcept {
    static const std::array<compass::codec::types::descriptor, 5> all{
        enum_attributes, exported_types, portable_stop,
        test_case_reachability, unity_collisions};
    return all;
}

}
