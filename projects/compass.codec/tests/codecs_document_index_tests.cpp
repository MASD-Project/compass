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
#include "compass.codec/codecs/document_index.hpp"

#include <array>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "test_corpus.hpp"

namespace {

using namespace compass::codec;

types::run_result run_index(const tests::test_corpus& t,
    const types::configuration& settings = {}) {
    const std::array codecs{codecs::document_index};
    const auto r = ops::make_registry(codecs, t.types(), settings);
    return ops::run(r, t.corpus(), t.types(), 2);
}

}

TEST_CASE("the index lists the ID, title, type and path of every document "
    "in path order", "[codecs][document_index]") {
    tests::test_corpus t;
    t.add("doc/task_b.org",
        ":PROPERTIES:\n:ID: B-2\n:END:\n"
        "#+title: Task: second\n#+type: task\n* Goal\n");
    t.add("readme.org", "Plain text.\n");
    t.add("doc/story.org",
        ":PROPERTIES:\n:ID: S-1\n:END:\n"
        "#+TITLE:  Story:\ttabbed \n#+type: story\n"
        "* Goal\n:PROPERTIES:\n:ID: HEADING\n:END:\n");
    t.add("doc/notes.org", "#+title: Notes\n#+type: scribble\n");

    const auto result = run_index(t);

    CHECK(result.documents[0] == 4);
    const auto& files = result.files[0];
    REQUIRE(files.size() == 1);
    CHECK(files.paths[{0}] == "doc/document_index.tsv");
    CHECK(std::string(files.contents[{0}]) ==
        "id\ttitle\ttype\tpath\n"
        "\tNotes\tscribble\tdoc/notes.org\n"
        "S-1\tStory: tabbed\tstory\tdoc/story.org\n"
        "B-2\tTask: second\ttask\tdoc/task_b.org\n"
        "\t\t\treadme.org\n");
    CHECK(result.diagnostics[0].size() == 0);
}

TEST_CASE("the index of an empty corpus is its header line",
    "[codecs][document_index]") {
    const tests::test_corpus t;
    const auto result = run_index(t);
    REQUIRE(result.files[0].size() == 1);
    CHECK(result.files[0].contents[{0}] == "id\ttitle\ttype\tpath\n");
}

TEST_CASE("the path option names the index file",
    "[codecs][document_index]") {
    const tests::test_corpus t;
    types::configuration settings;
    settings.set("document_index", "path", "build/index.tsv");
    const auto result = run_index(t, settings);
    REQUIRE(result.files[0].size() == 1);
    CHECK(result.files[0].paths[{0}] == "build/index.tsv");
}
