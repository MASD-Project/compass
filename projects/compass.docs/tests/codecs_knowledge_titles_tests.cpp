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
#include "compass.docs/codecs/knowledge_titles.hpp"

#include <string>
#include <utility>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.codec/ops/data_tables.hpp"
#include "test_tree.hpp"

namespace {

using namespace compass::docs;
using Catch::Matchers::ContainsSubstring;
using tests::files;
using findings = std::vector<tests::finding>;

namespace cops = compass::codec::ops;

using row = std::vector<std::string>;

cops::data_table table(std::string name, std::vector<std::string> columns,
    std::vector<row> rows) {
    cops::data_table t;
    t.name = std::move(name);
    t.line = 1;
    t.columns = std::move(columns);
    t.rows = std::move(rows);
    for (std::size_t k = 0; k < t.rows.size(); ++k)
        t.row_lines.push_back(static_cast<std::uint32_t>(10 + k));
    return t;
}

cops::data_tables tables_with(std::vector<row> settings) {
    cops::data_tables t;
    t.source = "compass/checks/knowledge_titles.org";
    t.tables.push_back(table("Finite verbs", {"verb"},
        {{"is"}, {"are"}, {"publishes"}}));
    t.tables.push_back(table("Ambiguous", {"word"}, {{"books"}, {"states"}}));
    t.tables.push_back(table("Determiners", {"word"}, {{"a"}, {"the"}}));
    t.tables.push_back(table("Skip", {"path", "reason"},
        {{"doc/prototypes/", "out of scope"}}));
    t.tables.push_back(table("Settings", {"setting", "value"},
        std::move(settings)));
    return t;
}

tests::options with(const cops::data_tables& data) {
    return {{std::string(cops::data_option),
        cops::encode_data_tables(data)}};
}

const tests::options defaults = with(tables_with(
    {{"house_style", "the house style's Titles section"}}));

std::string page(std::string_view title, std::string_view tags = ":x:") {
    return "#+title: " + std::string(title) + "\n#+type: knowledge\n"
        "#+filetags: " + std::string(tags) + "\n\nText.\n";
}

findings run(const files& t, const tests::options& o = defaults) {
    return tests::run(codecs::knowledge_titles, t, o);
}

}

TEST_CASE("a sentence title is reported on its title line, with the verb "
    "and the house style", "[codecs][knowledge_titles]") {
    const files t{{"doc/k/a_report_is_published_twice.org",
        page("A report is published twice")}};
    const auto out = run(t);
    REQUIRE(out.size() == 1);
    CHECK(std::get<1>(out[0]) == 1);
    CHECK(std::get<2>(out[0]) == "the title \"A report is published twice\" "
        "is a sentence: it holds the finite verb 'is'");
}

TEST_CASE("a noun-phrase title whose file name is its snake case passes",
    "[codecs][knowledge_titles]") {
    const files t{{"doc/k/layered_validation_of_scripted_trades.org",
        page("Layered Validation of Scripted Trades")},
        {"doc/k/twin_recipes_mapping_native_terms_to_script_inputs.org",
         page("Twin Recipes: Mapping Native Terms to Script Inputs")}};
    CHECK(run(t).empty());
}

TEST_CASE("an ambiguous word counts only when a determiner opens the title",
    "[codecs][knowledge_titles]") {
    const files plural{{"doc/k/books_and_portfolios.org",
        page("Books and portfolios")}};
    CHECK(run(plural).empty());
    const files verb{{"doc/k/the_desk_books_a_trade.org",
        page("The desk books a trade")}};
    CHECK(tests::messages(run(verb)) == std::vector<std::string>{
        "the title \"The desk books a trade\" is a sentence: it holds the "
        "finite verb 'books'"});
}

TEST_CASE("a structure note is not held to the verbs, and its file name "
    "ends in the hub suffix", "[codecs][knowledge_titles]") {
    const files hub{{"doc/k/fixings_hub.org",
        page("Fixings are where it is", ":structure_note:")}};
    CHECK(run(hub).empty());
    const files named{{"doc/k/fixings.org",
        page("Fixings", ":fx:structure_note:")}};
    CHECK(tests::messages(run(named)) == std::vector<std::string>{
        "the page is a structure note, and its file name fixings does not "
        "end in _hub"});
}

TEST_CASE("a page under a skipped path and a page of another type are not "
    "read", "[codecs][knowledge_titles]") {
    const files t{{"doc/prototypes/r/index.org",
        page("See the reports a party runs")},
        {"doc/k/note.org", "#+title: It is a story\n#+type: story\n"}};
    CHECK(run(t).empty());
}

TEST_CASE("a file name that is not the title in snake case is reported, "
    "unless the setting turns the rule off", "[codecs][knowledge_titles]") {
    const files t{{"doc/k/fx-spot.org", page("FX spot: rates & quotes")}};
    const auto out = run(t);
    REQUIRE(out.size() == 1);
    CHECK(std::get<2>(out[0]) == "the file name fx-spot is not the title in "
        "snake case, fx_spot_rates_quotes");
    CHECK(run(t, with(tables_with({{"title_file_mismatch", "no"}})))
        .empty());
}

TEST_CASE("an apostrophe stays in its word, so a possessive is no verb",
    "[codecs][knowledge_titles]") {
    const files t{{"doc/k/the_party_s_states.org",
        page("The party's states")}};
    CHECK(tests::messages(run(t)) == std::vector<std::string>{
        "the title \"The party's states\" is a sentence: it holds the finite "
        "verb 'states'"});
    const files not_is{{"doc/k/the_desk_isn_t_open.org",
        page("The desk isn't open")}};
    CHECK(run(not_is).empty());
}

TEST_CASE("a check document that is missing or wrong is one unreadable "
    "finding", "[codecs][knowledge_titles]") {
    const files t{{"doc/k/a_report_is_published.org",
        page("A report is published")}};
    const auto missing = run(t, {});
    REQUIRE(missing.size() == 1);
    CHECK_THAT(std::get<2>(missing[0]), ContainsSubstring(
        "compass/checks/knowledge_titles.org"));
    const auto wrong = run(t, with(tables_with({{"title_file_mismatch",
        "maybe"}})));
    REQUIRE(wrong.size() == 1);
    CHECK_THAT(std::get<2>(wrong[0]), ContainsSubstring("not yes or no"));
}
