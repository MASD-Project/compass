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
#include "compass.store/ops/pack.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.store/ops/blob_hash.hpp"

namespace {

using namespace compass::store;
using types::entry_kind;

/**
 * @brief A whole pack built from bodies, as a writer lays it out.
 */
struct built_pack {
    std::string bytes;
    std::vector<types::pack_entry> entries;

    void add(entry_kind kind, const types::blob_id& key,
        const std::string& body, std::int64_t written = 1) {
        if (bytes.empty())
            bytes = ops::encode_pack_header();
        bytes.append(ops::padding_for(bytes.size()), '\0');
        entries.push_back({key, kind, bytes.size(), body.size(), written});
        bytes += body;
    }

    std::string finish() {
        bytes.append(ops::padding_for(bytes.size()), '\0');
        return bytes + ops::encode_pack_index(entries, bytes.size());
    }
};

std::span<const std::byte> span_of(const std::string& s) {
    return std::as_bytes(std::span(s));
}

std::string body_of(const ops::pack_view& p, const types::pack_entry& e) {
    const auto b = ops::bytes_of(p, e);
    return {reinterpret_cast<const char*>(b.data()), b.size()};
}

types::blob_id key_starting(std::uint8_t first, std::uint8_t last) {
    types::blob_id id;
    id.bytes.fill(0x11);
    id.bytes[0] = first;
    id.bytes[19] = last;
    return id;
}

}

TEST_CASE("every entry of a pack is found by kind and key and nothing else is",
    "[ops][pack]") {
    built_pack b;
    std::vector<types::blob_id> keys;
    for (const std::uint8_t first : {0x00, 0x00, 0x7f, 0xff, 0xff}) {
        keys.push_back(key_starting(first, static_cast<std::uint8_t>(
            keys.size())));
        b.add(entry_kind::result, keys.back(),
            "body " + std::to_string(keys.size()));
    }
    b.add(entry_kind::object, keys[0], "the object of key 0");
    const auto file = b.finish();

    const auto p = ops::parse_pack(span_of(file));
    REQUIRE(p);
    CHECK(p->count == 6);
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const auto e = ops::find_entry(*p, entry_kind::result, keys[i]);
        REQUIRE(e);
        CHECK(body_of(*p, *e) == "body " + std::to_string(i + 1));
        CHECK(e->offset % ops::pack_alignment == 0);
    }
    const auto object = ops::find_entry(*p, entry_kind::object, keys[0]);
    REQUIRE(object);
    CHECK(body_of(*p, *object) == "the object of key 0");
    CHECK_FALSE(ops::find_entry(*p, entry_kind::manifest, keys[0]));
    CHECK_FALSE(ops::find_entry(*p, entry_kind::object, keys[1]));
    CHECK_FALSE(ops::find_entry(*p, entry_kind::result,
        key_starting(0x7f, 99)));
}

TEST_CASE("of two entries with one kind and key the index keeps the later "
    "written", "[ops][pack]") {
    built_pack b;
    const auto key = ops::blob_id_of("manifest");
    b.add(entry_kind::manifest, key, "newer", 20);
    b.add(entry_kind::manifest, key, "older", 10);
    const auto file = b.finish();

    const auto p = ops::parse_pack(span_of(file));
    REQUIRE(p);
    CHECK(p->count == 1);
    const auto e = ops::find_entry(*p, entry_kind::manifest, key);
    REQUIRE(e);
    CHECK(body_of(*p, *e) == "newer");
    CHECK(e->written == 20);
}

TEST_CASE("a pack with no entries parses and finds nothing", "[ops][pack]") {
    built_pack b;
    b.bytes = ops::encode_pack_header();
    const auto file = b.finish();
    const auto p = ops::parse_pack(span_of(file));
    REQUIRE(p);
    CHECK(p->count == 0);
    CHECK_FALSE(ops::find_entry(*p, entry_kind::object,
        ops::blob_id_of("x")));
}

TEST_CASE("a damaged pack is rejected before any entry is read",
    "[ops][pack]") {
    built_pack b;
    b.add(entry_kind::result, key_starting(1, 1), "one");
    b.add(entry_kind::result, key_starting(2, 2), "two");
    const auto good = b.finish();
    REQUIRE(ops::parse_pack(span_of(good)));
    const auto index_offset = good.size() - 32 - 1024 - 2 * 48;

    const auto rejected = [](std::string bytes) {
        return !ops::parse_pack(span_of(bytes)).has_value();
    };
    CHECK(rejected(good.substr(0, good.size() - 1)));
    CHECK(rejected(good + '\0'));
    CHECK(rejected("CMPSPAK"));
    auto magic = good;
    magic[0] = 'X';
    CHECK(rejected(magic));
    auto version = good;
    version[8] = 2;
    CHECK(rejected(version));
    auto footer = good;
    footer[footer.size() - 1] = 'X';
    CHECK(rejected(footer));
    auto order = good;
    std::swap(order[index_offset + 1024],
        order[index_offset + 1024 + 48]);
    CHECK(rejected(order));
    auto kind = good;
    kind[index_offset + 1024 + 20] = 9;
    CHECK(rejected(kind));
    auto outside = good;
    outside[index_offset + 1024 + 32 + 4] = 1;
    CHECK(rejected(outside));
    auto misaligned = good;
    misaligned[index_offset + 1024 + 24] = 17;
    CHECK(rejected(misaligned));
    auto fanout = good;
    fanout[index_offset] = 2;
    CHECK(rejected(fanout));
}

TEST_CASE("a geometric repack merges the small packs until each pack left "
    "is twice all those below it", "[ops][pack]") {
    const auto merge = [](std::vector<std::uint64_t> sizes) {
        return ops::packs_to_merge(sizes);
    };
    CHECK(merge({}) == 0);
    CHECK(merge({100}) == 0);
    CHECK(merge({10, 100}) == 0);
    CHECK(merge({10, 10}) == 2);
    CHECK(merge({1, 1, 1000}) == 2);
    CHECK(merge({1, 2, 4, 8, 1000}) == 4);
    CHECK(merge({1, 2, 6, 18, 1000}) == 0);
    CHECK(merge({5, 1000, 1000}) == 3);
}
