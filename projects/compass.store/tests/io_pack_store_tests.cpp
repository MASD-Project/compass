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
#include "compass.store/io/pack_store.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.store/io/location.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "scoped_directory.hpp"

namespace {

using namespace compass::store;
using types::action_kind;
using types::entry_kind;

std::span<const std::byte> span_of(const std::string& s) {
    return std::as_bytes(std::span(s));
}

std::string text_of(std::span<const std::byte> b) {
    return {reinterpret_cast<const char*>(b.data()), b.size()};
}

struct listing {
    std::size_t packs = 0;
    std::size_t temporaries = 0;
    std::size_t other = 0;
};

listing list(const std::filesystem::path& dir) {
    listing l;
    for (const auto& e : std::filesystem::directory_iterator(dir)) {
        const auto name = e.path().filename().string();
        if (io::is_pack_name(name))
            ++l.packs;
        else if (name.starts_with("tmp_"))
            ++l.temporaries;
        else if (name != "lock")
            ++l.other;
    }
    return l;
}

std::string body_of(const std::string& name) {
    return "the body of " + name + std::string(name.size() * 7 % 61, '.');
}

}

TEST_CASE("a published pack is read back entry for entry, aligned",
    "[io][pack_store]") {
    tests::scoped_directory dir;
    io::pack_writer w(dir.root());
    for (int i = 0; i < 50; ++i) {
        const auto name = "entry " + std::to_string(i);
        w.add(entry_kind::object, ops::blob_id_of(name), span_of(body_of(name)));
    }
    w.add_action(action_kind::result, ops::blob_id_of("r"), "result body");
    CHECK(w.size() == 51);
    REQUIRE(w.publish());
    CHECK(list(dir.root()).packs == 1);
    CHECK(list(dir.root()).temporaries == 0);

    const io::pack_reader r(dir.root());
    for (int i = 0; i < 50; ++i) {
        const auto name = "entry " + std::to_string(i);
        const auto bytes = r.find(entry_kind::object, ops::blob_id_of(name));
        REQUIRE(bytes);
        CHECK(text_of(*bytes) == body_of(name));
        CHECK(reinterpret_cast<std::uintptr_t>(bytes->data()) % 8 == 0);
    }
    CHECK(r.read_action(action_kind::result, ops::blob_id_of("r")) ==
        "result body");
    CHECK_FALSE(r.read_action(action_kind::manifest, ops::blob_id_of("r")));
    CHECK_FALSE(r.read_action(action_kind::result, ops::blob_id_of("x")));
}

TEST_CASE("nothing is seen of a pack until it is published, and an "
    "unpublished one leaves nothing", "[io][pack_store]") {
    tests::scoped_directory dir;
    {
        io::pack_writer w(dir.root());
        w.add_action(action_kind::result, ops::blob_id_of("k"), "v");
        CHECK(list(dir.root()).temporaries == 1);
        CHECK_FALSE(io::pack_reader(dir.root()).read_action(
            action_kind::result, ops::blob_id_of("k")));
    }
    CHECK(list(dir.root()).temporaries == 0);
    CHECK(list(dir.root()).packs == 0);
    io::pack_writer empty(dir.root());
    CHECK_FALSE(empty.publish());
}

TEST_CASE("a reader skips a pack that does not parse and keeps the "
    "others", "[io][pack_store]") {
    tests::scoped_directory dir;
    io::pack_writer w(dir.root());
    w.add_action(action_kind::result, ops::blob_id_of("k"), "v");
    REQUIRE(w.publish_only());
    dir.write("pack-" + std::string(32, 'f') + ".cpk", "CMPSPAK\n garbage");

    std::vector<std::filesystem::path> rejected;
    const auto packs = io::open_packs(dir.root(), &rejected);
    CHECK(packs.size() == 1);
    CHECK(rejected.size() == 1);
    CHECK(io::pack_reader(dir.root()).read_action(action_kind::result,
        ops::blob_id_of("k")) == "v");
}

TEST_CASE("the manifest written last is read, through any merge",
    "[io][pack_store]") {
    tests::scoped_directory dir;
    const auto key = ops::blob_id_of("manifest");
    // Three publishes, each a pack: the second holds the newest manifest
    // and is the largest, so a geometric repack merges the first and the
    // third, whose name is newer than the second's.
    {
        io::pack_writer w(dir.root());
        w.add_action(action_kind::manifest, key, "old");
        REQUIRE(w.publish_only());
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    {
        io::pack_writer w(dir.root());
        w.add_action(action_kind::manifest, key, "new");
        w.add(entry_kind::object, ops::blob_id_of("big"),
            span_of(std::string(100'000, 'b')));
        REQUIRE(w.publish_only());
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    {
        io::pack_writer w(dir.root());
        w.add(entry_kind::object, ops::blob_id_of("small"),
            span_of(std::string(10, 's')));
        REQUIRE(w.publish_only());
    }
    CHECK(io::pack_reader(dir.root()).read_action(action_kind::manifest,
        key) == "new");
    CHECK(io::repack(dir.root()) == 2);
    CHECK(list(dir.root()).packs == 2);
    CHECK(io::pack_reader(dir.root()).read_action(action_kind::manifest,
        key) == "new");
}

TEST_CASE("a damaged copy of an action object does not hide a sound one",
    "[io][pack_store]") {
    tests::scoped_directory dir;
    const auto key = ops::blob_id_of("result");
    {
        io::pack_writer w(dir.root());
        w.add_action(action_kind::result, key, "sound");
        REQUIRE(w.publish_only());
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    {
        io::pack_writer w(dir.root());
        w.add(entry_kind::result, key, span_of(std::string("damaged")));
        REQUIRE(w.publish_only());
    }
    CHECK(io::pack_reader(dir.root()).read_action(action_kind::result, key) ==
        "sound");
}

TEST_CASE("publishing and repacking keeps the pack count logarithmic",
    "[io][pack_store]") {
    tests::scoped_directory dir;
    std::vector<types::blob_id> keys;
    for (int i = 0; i < 300; ++i) {
        io::pack_writer w(dir.root());
        keys.push_back(ops::blob_id_of("key " + std::to_string(i)));
        w.add_action(action_kind::result, keys.back(),
            std::string(100 + i % 7, 'x'));
        REQUIRE(w.publish());
        CHECK(list(dir.root()).packs <= 10);
    }
    CHECK(list(dir.root()).temporaries == 0);
    const io::pack_reader r(dir.root());
    for (const auto& k : keys)
        CHECK(r.read_action(action_kind::result, k));
}

TEST_CASE("a held lock makes a repack give up and a collection wait",
    "[io][pack_store]") {
    tests::scoped_directory dir;
    for (int i = 0; i < 2; ++i) {
        io::pack_writer w(dir.root());
        w.add_action(action_kind::result, ops::blob_id_of(std::to_string(i)),
            "same size");
        REQUIRE(w.publish_only());
    }
    bool held = false;
    std::thread holder([&] {
        const io::pack_lock lock(dir.root(), true);
        held = lock.held();
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    CHECK(io::repack(dir.root()) == 0);
    CHECK(list(dir.root()).packs == 2);
    const io::pack_lock waited(dir.root(), true);
    CHECK(waited.held());
    holder.join();
    CHECK(held);
}
