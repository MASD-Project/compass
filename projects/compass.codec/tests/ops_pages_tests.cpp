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
#include "compass.codec/ops/pages.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "test_corpus.hpp"

namespace {

using namespace compass::codec;

using sources = std::map<std::string, std::string>;
using counts = std::vector<std::uint64_t>;

constexpr std::array<types::rule, 1> rules{{{"unreadable", ""}}};
constexpr std::array<types::option, 1> options{{{"page", "", ""}}};

/**
 * @brief Rewrites the page the option names, wherever it is.
 */
void stamp(const types::corpus_view& v, types::file_output& out) {
    const auto page = ops::page_of(v, "page");
    if (!page) {
        out.refusals.add({.document = compass::org::types::document_id::none(),
            .range = {}, .rule = types::rule_id{0},
            .level = types::severity::error, .message = page.error(),
            .fix = "", .file = ".compass.org"});
        return;
    }
    out.files.add(page->path, std::string(page->source) + "stamped\n");
}

const std::array<types::descriptor, 1> codecs{{{.name = "stamp",
    .version = 1, .summary = "",
    .reads = types::inputs::every_document(), .options = options,
    .rules = rules, .enabled_by_default = true,
    .run = types::corpus_function<types::file_output>{stamp}}}};

sources base() {
    return {{"notes/old_title.org", ":PROPERTIES:\n:ID: P-1\n:END:\n"},
        {"notes/other.org", ":PROPERTIES:\n:ID: P-2\n:END:\n"}};
}

types::run_result run(const sources& docs, std::string_view page,
    const types::dependency_record& previous = {}) {
    tests::test_corpus t;
    for (const auto& [path, source] : docs)
        t.add(path, source);
    types::configuration settings;
    settings.set("stamp", "page", page);
    const auto r = ops::make_registry(codecs, t.types(), settings);
    return ops::run(r, t.corpus(), t.types(), 2, previous);
}

std::vector<std::string> paths(const types::run_result& r) {
    std::vector<std::string> out;
    for (std::uint32_t i = 0; i < r.files[0].size(); ++i)
        out.emplace_back(r.files[0].paths[{i}]);
    return out;
}

std::vector<std::string> messages(const types::run_result& r) {
    std::vector<std::string> out;
    for (std::uint32_t i = 0; i < r.diagnostics[0].size(); ++i)
        out.emplace_back(r.diagnostics[0].messages[{i}]);
    return out;
}

}

TEST_CASE("a page named by its ID is found at the path it moved to, and "
    "the unit that found it runs again", "[ops][pages]") {
    const auto first = run(base(), "P-1");
    CHECK(paths(first) == std::vector<std::string>{"notes/old_title.org"});

    auto moved = base();
    moved.erase("notes/old_title.org");
    moved["notes/new_title.org"] = ":PROPERTIES:\n:ID: P-1\n:END:\n";
    const auto second = run(moved, "P-1", first.record);

    CHECK(second.ran == counts{1});
    CHECK(paths(second) == std::vector<std::string>{"notes/new_title.org"});
}

TEST_CASE("a unit that found a page by its ID is not run again when "
    "another page moves", "[ops][pages]") {
    const auto first = run(base(), "P-1");

    auto moved = base();
    moved.erase("notes/other.org");
    moved["elsewhere/other.org"] = ":PROPERTIES:\n:ID: P-2\n:END:\n";
    const auto second = run(moved, "P-1", first.record);

    CHECK(second.ran == counts{0});
    CHECK(second.skipped == counts{1});
}

TEST_CASE("a page option that is empty, or names an ID no document has, "
    "is reported and nothing is written", "[ops][pages]") {
    const auto unset = run(base(), "");
    CHECK(paths(unset).empty());
    CHECK(messages(unset) == std::vector<std::string>{"the option 'page' "
        "names no page; set it to the :ID: of the page"});

    const auto missing = run(base(), "P-404");
    CHECK(paths(missing).empty());
    CHECK(messages(missing) == std::vector<std::string>{"no document has "
        "the :ID: P-404 that the option 'page' names"});
}
