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
#include "compass.store/ops/git_object.hpp"

#include <cstddef>
#include <initializer_list>
#include <span>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include "compass.store/ops/blob_hash.hpp"

namespace {

using namespace compass::store;

std::string bytes(std::initializer_list<int> values) {
    std::string s;
    for (const auto v : values)
        s.push_back(static_cast<char>(v));
    return s;
}

std::span<const std::byte> span_of(const std::string& s) {
    return std::as_bytes(std::span(s.data(), s.size()));
}

}

TEST_CASE("an ID reads from hexadecimal of either case and nothing else",
    "[ops][git_object]") {
    const auto id = ops::from_hex("CE013625030BA8DBA906F756967F9E9CA394464A");
    REQUIRE(id);
    CHECK(ops::to_hex(*id) == "ce013625030ba8dba906f756967f9e9ca394464a");
    CHECK_FALSE(ops::from_hex("ce013625030ba8dba906f756967f9e9ca394464"));
    CHECK_FALSE(ops::from_hex("ce013625030ba8dba906f756967f9e9ca394464g"));
}

TEST_CASE("a delta copies from its base and inserts literal bytes",
    "[ops][git_object]") {
    const std::string base = "0123456789";
    // base 10, result 9: copy offset 2 size 3, insert "xy", copy offset 6
    // size 4.
    const auto delta = bytes({10, 9, 0x91, 2, 3, 2, 'x', 'y', 0x91, 6, 4});
    const auto r = ops::apply_delta(base, delta);
    REQUIRE(r);
    CHECK(*r == "234xy6789");
}

TEST_CASE("a copy of size zero copies 64 KiB", "[ops][git_object]") {
    std::string base(0x10000 + 5, 'a');
    base[0x10000 - 1] = 'z';
    // Sizes as base-128: 65541 and 65536. The copy names offset 0 and no
    // size byte.
    const auto delta = bytes({0x85, 0x80, 0x04, 0x80, 0x80, 0x04, 0x80});
    const auto r = ops::apply_delta(base, delta);
    REQUIRE(r);
    CHECK(r->size() == 0x10000);
    CHECK(r->back() == 'z');
}

TEST_CASE("a delta that does not fit its base or its result is rejected",
    "[ops][git_object]") {
    const std::string base = "0123456789";
    CHECK_FALSE(ops::apply_delta(base, bytes({11, 3, 0x91, 0, 3})));
    CHECK_FALSE(ops::apply_delta(base, bytes({10, 3, 0x91, 8, 3})));
    CHECK_FALSE(ops::apply_delta(base, bytes({10, 4, 0x91, 0, 3})));
    CHECK_FALSE(ops::apply_delta(base, bytes({10, 2, 0x91, 0, 3})));
    CHECK_FALSE(ops::apply_delta(base, bytes({10, 3, 3, 'a', 'b'})));
    CHECK_FALSE(ops::apply_delta(base, bytes({10, 1, 0})));
    CHECK_FALSE(ops::apply_delta(base, bytes({10})));
}

TEST_CASE("a tree lists its entries and rejects cut short ones",
    "[ops][git_object]") {
    const std::string id(20, '\x11');
    const auto tree = "100644 a.txt" + std::string(1, '\0') + id +
        "40000 dir" + std::string(1, '\0') + id;
    const auto entries = ops::parse_tree(tree);
    REQUIRE(entries);
    REQUIRE(entries->size() == 2);
    CHECK((*entries)[0].mode == 0100644);
    CHECK((*entries)[0].name == "a.txt");
    CHECK((*entries)[1].mode == types::tree_mode);
    CHECK((*entries)[1].id.bytes[19] == 0x11);
    CHECK_FALSE(ops::parse_tree(tree.substr(0, tree.size() - 1)));
    CHECK_FALSE(ops::parse_tree("100644a.txt" + std::string(1, '\0') + id));
    CHECK_FALSE(ops::parse_tree("10064x a" + std::string(1, '\0') + id));
}

TEST_CASE("a commit names its tree on its first line", "[ops][git_object]") {
    const auto tree = ops::tree_of_commit(
        "tree 4b825dc642cb6eb9a060e54bf8d69288fbee4904\nauthor x\n");
    REQUIRE(tree);
    CHECK(ops::to_hex(*tree) == "4b825dc642cb6eb9a060e54bf8d69288fbee4904");
    CHECK_FALSE(ops::tree_of_commit("parent 4b825dc642cb6eb9a060e54bf8d69288"
                                    "fbee4904\n"));
}

TEST_CASE("a stream that inflates to another size is rejected",
    "[ops][git_object]") {
    // zlib's stream of "hello".
    const auto hello = bytes({0x78, 0x9c, 0xcb, 0x48, 0xcd, 0xc9, 0xc9, 0x07,
        0x00, 0x06, 0x2c, 0x02, 0x15});
    const auto r = ops::inflate(span_of(hello), 5);
    REQUIRE(r);
    CHECK(*r == "hello");
    CHECK_FALSE(ops::inflate(span_of(hello), 4));
    CHECK_FALSE(ops::inflate(span_of(hello), 6));
    CHECK_FALSE(ops::inflate(span_of(hello.substr(0, 8)), 5));
}

TEST_CASE("a pack entry header reads its kind, size and base",
    "[ops][git_object]") {
    // A blob of 300 bytes: kind 3, size 300 = 0b1_0010_1100.
    const auto blob = bytes({0xbc, 0x12});
    auto e = ops::parse_pack_entry(span_of(blob), 0);
    REQUIRE(e);
    CHECK(e->kind == ops::pack_entry_kind::blob);
    CHECK(e->size == 300);
    CHECK(e->data == 2);

    // An offset delta at 200 whose base is 200 - ((0 + 1) << 7 | 2), which
    // is 70.
    std::string pack(200, '\0');
    pack += bytes({0x65, 0x80, 0x02});
    e = ops::parse_pack_entry(span_of(pack), 200);
    REQUIRE(e);
    CHECK(e->kind == ops::pack_entry_kind::offset_delta);
    CHECK(e->size == 5);
    CHECK(e->base_offset == 70);

    CHECK_FALSE(ops::parse_pack_entry(span_of(bytes({0x05})), 0));
    CHECK_FALSE(ops::parse_pack_entry(span_of(bytes({0xbc})), 0));
    CHECK_FALSE(ops::parse_pack_entry(span_of(bytes({0x65, 0x81, 0x02})), 0));
}
