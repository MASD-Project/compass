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
#include "compass.crosscheck/codecs/convention_kind_terms.hpp"
#include "compass.crosscheck/codecs/conventions_coverage.hpp"
#include "compass.crosscheck/codecs/conventions_dataset.hpp"
#include "compass.crosscheck/codecs/curve_sections_wiring.hpp"
#include "compass.crosscheck/codecs/entity_binary_types.hpp"
#include "compass.crosscheck/codecs/er_diagram.hpp"
#include "compass.crosscheck/codecs/entity_writes.hpp"
#include "compass.crosscheck/codecs/generated_column_refs.hpp"
#include "compass.crosscheck/codecs/marketdata_identity_columns.hpp"
#include "compass.crosscheck/codecs/model_drift.hpp"
#include "compass.crosscheck/codecs/model_rules.hpp"
#include "compass.crosscheck/codecs/open_reads.hpp"
#include "compass.crosscheck/codecs/ore_output_catalogue.hpp"
#include "compass.crosscheck/codecs/ore_renderings_tables.hpp"
#include "compass.crosscheck/codecs/protocol_twins.hpp"
#include "compass.crosscheck/codecs/registry_gates.hpp"
#include "compass.crosscheck/codecs/route_inventory.hpp"
#include "compass.crosscheck/codecs/script_library_seed.hpp"
#include "compass.crosscheck/codecs/subject_conformance.hpp"
#include "compass.crosscheck/codecs/template_sections.hpp"
#include "compass.crosscheck/codecs/value_sets.hpp"

namespace compass::crosscheck::codecs {

std::span<const compass::codec::types::descriptor> catalogue() noexcept {
    static const auto all = [] {
        std::array<compass::codec::types::descriptor, 25> out{
            convention_kind_terms, conventions_coverage, conventions_dataset,
            curve_column_guards, curve_sections_wiring, entity_binary_types,
            entity_writes, er_diagram, generated_column_refs, http_areas,
            marketdata_identity_columns, model_drift, model_rules,
            open_reads, ore_output_catalogue, ore_renderings_tables,
            ore_value_sets, parameter_value_sets, protocol_twins,
            registry_exceptions, route_inventory, route_units,
            script_library_seed, subject_conformance, template_sections};
        for (auto& d : out)
            d.code = code_hash();
        return out;
    }();
    return all;
}

}
