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

#include <algorithm>
#include <array>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.platform/text/regex.hpp"
#include "compass.sql/ops/scripts.hpp"

namespace compass::sql::codecs {

namespace {

namespace ct = compass::codec::types;
namespace pt = compass::platform::text;

constexpr std::array<ct::rule, 1> rules{{
    {"system_tenant_widening", "No row-level security policy lets a "
        "system-tenant session see every tenant's rows."}}};

constexpr std::string_view create_dir = "projects/ores.sql/create/";

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    static const pt::regex comment(R"(--[^\n]*)");
    static const pt::regex policy(R"(create\s+policy\s+(\w+)\s+on\s+(\w+)(.*?);)",
        {.dotall = true, .ignorecase = true});
    static const pt::regex widening(
        R"(ores_iam_current_tenant_id_fn\(\)\s*=\s*ores_utility_system_tenant_id_fn\(\))"
        R"(|ores_utility_system_tenant_id_fn\(\)\s*=\s*ores_iam_current_tenant_id_fn\(\))",
        {.ignorecase = true});
    for (const auto p : v.list_files(create_dir, ".sql")) {
        const auto text = v.read_file(p).value_or("");
        // Blanked rather than removed, so offsets hold; the policies and
        // their bodies match as they do once the comments are gone.
        const auto sql = comment.substitute(text, [](const auto& m) {
            return std::string(m.group().size(), ' ');
        });
        for (const auto& m : policy.find_all(sql)) {
            if (!widening.search(m.group(3)))
                continue;
            out.add({.document = compass::org::types::document_id::none(),
                .range = {static_cast<std::uint32_t>(m.start()),
                    static_cast<std::uint32_t>(m.start())},
                .rule = ct::rule_id{0},
                .level = ct::severity::error,
                .message = "policy " + std::string(m.group(1)) + " on " +
                    std::string(m.group(2)) + " lets a system-tenant "
                    "session see every tenant, which hands one tenant's "
                    "rows to another",
                .fix = "act inside the tenant, or except the policy inline "
                    "with its reason",
                .file = p});
        }
    }
}

}

const ct::descriptor rls_widenings{
    .name = "rls_widenings",
    .version = 1,
    .summary = "Reports each row-level security policy that admits the "
        "system tenant to every tenant's rows.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}
