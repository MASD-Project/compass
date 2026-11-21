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
#include "compass.codec/ops/rule_table.hpp"

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.codec/ops/exceptions.hpp"
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "test_corpus.hpp"

namespace {

using namespace compass::codec;
using Catch::Matchers::ContainsSubstring;

constexpr std::string_view memory_contract = R"(#+title: Document type: memory

* Contract

- Folder :: =doc/llm/memory/=
- Level :: =cross=
)";

constexpr std::string_view knowledge_contract =
    R"(#+title: Document type: knowledge

* Contract

- Folder :: =doc/knowledge/=
- Level :: =cross=
)";

constexpr std::string_view amend_id = "5FF87481-1FF7-4AAA-A4CC-12BC3A5DE166";
constexpr std::string_view push_id = "40003D46-DAEB-41DF-B4EE-947E223A8B8F";
constexpr std::string_view absent_id = "177EDB1C-6241-45AA-81B1-46C4B9D6329D";
constexpr std::string_view guide_id = "8B29A8D8-E004-4E87-B81D-ABEAF68F2B98";

constexpr std::array<std::string_view, 2> amend_sources{amend_id, absent_id};
constexpr std::array<std::string_view, 1> guide_sources{guide_id};

constexpr std::array<types::rule, 2> history_rules{{
    {"amended", "No commit rewrites another.", amend_sources},
    {"wide|table", "A pipe | in a statement stays in its cell."}}};
constexpr std::array<types::rule, 1> layout_rules{{
    {"missing_section", "A document has its sections.", guide_sources}}};
constexpr std::array<types::rule, 1> pages_rules{{
    {"refused", "Every page is written from a source it can read."}}};

void no_diagnostics(const types::document&, const types::corpus_view&,
    types::diagnostic_table&) {}
void no_files(const types::corpus_view&, types::file_output&) {}

types::descriptor checker(std::string_view name,
    std::span<const types::rule> rules) {
    return {.name = name, .version = 1, .summary = "Checks.",
        .reads = types::inputs::every_typed_document(), .options = {},
        .rules = rules, .enabled_by_default = true,
        .run = types::document_function<types::diagnostic_table>{
            no_diagnostics}};
}

const types::descriptor history = checker("history", history_rules);
const types::descriptor layout = checker("layout", layout_rules);
const types::descriptor pages{.name = "pages", .version = 1,
    .summary = "Writes pages.", .reads = types::inputs::every_typed_document(),
    .options = {}, .rules = pages_rules, .enabled_by_default = true,
    .run = types::corpus_function<types::file_output>{no_files}};

const std::array catalogue{history, pages, layout};

void render(const types::corpus_view& v, types::file_output& out) {
    out.files.add(v.option("path"), ops::rule_table(catalogue, v));
}

const types::descriptor table{.name = "rule_table", .version = 1,
    .summary = "Writes the rule table.",
    .reads = types::inputs::every_typed_document(),
    .options = ops::rule_table_options, .rules = {},
    .enabled_by_default = true,
    .run = types::corpus_function<types::file_output>{render}};

tests::test_corpus memories() {
    tests::test_corpus t({memory_contract, knowledge_contract});
    t.add("doc/llm/memory/never_amend.org",
        ":PROPERTIES:\n:ID: " + std::string(amend_id) + "\n:END:\n"
        "#+title: Never amend commits\n#+type: memory\n");
    t.add("doc/llm/memory/no_push.org",
        ":PROPERTIES:\n:ID: " + std::string(push_id) + "\n:END:\n"
        "#+title: Do not push automatically\n#+type: memory\n");
    t.add("doc/llm/memory/a_untitled.org",
        "#+title: A memory with no ID\n#+type: memory\n");
    t.add("doc/meta/document_types.org",
        ":PROPERTIES:\n:ID: " + std::string(guide_id) + "\n:END:\n"
        "#+title: Document types\n#+type: knowledge\n");
    return t;
}

std::string run_table(const tests::test_corpus& t) {
    const std::array codecs{table};
    const auto r = ops::make_registry(codecs, t.types(), {});
    const auto result = ops::run(r, t.corpus(), t.types(), 1);
    REQUIRE(result.files[0].size() == 1);
    CHECK(result.files[0].paths[{0}] ==
        "doc/knowledge/compass/rule_table.org");
    return std::string(result.files[0].contents[{0}]);
}

}

TEST_CASE("the rule table lists each rule with its codec, statement and the "
    "pages that state it", "[ops][rule_table]") {
    const auto text = run_table(memories());

    CHECK_THAT(text, ContainsSubstring(
        "| =history= | amended | No commit rewrites another. | "
        "[[id:" + std::string(amend_id) + "][Never amend commits]], =" +
        std::string(absent_id) + "= |\n"));
    CHECK_THAT(text, ContainsSubstring(
        "| =history= | wide\\vert{}table | A pipe \\vert{} in a statement "
        "stays in its cell. |  |\n"));
    CHECK_THAT(text, ContainsSubstring(
        "| =layout= | missing_section | A document has its sections. | "
        "[[id:" + std::string(guide_id) + "][Document types]] |\n"));
    CHECK_THAT(text, ContainsSubstring(
        "| =exception= | expired | No exception is used past its expiry "
        "date. |"));
    CHECK_THAT(text, ContainsSubstring(
        "| /each files codec/ | drift | Every file a codec generates"));
    CHECK_THAT(text, ContainsSubstring(
        "| =pages= | refused | Every page is written from a source it can "
        "read. |  |\n"));
    CHECK(text.find("| =history= | amended") <
        text.find("| =layout= | missing_section"));
    CHECK(text.find("| =layout=") < text.find("| =pages= | refused"));
    CHECK(text.find("| =layout=") < text.find("| =exception= | malformed"));
}

TEST_CASE("the rule table lists the memories no rule cites, by path",
    "[ops][rule_table]") {
    const auto text = run_table(memories());
    const auto section = text.substr(text.find("** Unenforced memories"));

    CHECK(section == "** Unenforced memories\n\n"
        "- =doc/llm/memory/a_untitled.org=\n"
        "- [[id:" + std::string(push_id) + "][Do not push automatically]]\n");
    CHECK_THAT(text, ContainsSubstring("10 rules, 7 of which cite a memory or "
        "a document that states them. Of the 3 memories of this repository, "
        "2 are cited by no rule."));
}

TEST_CASE("the rule table says so when every memory is cited",
    "[ops][rule_table]") {
    tests::test_corpus t({memory_contract});
    t.add("doc/llm/memory/never_amend.org",
        ":PROPERTIES:\n:ID: " + std::string(amend_id) + "\n:END:\n"
        "#+title: Never amend commits\n#+type: memory\n");
    const auto text = run_table(t);
    CHECK(text.ends_with("** Unenforced memories\n\n"
        "Every memory is cited by some rule.\n"));
}

TEST_CASE("the rule table's version follows the rules it lists",
    "[ops][rule_table]") {
    constexpr std::array<std::string_view, 1> other_sources{push_id};
    const std::array<types::rule, 2> recited{{
        {"amended", "No commit rewrites another.", other_sources},
        history_rules[1]}};
    auto changed = history;
    changed.rules = recited;
    const std::array<types::descriptor, 3> recited_catalogue{changed, pages,
        layout};
    const std::array<types::descriptor, 3> reordered{layout, history, pages};
    const std::array<types::descriptor, 2> no_files{history, layout};

    const auto version = ops::rule_table_version(catalogue);
    CHECK(ops::rule_table_version(catalogue) == version);
    CHECK(ops::rule_table_version(reordered) == version);
    CHECK(ops::rule_table_version(recited_catalogue) != version);
    CHECK(ops::rule_table_version(no_files) != version);
}
