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
#include "compass.store/io/collection.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.store/io/location.hpp"
#include "compass.store/io/pack_store.hpp"
#include "compass.store/ops/action_object.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "compass.store/ops/worktree_index.hpp"
#include "git_repository.hpp"

namespace {

using namespace compass::store;
using namespace std::chrono_literals;

constexpr std::size_t object_size = 100;

std::int64_t ago(std::chrono::seconds age) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        (std::chrono::system_clock::now() - age).time_since_epoch()).count();
}

std::size_t files_in(const std::filesystem::path& dir) {
    std::size_t n = 0;
    std::error_code ignored;
    for (const auto& e : std::filesystem::recursive_directory_iterator(dir,
             ignored))
        n += e.is_regular_file() ? 1 : 0;
    return n;
}

/**
 * @brief A repository whose main worktree tracks a.txt holding "A", with
 * one commit, and its store.
 */
class store_fixture final {
public:
    store_fixture() {
        repo_.write("a.txt", "A");
        repo_.git("add a.txt");
        repo_.git("commit -q -m base");
    }

    [[nodiscard]] const tests::git_repository& repo() const { return repo_; }

    [[nodiscard]] std::filesystem::path common() const {
        return io::git_common_dir(repo_.root());
    }

    [[nodiscard]] std::filesystem::path packs() const {
        return io::packs_dir(common());
    }

    /**
     * @brief Writes, in a pack of its own, an object of object_size bytes
     * for the blob @p content, written @p age ago, and returns its name.
     */
    std::string put(const std::string& content,
        std::chrono::seconds age = 2h) const {
        const auto id = ops::blob_id_of(content);
        const std::string bytes(object_size, 'x');
        io::pack_writer w(packs());
        w.add(types::entry_kind::object, id, std::as_bytes(std::span(bytes)),
            ago(age));
        (void)w.publish_only();
        return "object/" + ops::to_hex(id);
    }

    [[nodiscard]] bool has(const std::string& name) const {
        const auto slash = name.find('/');
        const auto kind = name.substr(0, slash);
        const io::pack_reader store(packs());
        for (const auto k : {types::entry_kind::object,
                 types::entry_kind::manifest, types::entry_kind::result}) {
            const auto bytes = store.find(k, id_of(name.substr(slash + 1)));
            if (bytes && name.starts_with(kind_name(k)))
                return true;
        }
        return false;
    }

    [[nodiscard]] types::collection_result collect(std::uint64_t cap = 0,
        std::chrono::seconds grace = 1h, bool dry_run = false) const {
        return io::collect(repo_.root(), {cap, grace, dry_run},
            [this](const std::filesystem::path& dir) {
                return dir.filename() == "linked" ? linked_keys
                                                  : std::vector<types::blob_id>{};
            });
    }

    /**
     * @brief Writes, in a pack of its own, an action object of kind
     * @p kind named by the hash of @p name, holding @p body, written @p age
     * ago, and returns its key.
     */
    types::blob_id put_action(types::action_kind kind,
        const std::string& name, std::chrono::seconds age = 2h,
        const std::string& body = std::string(object_size, 'x')) const {
        const auto key = ops::blob_id_of(name);
        const auto bytes = ops::encode_action(key, body);
        io::pack_writer w(packs());
        w.add(types::entry_kind_of(kind), key,
            std::as_bytes(std::span(bytes)), ago(age));
        (void)w.publish_only();
        return key;
    }

    [[nodiscard]] std::optional<std::string> action(types::action_kind kind,
        const types::blob_id& key) const {
        return io::pack_reader(packs()).read_action(kind, key);
    }

    /**
     * @brief The action keys the record of the linked worktree names.
     */
    std::vector<types::blob_id> linked_keys;

    /**
     * @brief Adds a linked worktree at the commit, whose index names b.txt
     * holding "B" and a.txt holding "A".
     */
    std::filesystem::path add_linked_worktree() const {
        const auto linked = repo_.root().parent_path() / "linked";
        repo_.git("worktree add -q '" + linked.generic_string() + "'");
        std::ofstream(linked / "b.txt", std::ios::binary) << "B";
        repo_.git("-C '" + linked.generic_string() + "' add b.txt");
        return linked;
    }

private:
    static std::string kind_name(types::entry_kind k) {
        return k == types::entry_kind::object ? "object/"
            : k == types::entry_kind::manifest ? "manifest/" : "result/";
    }

    static types::blob_id id_of(const std::string& hex) {
        types::blob_id id;
        for (std::size_t i = 0; i < id.bytes.size(); ++i)
            id.bytes[i] = static_cast<std::uint8_t>(
                std::stoi(hex.substr(2 * i, 2), nullptr, 16));
        return id;
    }

    tests::git_repository repo_;
};

}

TEST_CASE("an object only a linked worktree's index names is kept",
    "[io][collection]") {
    store_fixture f;
    (void)f.add_linked_worktree();
    const auto a = f.put("A");
    const auto b = f.put("B");
    const auto c = f.put("C");

    const auto r = f.collect();

    CHECK(r.worktrees.size() == 2);
    CHECK(r.live_ids == 2);
    CHECK(f.has(a));
    CHECK(f.has(b));
    CHECK_FALSE(f.has(c));
    CHECK(r.plan.live.objects == 2);
    CHECK(r.removed.objects == 1);
    CHECK(r.removed.bytes == object_size);
}

TEST_CASE("a linked worktree whose checkout is gone still keeps its objects",
    "[io][collection]") {
    store_fixture f;
    std::filesystem::remove_all(f.add_linked_worktree());
    const auto b = f.put("B");

    const auto r = f.collect();

    CHECK(r.worktrees.size() == 2);
    CHECK(f.has(b));
    CHECK(r.removed.objects == 0);
}

TEST_CASE("an object only a worktree index names is kept",
    "[io][collection]") {
    store_fixture f;
    types::worktree_index index;
    index.paths.push_back("converted.org");
    index.stats.emplace_back();
    index.ids.push_back(ops::blob_id_of("W"));
    const auto file = io::worktree_index_path(io::git_dir(f.repo().root()));
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << ops::encode_worktree_index(index);
    const auto w = f.put("W");
    const auto c = f.put("C");

    const auto r = f.collect();

    CHECK(f.has(w));
    CHECK_FALSE(f.has(c));
    CHECK(r.live_ids == 2);
}

TEST_CASE("an old unreferenced object goes only when the store is over its "
    "cap", "[io][collection]") {
    store_fixture f;
    const auto a = f.put("A");
    const auto c = f.put("C");

    auto r = f.collect(2 * object_size);
    CHECK(f.has(c));
    CHECK(r.victims.empty());
    CHECK(r.plan.old_unreferenced.objects == 1);

    r = f.collect(2 * object_size - 1);
    CHECK_FALSE(f.has(c));
    CHECK(f.has(a));
    CHECK(r.victims == std::vector<std::string>{c});
}

TEST_CASE("an unreferenced object inside the grace period is kept",
    "[io][collection]") {
    store_fixture f;
    const auto fresh = f.put("fresh", 0s);
    const auto recent = f.put("recent", 59min);
    const auto old = f.put("old", 61min);

    const auto r = f.collect();

    CHECK(f.has(fresh));
    CHECK(f.has(recent));
    CHECK_FALSE(f.has(old));
    CHECK(r.plan.within_grace.objects == 2);
    CHECK(r.plan.old_unreferenced.objects == 1);
}

TEST_CASE("the age of an entry is when it was written, not when its pack "
    "was", "[io][collection]") {
    store_fixture f;
    const auto old = f.put("old", 3h);
    const auto other = f.put("other", 3h);
    // The first collection merges both packs into a new one; the entries
    // keep their age through it.
    (void)f.collect(1'000'000);
    REQUIRE(f.has(old));

    const auto r = f.collect();

    CHECK_FALSE(f.has(old));
    CHECK_FALSE(f.has(other));
    CHECK(r.plan.old_unreferenced.objects == 2);
}

TEST_CASE("victims are taken oldest first until the store fits",
    "[io][collection]") {
    store_fixture f;
    const auto three = f.put("three hours", 3h);
    const auto five = f.put("five hours", 5h);
    const auto four = f.put("four hours", 4h);

    const auto r = f.collect(object_size + object_size / 2);

    CHECK(r.victims == std::vector<std::string>{five, four});
    CHECK(f.has(three));
    CHECK_FALSE(f.has(four));
    CHECK_FALSE(f.has(five));
    CHECK(r.plan.victims.bytes == 2 * object_size);
}

TEST_CASE("a dry run chooses the same victims and removes nothing",
    "[io][collection]") {
    store_fixture f;
    const auto three = f.put("three hours", 3h);
    const auto five = f.put("five hours", 5h);
    const auto files = files_in(f.packs());

    const auto r = f.collect(0, 1h, true);

    CHECK(r.victims == std::vector<std::string>{five, three});
    CHECK(r.removed.objects == 0);
    CHECK(f.has(three));
    CHECK(f.has(five));
    CHECK(files_in(f.packs()) == files);
}

TEST_CASE("a collection leaves one pack of what it keeps",
    "[io][collection]") {
    store_fixture f;
    const auto a = f.put("A");
    const auto recent = f.put("recent", 1min);
    (void)f.put("C");
    REQUIRE(files_in(f.packs()) == 3);

    const auto r = f.collect();

    CHECK(r.packs == 3);
    CHECK(r.packs_after == 1);
    CHECK(f.has(a));
    CHECK(f.has(recent));
    std::size_t packs = 0;
    for (const auto& e : std::filesystem::directory_iterator(f.packs()))
        packs += io::is_pack_name(e.path().filename().string()) ? 1 : 0;
    CHECK(packs == 1);

    const auto again = f.collect();
    CHECK(again.removed.objects == 0);
    CHECK(again.packs_after == 1);
}

TEST_CASE("an old temporary file or a pack that does not parse goes "
    "whatever the cap, a recent one stays", "[io][collection]") {
    store_fixture f;
    (void)f.put("A");
    const auto dir = f.packs();
    const auto bad = "pack-" + std::string(32, '0') + ".cpk";
    std::ofstream(dir / "tmp_old") << "x";
    std::ofstream(dir / "tmp_new") << "x";
    std::ofstream(dir / bad) << "not a pack";
    std::ofstream(dir / "not_a_pack") << "x";
    for (const auto* name : {"tmp_old", "not_a_pack"})
        std::filesystem::last_write_time(dir / name,
            std::filesystem::file_time_type::clock::now() - 2h);
    std::filesystem::last_write_time(dir / bad,
        std::filesystem::file_time_type::clock::now() - 2h);

    const auto r = f.collect(1'000'000);

    CHECK_FALSE(std::filesystem::exists(dir / "tmp_old"));
    CHECK_FALSE(std::filesystem::exists(dir / bad));
    CHECK(std::filesystem::exists(dir / "tmp_new"));
    CHECK(std::filesystem::exists(dir / "not_a_pack"));
    CHECK(r.plan.temporaries.objects == 3);
    CHECK(r.removed.objects == 2);
}

TEST_CASE("an index that cannot be read stops the collection",
    "[io][collection]") {
    store_fixture f;
    const auto file = io::worktree_index_path(io::git_dir(f.repo().root()));
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << "damaged";
    const auto c = f.put("C");

    CHECK_THROWS_AS(f.collect(), std::runtime_error);
    CHECK(f.has(c));
}

TEST_CASE("a store that does not exist has nothing to collect",
    "[io][collection]") {
    store_fixture f;

    const auto r = f.collect();

    CHECK(r.plan.stored.objects == 0);
    CHECK(r.live_ids == 1);
    CHECK(r.packs_after == 0);
}

TEST_CASE("an action object a linked worktree's record names is kept and an "
    "old one nobody names goes", "[io][collection]") {
    store_fixture f;
    (void)f.add_linked_worktree();
    const auto named = f.put_action(types::action_kind::manifest, "named");
    const auto result = f.put_action(types::action_kind::result, "result");
    const auto old = f.put_action(types::action_kind::result, "old");
    const auto recent = f.put_action(types::action_kind::manifest, "recent",
        1min);
    f.linked_keys = {named, result};

    const auto r = f.collect();

    CHECK(f.action(types::action_kind::manifest, named));
    CHECK(f.action(types::action_kind::result, result));
    CHECK_FALSE(f.action(types::action_kind::result, old));
    CHECK(f.action(types::action_kind::manifest, recent));
    CHECK(r.live_keys == 2);
    CHECK(r.live_actions.objects == 2);
    CHECK(r.removed.objects == 1);
    REQUIRE(r.victims.size() == 1);
    CHECK(r.victims[0].starts_with("result/"));
}

TEST_CASE("a manifest a later one of the same key hides is dropped, "
    "however recent", "[io][collection]") {
    store_fixture f;
    (void)f.add_linked_worktree();
    const auto key = f.put_action(types::action_kind::manifest, "m", 2min,
        "first");
    (void)f.put_action(types::action_kind::manifest, "m", 1min, "second");
    f.linked_keys = {key};
    REQUIRE(f.action(types::action_kind::manifest, key) == "second");

    const auto r = f.collect();

    CHECK(r.superseded.objects == 1);
    CHECK(r.removed.objects == 1);
    CHECK(f.action(types::action_kind::manifest, key) == "second");
    CHECK(r.packs_after == 1);
}

TEST_CASE("a collection removes the first layout of the store whole; a dry "
    "run counts it", "[io][collection]") {
    store_fixture f;
    (void)f.put("A");
    const auto legacy = io::legacy_store_dirs(f.common());
    for (const auto& d : legacy) {
        std::filesystem::create_directories(d / "ab");
        std::ofstream(d / "ab" / std::string(38, 'c')) << "old";
    }

    const auto dry = f.collect(0, 1h, true);
    CHECK(dry.legacy.objects == 3);
    CHECK(dry.legacy.bytes == 9);
    for (const auto& d : legacy)
        CHECK(std::filesystem::exists(d));

    (void)f.collect();
    for (const auto& d : legacy)
        CHECK_FALSE(std::filesystem::exists(d));
    CHECK(f.has("object/" + ops::to_hex(ops::blob_id_of("A"))));
}
