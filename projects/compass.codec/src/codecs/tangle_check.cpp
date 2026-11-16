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
#include "compass.codec/codecs/tangle_check.hpp"

#include <array>
#include "compass.codec/ops/tangle.hpp"
#include "compass.codec/types/corpus_view.hpp"

namespace compass::codec::codecs {

namespace {

constexpr std::array<types::rule, 2> rules{{
    {"unsupported_header_argument",
        "A tangled block sets nothing whose effect on the bytes tangling "
        "does not reproduce."},
    {"target_outside_checkout",
        "A tangled block names a target under the root of the checkout."}}};

types::rule_id rule_of(ops::tangle_fault f) noexcept {
    return types::rule_id{f == ops::tangle_fault::unsupported ? 0u : 1u};
}

void run(const types::document& d, const types::corpus_view&,
    types::diagnostic_table& out) {
    for (const auto& p : ops::tangle(d).problems)
        out.add({d.row, p.range, rule_of(p.fault), types::severity::error,
            p.message, p.fix});
}

}

const types::descriptor tangle_check{
    .name = "tangle_check",
    .version = 1,
    .summary = "Reports each source block the tangle codec leaves out.",
    .reads = types::inputs::every_document(),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = types::document_function<types::diagnostic_table>{run}};

}
