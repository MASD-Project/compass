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
#include "compass.codec/codecs/catalogue.hpp"

#include <array>
#include "compass.codec/codecs/document_index.hpp"
#include "compass.codec/codecs/evidence_location.hpp"
#include "compass.codec/codecs/required_sections.hpp"
#include "compass.codec/codecs/tangle.hpp"
#include "compass.codec/codecs/tangle_check.hpp"

namespace compass::codec::codecs {

std::span<const types::descriptor> catalogue() noexcept {
    static const auto all = [] {
        std::array<types::descriptor, 5> out{
            document_index, evidence_location, required_sections, tangle,
            tangle_check};
        for (auto& d : out)
            d.code = code_hash();
        return out;
    }();
    return all;
}

}
