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
#include "compass.cli/ops/attribution.hpp"

#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.store/ops/blob_hash.hpp"

namespace {

using namespace compass::cli;
namespace ct = compass::codec::types;
using types::change_cause;

const auto one = compass::store::ops::blob_id_of("one");
const auto two = compass::store::ops::blob_id_of("two");

types::attributed_file file(std::string path, std::string codec,
    std::string unit, std::string origin = {}) {
    types::attributed_file f;
    f.path = std::move(path);
    f.codec = std::move(codec);
    f.unit = std::move(unit);
    f.origin = std::move(origin);
    f.cause = change_cause::yours;
    return f;
}

}

TEST_CASE("a file is the change's own when the checkout generates it "
    "otherwise than HEAD, or edits a file HEAD had current",
    "[ops][attribution]") {
    CHECK(ops::cause_of(two, one, one) == change_cause::yours);
    CHECK(ops::cause_of(one, std::nullopt, std::nullopt) ==
        change_cause::yours);
    CHECK(ops::cause_of(std::nullopt, one, one) == change_cause::yours);
    CHECK(ops::cause_of(one, one, one) == change_cause::yours);
    CHECK(ops::cause_of(one, one, two) == change_cause::stale);
    CHECK(ops::cause_of(one, one, std::nullopt) == change_cause::stale);
    CHECK(ops::cause_of(std::nullopt, std::nullopt, two) ==
        change_cause::stale);
}

TEST_CASE("a listing pattern is a prefix and a suffix around a star",
    "[ops][attribution]") {
    CHECK(ops::matches_listing("lib/templates/*.mustache",
        "lib/templates/a.mustache"));
    CHECK_FALSE(ops::matches_listing("lib/templates/*.mustache",
        "lib/templates/a.org"));
    CHECK(ops::matches_listing("projects/", "projects/a/b.org"));
    CHECK(ops::matches_listing("a*bc", "abc"));
    CHECK_FALSE(ops::matches_listing("a*bc", "ac"));
}

TEST_CASE("narrowing the outputs keeps the codec of a dropped file from "
    "removing the files it no longer holds", "[ops][attribution]") {
    ct::output_set outputs;
    outputs.complete = {"a", "b"};
    outputs.add("a", "x", "1", one);
    outputs.add("b", "y", "2", two);
    outputs.add("b", "z", "3", two);

    ops::narrow(outputs, [](std::size_t row) { return row != 1; });

    REQUIRE(outputs.size() == 2);
    CHECK(outputs.files.paths[ct::file_id{0}] == "x");
    CHECK(outputs.files.paths[ct::file_id{1}] == "z");
    CHECK(outputs.hashes[ct::file_id{1}] == two);
    CHECK(outputs.complete == std::vector<std::string>{"a"});
}

TEST_CASE("the units that read a path are those of its document, of a "
    "lookup of it and of a listing it matches, not of a read of every "
    "document", "[ops][attribution]") {
    ct::dependency_record r;
    const auto unit = [&](std::string_view document,
        std::vector<std::pair<ct::input_kind, std::string_view>> inputs) {
        r.codecs.add("c");
        r.documents.add(document);
        r.input_firsts.values.push_back(
            ct::input_id{static_cast<std::uint32_t>(r.input_count())});
        r.file_firsts.values.push_back(ct::file_id{0});
        for (const auto& [kind, key] : inputs) {
            r.input_kinds.values.push_back(kind);
            r.input_keys.add(key);
        }
    };
    unit("t.org", {});
    unit("", {{ct::input_kind::file, "t.org"}});
    unit("", {{ct::input_kind::listing, "lib/*.org"}});
    unit("", {{ct::input_kind::type_set, ""}});
    unit("", {{ct::input_kind::path, "u.org"}});

    const std::vector<std::string> paths{"t.org", "lib/x.org"};

    CHECK(ops::units_reading(r, paths) ==
        std::vector<std::uint8_t>{1, 1, 1, 0, 0});
}

TEST_CASE("files are grouped by codec and origin, else unit, and by codec "
    "alone when that leaves too many groups", "[ops][attribution]") {
    std::vector<types::attributed_file> files{
        file("a.hpp", "codegen", "m1.org", "t/h.mustache"),
        file("b.hpp", "codegen", "m2.org", "t/h.mustache"),
        file("a.txt", "tangle", "a.org"),
        file("b.txt", "tangle", "b.org"),
        file("c.txt", "tangle", "b.org")};
    files[4].cause = change_cause::stale;

    const auto groups = ops::groups_of(files, change_cause::yours, 5);

    REQUIRE(groups.size() == 3);
    CHECK(groups[0].first == "codegen, t/h.mustache");
    CHECK(groups[0].second == std::vector<std::size_t>{0, 1});
    CHECK(groups[1].first == "tangle, a.org");
    CHECK(groups[2].first == "tangle, b.org");

    const auto fewer = ops::groups_of(files, change_cause::yours, 2);

    REQUIRE(fewer.size() == 2);
    CHECK(fewer[0].first == "codegen");
    CHECK(fewer[1].first == "tangle");
    CHECK(fewer[1].second == std::vector<std::size_t>{2, 3});
    CHECK(ops::producer_label(files[0]) == "codegen, m1.org, t/h.mustache");
}
