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

#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::sql;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

files tree() {
    return {
        {"projects/ores.sql/create/a/policies.sql",
         "create policy own_rows on t1\n"
         "  using (tenant_id = ores_iam_current_tenant_id_fn());\n"
         "CREATE POLICY widened on t2 using (\n"
         "  tenant_id = ores_iam_current_tenant_id_fn()\n"
         "  or ores_utility_system_tenant_id_fn() = ores_iam_current_tenant_id_fn());\n"
         "-- create policy commented on t3 using (ores_iam_current_tenant_id_fn() = ores_utility_system_tenant_id_fn());\n"
         "create policy flipped on t4 using (ores_iam_current_tenant_id_fn()\n"
         "  = ores_utility_system_tenant_id_fn() -- admin; then\n"
         ");\n"},
        {"projects/ores.sql/populate/p.sql",
         "create policy elsewhere on t5 using (ores_iam_current_tenant_id_fn()"
         " = ores_utility_system_tenant_id_fn());\n"}};
}

}

TEST_CASE("a policy that admits the system tenant to every row is "
    "reported", "[codecs][rls_widenings]") {
    const auto r = tests::run(codecs::rls_widenings, tree());
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.sql/create/a/policies.sql", 3},
        {"projects/ores.sql/create/a/policies.sql", 7}});
}
