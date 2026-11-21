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
#include "compass.sql/codecs/rls_widenings.hpp"

#include <array>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.sql/ops/sql_model.hpp"

namespace compass::sql::codecs {

namespace {

namespace ct = compass::codec::types;

constexpr std::array<std::string_view, 3> system_tenant_widening_sources{
    "C3D570EF-468D-427D-B355-30C0443BCDDF",
    "5A5D6490-AF64-4AAF-8B3D-48EF3EDAF3D2",
    "07177FE0-F0B9-44E8-B67C-CEAA6DCC696A"};

constexpr std::array<ct::rule, 1> rules{{
    {"system_tenant_widening", "No row-level security policy lets a "
        "system-tenant session see every tenant's rows.",
        system_tenant_widening_sources, ct::blame_method::line}}};

constexpr std::string_view create_dir = "projects/ores.sql/create/";

std::vector<std::string_view> scanned(const ct::corpus_view& v) {
    return v.list_files(create_dir, ".sql");
}

void map(std::string_view path, std::string_view bytes,
    const ct::option_values&, ct::file_scan& out) {
    for (const auto& p : ops::model_of(bytes).policies) {
        if (!p.widens_to_system_tenant)
            continue;
        out.findings.add({.document = compass::org::types::document_id::none(),
            .range = {p.offset, p.offset},
            .rule = ct::rule_id{0},
            .level = ct::severity::error,
            .message = "policy " + p.name + " on " + p.table + " lets a "
                "system-tenant session see every tenant, which hands one "
                "tenant's rows to another",
            .fix = "act inside the tenant, or except the policy inline "
                "with its reason",
            .file = path});
    }
}

}

const ct::descriptor rls_widenings{
    .name = "rls_widenings",
    .version = 2,
    .summary = "Reports each row-level security policy that admits the "
        "system tenant to every tenant's rows.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::scan_function{scanned, map, nullptr}};

}
