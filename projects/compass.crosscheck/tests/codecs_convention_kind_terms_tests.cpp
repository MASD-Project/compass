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
#include "compass.crosscheck/codecs/convention_kind_terms.hpp"

#include <string>
#include <catch2/catch_test_macros.hpp>
#include "compass.crosscheck/ops/python_literals.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using tests::files;

const std::string script_path =
    "projects/ores.sql/utility/generate_convention_kind_terms.py";
const std::string populate_path = "projects/ores.sql/populate/refdata/"
    "refdata_convention_kind_terms_populate.sql";
const std::string create_path = "projects/ores.sql/create/refdata/"
    "refdata_convention_terms_validate_create.sql";
const std::string drop_path = "projects/ores.sql/drop/refdata/"
    "refdata_convention_terms_validate_drop.sql";
const std::string fx_path =
    "projects/ores.ore/core/src/domain/conventions_fx_projection.cpp";

// generate_convention_kind_terms.py was run on this fixture, with its own
// tables; the expected texts are what it wrote.
const std::string script = R"py(# The asset classes a kind may serve; a screen draws its kinds under them.
ASSET_CLASSES = ("interest-rates", "fx", "inflation", "credit", "commodity", "bond")

# A mapper read whose target is not a column of a convention entity, with why.
NOT_A_COLUMN = {
    ("CURRENCY_PAIR", "r", "spot_days"):
        "the FX projection's spot days, which the projection carries and no entity stores",
}

SCHEMA_NAME = "conventions.xsd"

# The kind a mapper file serves, where its name does not say it.
FILE_KIND = {"conventions_fx_projection.cpp": "CURRENCY_PAIR"}

# A term whose element the mapper spells otherwise than the ORE parser reads it,
# with the leaf the parser reads and why. The contract is keyed by what ORE reads.
SPELLED_OTHERWISE = {
    "BMA_BASIS_SWAP:bma_basis_swap_convention.overnight_lockout_days": {
        "xpath": "/Conventions/BMABasisSwap/OvernightLockouDays",
        "reason": "ORE's fromXML reads OvernightLockouDays and its toXML writes "
                  "OvernightLockoutDays; the mapper uses the written spelling.",
    },
}

# A block of the document another ORE parser reads whole, so its parts are not
# leaves of the conventions parser and take no term, with why.
READ_ELSEWHERE = {
    ("INFLATION_SWAP", "PublicationSchedule"):
        "a ScheduleData block that ScheduleData::fromXML reads, not the conventions parser",
}

# The element a kind is written as, where no pack_family call names it.
ELEMENT_OF = {"TENOR_BASIS_SWAP": "TenorBasisSwap",
              "COMMODITY_FUTURE": "CommodityFuture",
              "CROSS_CURRENCY_BASIS": "CrossCurrencyBasis",
              "CROSS_CURRENCY_FIX_FLOAT": "CrossCurrencyFixFloat",
              "CURRENCY_PAIR": "FX"}

# The entity a member of a projection struct holds, where a mapper reaches an
# entity through an alias, as the FX mapper reaches the currency pair.
MEMBER_ENTITY = {"pair": "currency_pair", "convention": "currency_pair_convention"}
)py";

const std::string file0_path = "projects/ores.ore/core/src/domain/conventions_family_swap.cpp";
const std::string file0 = R"f(void pack(bundle& bundle, const record_reader& root) {
    pack_family(bundle, "swap", root.children("Swap"));
}

refdata::domain::swap_convention conventions_mapper::map_swap(const record_reader& v) {
    refdata::domain::swap_convention r;
    r.fixed_calendar = text_of(v, "FixedCalendar");
    if (const auto leg = v.child("FloatLeg")) {
        r.payload.float_index = text_of(*leg, "Index");
        assign(r.float_tenor, opt_text(leg, "Tenor"));
    }
    auto leg = v.child("Schedule");
    r.schedule_rule = text_of(leg, "Rule");
    r.fixed_calendar = text_of(v, "FixedCalendar");
    return r;
}
)f";

const std::string file1_path = "projects/ores.ore/core/src/domain/conventions_family_bma_basis_swap.cpp";
const std::string file1 = R"f(refdata::domain::bma_basis_swap_convention conventions_mapper::map_bma(const record_reader& v) {
    refdata::domain::bma_basis_swap_convention r;
    r.overnight_lockout_days = integer_of(v, "OvernightLockoutDays");
    r.index = text_of(v, "Index");
    return r;
}
)f";

const std::string file2_path = "projects/ores.ore/core/src/domain/sub/conventions_family_ignored.cpp";
const std::string file2 = R"f(refdata::domain::x conventions_mapper::map_x(const record_reader& v) {
    refdata::domain::x r;
    r.a = text_of(v, "A");
}
)f";

const std::string file3_path = "projects/ores.ore/core/src/domain/conventions_fx_projection.cpp";
const std::string file3 = R"f(// fx
ores::refdata::messaging::fx_convention conventions_mapper::map_fx(const record_reader& v) {
    ores::refdata::messaging::fx_convention r;
    r.spot_days = integer_of(v, "SpotDays");
    auto& pair = r.pair;
    pair.base_currency = spelling_of(v, "SourceCurrency", "currency code");
    auto& convention = r.convention;
    assign(convention.end_of_month, opt_flag(v, "EOM"));
    return r;
}
)f";

const std::string file4_path = "projects/ores.sql/utility/convention_terms.json";
const std::string file4 = R"f({"conventions": {
  "Swap": {"terms": [
    {"name": "FixedCalendar", "path": ["Swap"], "list_item": null},
    {"name": "Index", "path": ["Swap", "FloatLeg"], "list_item": ""},
    {"name": "Tenor", "path": ["Swap", "FloatLeg"]},
    {"name": "Rule", "path": ["Swap", "Schedule"], "list_item": "Item"},
    {"name": "Rule", "path": ["Swap", "Schedule"]}]},
  "BMABasisSwap": {"terms": [{"name": "Index", "path": ["BMABasisSwap"]}]},
  "FX": {"terms": [
    {"name": "SourceCurrency", "path": ["FX"]},
    {"name": "EOM", "path": ["FX"]}]}}}
)f";

const std::string file5_path = "projects/ores.sql/populate/refdata/refdata_convention_kinds_populate.sql";
const std::string file5 = R"f(    (ores_utility_system_tenant_id_fn(), 'SWAP', 0, 'Swap',
     'Swap convention',
     'interest-rates', 'Swap',
     1),
    (ores_utility_system_tenant_id_fn(), 'BMA_BASIS_SWAP', 0, 'BMA',
     'BMA',
     'interest-rates', 'BMABasisSwap',
     1),
    (ores_utility_system_tenant_id_fn(), 'CURRENCY_PAIR', 0, 'FX',
     'FX',
     'fx', 'FX',
     1),
)f";

const std::string file6_path = "projects/ores.refdata/modeling/ores.refdata.swap_convention.org";
const std::string file6 = R"f(#+title: m
:tablename: ores_refdata_swap_conventions_tbl
)f";

const std::string file7_path = "projects/ores.refdata/modeling/ores.refdata.bma_basis_swap_convention.org";
const std::string file7 = R"f(#+title: m
:tablename: ores_refdata_bma_tbl
)f";

const std::string file8_path = "projects/ores.refdata/modeling/ores.refdata.currency_pair.org";
const std::string file8 = R"f(#+title: m
:tablename: ores_refdata_currency_pairs_tbl
)f";

const std::string file9_path = "projects/ores.refdata/modeling/ores.refdata.currency_pair_convention.org";
const std::string file9 = R"f(#+title: m
:tablename: ores_refdata_currency_pair_conventions_tbl
)f";

const std::string expected_values = R"s() values
    ('SWAP:swap_convention.fixed_calendar', ores_utility_system_tenant_id_fn(), 0, 'SWAP', 'swap_convention', 'fixed_calendar', 'conventions.xsd', '/Conventions/Swap/FixedCalendar', 0, 'ores_refdata_service', 'system.initial_load', 'Seed convention kind terms'),
    ('SWAP:swap_convention.float_index', ores_utility_system_tenant_id_fn(), 0, 'SWAP', 'swap_convention', 'float_index', 'conventions.xsd', '/Conventions/Swap/FloatLeg/Index', 1, 'ores_refdata_service', 'system.initial_load', 'Seed convention kind terms'),
    ('SWAP:swap_convention.float_tenor', ores_utility_system_tenant_id_fn(), 0, 'SWAP', 'swap_convention', 'float_tenor', 'conventions.xsd', '/Conventions/Swap/FloatLeg/Tenor', 2, 'ores_refdata_service', 'system.initial_load', 'Seed convention kind terms'),
    ('SWAP:swap_convention.schedule_rule', ores_utility_system_tenant_id_fn(), 0, 'SWAP', 'swap_convention', 'schedule_rule', 'conventions.xsd', '/Conventions/Swap/Schedule/Rule', 3, 'ores_refdata_service', 'system.initial_load', 'Seed convention kind terms'),
    ('CURRENCY_PAIR:currency_pair.base_currency', ores_utility_system_tenant_id_fn(), 0, 'CURRENCY_PAIR', 'currency_pair', 'base_currency', 'conventions.xsd', '/Conventions/FX/SourceCurrency', 0, 'ores_refdata_service', 'system.initial_load', 'Seed convention kind terms'),
    ('CURRENCY_PAIR:currency_pair_convention.end_of_month', ores_utility_system_tenant_id_fn(), 0, 'CURRENCY_PAIR', 'currency_pair_convention', 'end_of_month', 'conventions.xsd', '/Conventions/FX/EOM', 1, 'ores_refdata_service', 'system.initial_load', 'Seed convention kind terms')
on conflict)s";

const std::string expected_triggers = R"s($$ language plpgsql;

create or replace trigger ores_refdata_currency_pairs_terms_trg
before insert or update of base_currency
on ores_refdata_currency_pairs_tbl
for each row execute function ores_refdata_validate_convention_terms_fn('currency_pair');

create or replace trigger ores_refdata_currency_pair_conventions_terms_trg
before insert or update of end_of_month
on ores_refdata_currency_pair_conventions_tbl
for each row execute function ores_refdata_validate_convention_terms_fn('currency_pair_convention');

create or replace trigger ores_refdata_swap_conventions_terms_trg
before insert or update of fixed_calendar, float_index, float_tenor, schedule_rule
on ores_refdata_swap_conventions_tbl
for each row execute function ores_refdata_validate_convention_terms_fn('swap_convention');
)s";

const std::string expected_drops = R"s( */

/**
 * Drops the convention term validation triggers and their function. Generated by
 * projects/ores.sql/utility/generate_convention_kind_terms.py.
 */

drop trigger if exists ores_refdata_currency_pairs_terms_trg on ores_refdata_currency_pairs_tbl;
drop trigger if exists ores_refdata_currency_pair_conventions_terms_trg on ores_refdata_currency_pair_conventions_tbl;
drop trigger if exists ores_refdata_swap_conventions_terms_trg on ores_refdata_swap_conventions_tbl;
drop function if exists ores_refdata_validate_convention_terms_fn();
)s";


files tree() {
    return {{script_path, script},
        {file0_path, file0},
        {file1_path, file1},
        {file2_path, file2},
        {file3_path, file3},
        {file4_path, file4},
        {file5_path, file5},
        {file6_path, file6},
        {file7_path, file7},
        {file8_path, file8},
        {file9_path, file9}};
}

void replace(std::string& text, const std::string& from,
    const std::string& to) {
    const auto at = text.find(from);
    REQUIRE(at != std::string::npos);
    text.replace(at, from.size(), to);
}

}

TEST_CASE("the terms are the mapper's reads under each reader's path, with "
    "their triggers by entity", "[codecs][convention_kind_terms]") {
    const auto out = tests::generate(codecs::convention_kind_terms, tree());
    REQUIRE(out.size() == 3);
    CHECK(out.at(populate_path).find(expected_values) != std::string::npos);
    CHECK(out.at(create_path).ends_with(expected_triggers));
    CHECK(out.at(drop_path).ends_with(expected_drops));
}

TEST_CASE("a read the generator cannot attribute writes nothing until the "
    "script's own table exempts it", "[codecs][convention_kind_terms]") {
    auto t = tree();
    replace(t.at(fx_path), "    r.spot_days", "    r.id = text_of(v, \"Id\");\n"
        "    r.spot_days");
    CHECK(tests::generate(codecs::convention_kind_terms, t).empty());
    replace(t.at(script_path), "NOT_A_COLUMN = {\n",
        "NOT_A_COLUMN = {\n    (\"CURRENCY_PAIR\", \"r\", \"id\"): \"planted\",\n");
    CHECK(tests::generate(codecs::convention_kind_terms, t) ==
        tests::generate(codecs::convention_kind_terms, tree()));
}

TEST_CASE("a leaf the ORE parser does not read, or a kind outside the asset "
    "classes, writes nothing", "[codecs][convention_kind_terms]") {
    auto leaf = tree();
    replace(leaf.at("projects/ores.sql/utility/convention_terms.json"),
        "{\"name\": \"Tenor\", \"path\": [\"Swap\", \"FloatLeg\"]},", "");
    CHECK(tests::generate(codecs::convention_kind_terms, leaf).empty());
    auto kind = tree();
    replace(kind.at("projects/ores.sql/populate/refdata/"
        "refdata_convention_kinds_populate.sql"), "'fx', 'FX'", "'rates', 'FX'");
    CHECK(tests::generate(codecs::convention_kind_terms, kind).empty());
}

TEST_CASE("a dict keyed by tuples is read with each key's items",
    "[codecs][python_literals]") {
    const auto v = ops::python_assignment(
        "T = {\n    (\"A\", 'b'): \"x\" \"y\",\n    (\"C\",): \"z\",\n}\n", "T");
    REQUIRE(v);
    REQUIRE(v->entries.size() == 2);
    CHECK(v->entries[0].key.empty());
    CHECK(*ops::strings_of({ops::python_value::kind::sequence, {},
        v->entries[0].key_items, {}}) == std::vector<std::string>{"A", "b"});
    CHECK(v->entries[0].value.text == "xy");
    CHECK(v->entries[1].key_items.size() == 1);
}

TEST_CASE("a number is read as written, and a name is not one",
    "[codecs][python_literals]") {
    const auto v = ops::python_assignment(
        "T = {'a': 4.1, 'b': -2, 'c': x2}\n", "T");
    REQUIRE(v);
    REQUIRE(v->entries.size() == 3);
    CHECK(v->entries[0].value.what == ops::python_value::kind::number);
    CHECK(v->entries[0].value.text == "4.1");
    CHECK(v->entries[1].value.what == ops::python_value::kind::number);
    CHECK(v->entries[1].value.text == "-2");
    CHECK(v->entries[2].value.what == ops::python_value::kind::name);
}
