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
#include "compass.crosscheck/codecs/catalogue.hpp"

#include <array>
#include "compass.crosscheck/codecs/entity_binary_types.hpp"
#include "compass.crosscheck/codecs/entity_writes.hpp"
#include "compass.crosscheck/codecs/generated_column_refs.hpp"
#include "compass.crosscheck/codecs/model_drift.hpp"
#include "compass.crosscheck/codecs/model_sql_types.hpp"
#include "compass.crosscheck/codecs/money_decimal_columns.hpp"
#include "compass.crosscheck/codecs/open_reads.hpp"
#include "compass.crosscheck/codecs/paste_blocks.hpp"
#include "compass.crosscheck/codecs/protocol_twins.hpp"
#include "compass.crosscheck/codecs/registry_gates.hpp"
#include "compass.crosscheck/codecs/subject_conformance.hpp"
#include "compass.crosscheck/codecs/template_sections.hpp"

namespace compass::crosscheck::codecs {

std::span<const compass::codec::types::descriptor> catalogue() noexcept {
    static const std::array<compass::codec::types::descriptor, 14> all{
        entity_binary_types, entity_writes, generated_column_refs,
        http_areas, model_drift, model_sql_types, money_decimal_columns,
        open_reads, paste_blocks, protocol_twins, registry_exceptions,
        route_units, subject_conformance, template_sections};
    return all;
}

}
