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
#include "compass.codec/ops/outputs.hpp"

#include <stdexcept>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.codec/codecs/catalogue.hpp"
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "test_corpus.hpp"

namespace {

using namespace compass::codec;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::MessageMatches;

types::output_set set_of(
    std::vector<std::pair<std::string, std::string>> rows) {
    types::output_set s;
    for (const auto& [codec, path] : rows) {
        s.add(codec, path, "x", compass::store::ops::blob_id_of("x"));
        s.complete.push_back(codec);
    }
    return s;
}

types::written_outputs sample() {
    types::written_outputs r;
    r.root = "/tmp/out";
    r.written = {1'700'000'000, 5};
    r.paths = {"a/index.tsv", "b.txt"};
    r.codecs = {"document_index", "other"};
    r.hashes = {compass::store::ops::blob_id_of("a"),
        compass::store::ops::blob_id_of("b")};
    compass::store::types::stat_data s;
    s.mtime = {1'699'999'999, 7};
    s.ino = 42;
    s.size = 1;
    r.stats = {s, s};
    return r;
}

}

TEST_CASE("a plain relative path stays under the root",
    "[ops][outputs]") {
    CHECK(ops::is_plain_relative("index.tsv"));
    CHECK(ops::is_plain_relative("doc/generated/index.tsv"));
    CHECK(ops::is_plain_relative("..hidden/a"));
    CHECK_FALSE(ops::is_plain_relative(""));
    CHECK_FALSE(ops::is_plain_relative("/etc/passwd"));
    CHECK_FALSE(ops::is_plain_relative("../escape"));
    CHECK_FALSE(ops::is_plain_relative("a/../../escape"));
    CHECK_FALSE(ops::is_plain_relative("a/.."));
    CHECK_FALSE(ops::is_plain_relative("./a"));
    CHECK_FALSE(ops::is_plain_relative("a//b"));
    CHECK_FALSE(ops::is_plain_relative("a/"));
}

TEST_CASE("checking outputs names each escaping path and each path two "
    "codecs produce", "[ops][outputs]") {
    CHECK(ops::check_outputs(set_of({{"a", "x.txt"}, {"b", "y.txt"}}))
        .empty());

    const auto escapes = ops::check_outputs(
        set_of({{"a", "../x.txt"}, {"b", "/abs"}}));
    REQUIRE(escapes.size() == 2);
    CHECK_THAT(escapes[0], ContainsSubstring("\"../x.txt\""));
    CHECK_THAT(escapes[1], ContainsSubstring("\"/abs\""));

    const auto twice = ops::check_outputs(
        set_of({{"first", "same.txt"}, {"other", "y.txt"},
            {"second", "same.txt"}}));
    REQUIRE(twice.size() == 1);
    CHECK(twice[0] == "first and second both produced \"same.txt\"");
}

TEST_CASE("the outputs of a run carry each file's codec and blob ID, and "
    "every enabled codec is complete", "[ops][outputs]") {
    tests::test_corpus t;
    t.add("task.org", "#+type: task\n#+title: Task: a\n* Goal\n");
    const auto r = ops::make_registry(codecs::catalogue(), t.types(), {});
    const auto result = ops::run(r, t.corpus(), t.types(), 2);
    const auto out = ops::outputs_of(r, result);

    REQUIRE(out.size() == 1);
    const types::file_id f{0};
    CHECK(out.codecs[f] == "document_index");
    CHECK(out.files.paths[f] == result.files[0].paths[f]);
    CHECK(out.hashes[f] ==
        compass::store::ops::blob_id_of(out.files.contents[f]));
    CHECK(out.complete ==
        std::vector<std::string>{"document_index", "required_sections",
            "evidence_location"});

    compass::codec::types::configuration off;
    off.set("required_sections", "enabled", "false");
    const auto r2 = ops::make_registry(codecs::catalogue(), t.types(), off);
    CHECK(ops::outputs_of(r2, ops::run(r2, t.corpus(), t.types(), 2))
        .complete == std::vector<std::string>{"document_index",
            "evidence_location"});
}

TEST_CASE("the written-outputs record survives encoding",
    "[ops][outputs]") {
    const auto r = sample();
    const auto back = ops::decode_written_outputs(
        ops::encode_written_outputs(r));
    CHECK(back.root == r.root);
    CHECK(back.written == r.written);
    CHECK(back.paths == r.paths);
    CHECK(back.codecs == r.codecs);
    CHECK(back.hashes == r.hashes);
    CHECK(back.stats == r.stats);
}

TEST_CASE("a damaged written-outputs record, or one naming an escaping "
    "path, is rejected", "[ops][outputs]") {
    auto bytes = ops::encode_written_outputs(sample());
    bytes[bytes.size() / 2] ^= 0x01;
    CHECK_THROWS_MATCHES(ops::decode_written_outputs(bytes),
        std::runtime_error, MessageMatches(ContainsSubstring("checksum")));
    CHECK_THROWS_AS(ops::decode_written_outputs("CMPSOUT\n"),
        std::runtime_error);

    auto escaping = sample();
    escaping.paths[0] = "../a";
    CHECK_THROWS_MATCHES(ops::decode_written_outputs(
        ops::encode_written_outputs(escaping)), std::runtime_error,
        MessageMatches(ContainsSubstring("plain relative")));
}
