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
#include "compass.crosscheck/codecs/conventions_coverage.hpp"

#include <string>
#include <catch2/catch_test_macros.hpp>
#include "compass.crosscheck/ops/md5.hpp"
#include "compass.crosscheck/ops/words.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using tests::files;

const std::string here = "projects/ores.seeder/datasets/conventions/";
const std::string tools = "tools/ore_conventions/";
const std::string examples = "external/ore/examples/";

const std::string header = "kind\tid\tsignature\tsource\toresmd_uri\n";

files tree(const std::string& dataset) {
    return {
        {here + "propose.py",
         "BDC = {\"Following\": \"F\", \"ModifiedFollowing\": \"MF\"}\n"
         "INDEX_FIELDS = (\"Index\", \"FlatIndex\")\n"},
        {here + "conventions.tsv", header + dataset},
        {here + "propose_decisions.tsv",
         "# why\norigin\tkind\tid\treason\n"
         "ORE examples\tZero\tDECIDED\tkept\n"},
        {here + "oresmd_index_map.tsv",
         "EUR-EURIBOR-3M\toresmd://x?index=ibor&ccy=EUR&tenor=3M\n"
         "EUR-EURIBOR-6M\toresmd://x?index=ibor&ccy=EUR&tenor=6M\n"
         "USD-LIBOR-3M\toresmd://x?index=ibor&ccy=USD&tenor=3M\n"},
        {tools + "decisions.tsv", "kind\tid\tdecision\tsignature\n"},
        {tools + "builtin_index_fields.tsv",
         "# mined\nname\tkind\tsettlement_days\tfixing_calendar\t"
         "day_counter\tconvention\tend_of_month\n"
         "EUR-EURIBOR\tibor\t2\tTARGET\tA360\tModifiedFollowing\ttrue\n"
         "EUR-ESTR\tovernight\t0\tTARGET\tA360\tFollowing\tfalse\n"},
        {examples + "Input/conventions.xml",
         "<Conventions>\n"
         "  <Zero><Id> Z1 </Id><TenorBased>true</TenorBased>"
         "<Nested>a <B>1</B><A/>\n  b</Nested></Zero>\n"
         "  <Zero><Id>Z2</Id><Tenor>1Y</Tenor></Zero>\n"
         "  <Zero><Id>Z2-CONVENTIONS</Id><Tenor>1Y</Tenor></Zero>\n"
         "  <Zero><Id>DECIDED</Id></Zero>\n"
         "  <Zero><Id>Z3</Id><Tenor>9Y</Tenor></Zero>\n"
         "  <Zero><Tenor>no id</Tenor></Zero>\n"
         "</Conventions>\n"},
        {examples + "Legacy/conventions.xml",
         "<Conventions><Zero><Id>Z1</Id><TenorBased>false</TenorBased>"
         "<Extra>x</Extra><Nested>q</Nested></Zero>"
         "<Zero><Id>Z3</Id><Tenor>3Y</Tenor></Zero></Conventions>\n"},
        {examples + "Input/portfolio.xml", "<Portfolio/>\n"}};
}

const std::string complete =
    "Zero\tZ1\tExtra=x ; Nested=q ; TenorBased=false\tx\t\n"
    "Zero\tZ2-CONVENTIONS\tTenor=1Y\tx\t\n"
    "Zero\tZ3\tTenor=9Y\tx\t\n"
    "IborIndex\tEUR-EURIBOR\tBusinessDayConvention=MF ; DayCounter=A360 ; "
    "EndOfMonth=true ; FixingCalendar=TARGET ; SettlementDays=2\tx\t\n"
    "OvernightIndex\tEUR-ESTR\tDayCounter=A360 ; FixingCalendar=TARGET ; "
    "SettlementDays=0\tx\t\n";

}

TEST_CASE("MD5 and Python's word split give what hashlib and str.split "
    "give", "[codecs][conventions_coverage]") {
    CHECK(ops::md5_hex("") == "d41d8cd98f00b204e9800998ecf8427e");
    CHECK(ops::md5_hex("abc") == "900150983cd24fb0d6963f7d28e17f72");
    CHECK(ops::md5_hex(std::string(64, 'a')) ==
        "014842d480b571495a4a0363793f7367");
    CHECK(ops::normalised_words("  a\n\t b\xc2\xa0" "c\xe3\x80\x80 ") ==
        "a b c");
    CHECK(ops::normalised_words("\xc2\xa9") == "\xc2\xa9");
}

TEST_CASE("a dataset that holds what ORE holds passes",
    "[codecs][conventions_coverage]") {
    CHECK(tests::run(codecs::conventions_coverage, tree(complete))
        .findings.empty());
}

TEST_CASE("the rule picks the variant with most fields, then the one "
    "outside the legacy paths, a -CONVENTIONS twin is renamed, and the "
    "engine's terms are spelled as ORE does",
    "[codecs][conventions_coverage]") {
    const auto found = tests::messages_of(tests::run(
        codecs::conventions_coverage, tree(
            "Zero\tZ1\tTenorBased=true\tx\t\n"
            "Zero\tZ2\tTenor=1Y\tx\t\n"
            "IborIndex\tEUR-EURIBOR\tSettlementDays=2\tx\t\n"
            "Zero\tZ2\tTenor=1Y\t\t\n"
            "Swap\tS\tIndex=USD-LIBOR-3M ; FlatIndex=EUR-EURIBOR-6M\tx\t\n"
            "IborIndex\tEUR-EURIBOR-3M\tX=1\tx\t\n")));
    CHECK(found == std::vector<std::string>{
        "Zero Z2: more than one row",
        "Zero Z2: names no source",
        "Swap S: names USD-LIBOR-3M, which no Ibor or overnight index "
        "convention of the dataset defines",
        "ORE examples: Zero Z1 differs: ORE states 'Extra=x ; Nested=q ; "
        "TenorBased=false', the dataset 'TenorBased=true'",
        "ORE examples: Zero Z2-CONVENTIONS is not in the dataset",
        "ORE examples: Zero Z3 is not in the dataset",
        "ORE engine: IborIndex EUR-EURIBOR differs: ORE states "
        "'BusinessDayConvention=MF ; DayCounter=A360 ; EndOfMonth=true ; "
        "FixingCalendar=TARGET ; SettlementDays=2', the dataset "
        "'SettlementDays=2'",
        "ORE engine: OvernightIndex EUR-ESTR is not in the dataset"});
}

TEST_CASE("a pin by digest decides between variants, and a stale pin, or a "
    "TODO for an absent Id, is compared with its own signature",
    "[codecs][conventions_coverage]") {
    auto t = tree(complete);
    t[tools + "decisions.tsv"] += "Zero\tZ1\t" + ops::md5_hex(
        "Nested=a/A=/B=1 ; TenorBased=true").substr(0, 10) + "\t\n"
        "Zero\tGONE\tnowhere\tTenor=2Y\n"
        "Zero\tLATER\tTODO\tTenor=5Y\n";
    t[here + "conventions.tsv"] += "Zero\tLATER\tTenor=5Y\tx\t\n";
    CHECK(tests::messages_of(tests::run(codecs::conventions_coverage, t)) ==
        std::vector<std::string>{
            "ORE examples: Zero GONE is not in the dataset",
            "ORE examples: Zero Z1 differs: ORE states 'Nested=a/A=/B=1 ; "
            "TenorBased=true', the dataset 'Extra=x ; Nested=q ; "
            "TenorBased=false'"});
}

TEST_CASE("a decisions file extract.py refuses is one finding",
    "[codecs][conventions_coverage]") {
    auto t = tree(complete);
    t[tools + "decisions.tsv"] = "kind\tid\tdecision\n";
    CHECK(tests::messages_of(tests::run(codecs::conventions_coverage, t)) ==
        std::vector<std::string>{"extract.py stops: expected the header "
            "kind, id, decision, signature"});
}
