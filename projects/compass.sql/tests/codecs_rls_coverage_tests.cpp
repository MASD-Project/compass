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
#include "compass.sql/codecs/rls_coverage.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::sql;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

constexpr std::string_view root = "projects/ores.sql/create/";

std::string table(std::string_view name, std::string_view columns) {
    return "create table if not exists \"" + std::string(name) + "\" (\n" +
        std::string(columns) + "\n);\n";
}

files tree() {
    const std::string tenant = "    tenant_id uuid not null,\n    x int";
    const std::string party = "    tenant_id uuid not null,\n"
        "    party_id uuid not null,\n    x int";
    return {
        {std::string(root) + "trading/trading_isolated_create.sql",
         "-- A table with both policies.\n" +
         table("ores_trading_isolated_tbl", party) +
         "alter table ores_trading_isolated_tbl enable row level security;\n"
         "create policy p_party on \"ores_trading_isolated_tbl\"\n"
         "  as restrictive for all using (party_id = any(visible()));\n"},
        {std::string(root) + "trading/trading_cash_create.sql",
         table("ores_trading_cash_tbl", party) +
         "create policy p_tenant on ores_trading_cash_tbl\n"
         "  as permissive using (tenant_id = current_tenant());\n"
         "create policy p_other on ores_trading_cash_tbl as restrictive "
         "using (true);\n-- party_id after the semicolon does not count\n"},
        {std::string(root) + "trading/trading_rls_policies_create.sql",
         "ALTER TABLE \"ores_trading_cash_tbl\" ENABLE ROW LEVEL SECURITY;\n"},
        {std::string(root) + "refdata/refdata_types_create.sql",
         "\n\n" + table("ores_refdata_types_tbl", tenant)},
        {std::string(root) + "dq/dq_one_artefact_create.sql",
         table("ores_dq_one_artefact_tbl", party)},
        {std::string(root) + "plain/plain_create.sql",
         table("ores_plain_tbl", "    x int")},
        {std::string(root) + "plain/plain_notes.sql",
         table("ores_not_a_create_script_tbl", tenant)},
        {"projects/ores.sql/utility/validation_ignore.txt",
         "# comments and blank lines are skipped\n\n"
         "RLS_00[1] ores_dq_*_artefact_tbl\n"
         "RLS_002   ores_dq_*_artefact_tbl\n"}};
}

}

TEST_CASE("a table with tenant_id and no row-level security, or party_id "
    "and no restrictive party policy, is reported where it is created",
    "[codecs][rls_coverage]") {
    const auto r = tests::run(codecs::rls_coverage, tree());
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.sql/create/refdata/refdata_types_create.sql", 3},
        {"projects/ores.sql/create/trading/trading_cash_create.sql", 1}});
    REQUIRE(r.findings.size() == 2);
    CHECK(std::get<2>(r.findings[0]) == "Table 'ores_refdata_types_tbl' has "
        "tenant_id but no ENABLE ROW LEVEL SECURITY found in any "
        "*_create.sql file");
    CHECK(std::get<2>(r.findings[1]) == "Table 'ores_trading_cash_tbl' has "
        "party_id but no AS RESTRICTIVE party isolation policy found in any "
        "*_create.sql file");
}

TEST_CASE("without the ignore file every gap is reported",
    "[codecs][rls_coverage]") {
    auto all = tree();
    all.erase("projects/ores.sql/utility/validation_ignore.txt");
    const auto r = tests::run(codecs::rls_coverage, all);
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.sql/create/dq/dq_one_artefact_create.sql", 1},
        {"projects/ores.sql/create/dq/dq_one_artefact_create.sql", 1},
        {"projects/ores.sql/create/refdata/refdata_types_create.sql", 3},
        {"projects/ores.sql/create/trading/trading_cash_create.sql", 1}});
}
