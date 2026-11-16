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
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.store/io/action_file.hpp"
#include "compass.store/io/location.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "compass.store/ops/worktree_index.hpp"
#include "git_repository.hpp"

namespace {

using namespace compass::store;
using namespace std::chrono_literals;

constexpr std::size_t object_size = 100;

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

    [[nodiscard]] std::filesystem::path objects() const {
        return io::objects_dir(io::git_common_dir(repo_.root()));
    }

    /**
     * @brief Writes an object of object_size bytes for the blob @p content,
     * modified @p age ago, and returns its name under the store.
     */
    std::string put(const std::string& content,
        std::chrono::seconds age = 2h) const {
        const auto file = io::object_path(objects(), ops::blob_id_of(content));
        std::filesystem::create_directories(file.parent_path());
        std::ofstream(file, std::ios::binary) << std::string(object_size, 'x');
        std::filesystem::last_write_time(file,
            std::filesystem::file_time_type::clock::now() - age);
        return file.lexically_relative(objects()).string();
    }

    [[nodiscard]] bool has(const std::string& name) const {
        return std::filesystem::exists(objects() / name);
    }

    [[nodiscard]] types::collection_result collect(std::uint64_t cap = 0,
        std::chrono::seconds grace = 1h, bool dry_run = false) const {
        return io::collect(repo_.root(), objects(), {cap, grace, dry_run},
            [this](const std::filesystem::path& dir) {
                return dir.filename() == "linked" ? linked_keys
                                                  : std::vector<types::blob_id>{};
            });
    }

    /**
     * @brief Writes an action object of kind @p kind named by the hash of
     * @p name, modified @p age ago, and returns its key.
     */
    types::blob_id put_action(types::action_kind kind,
        const std::string& name, std::chrono::seconds age = 2h) const {
        const auto key = ops::blob_id_of(name);
        const auto dir = io::actions_dir(io::git_common_dir(repo_.root()),
            kind);
        io::write_action(dir, key, std::string(object_size, 'x'));
        std::filesystem::last_write_time(io::object_path(dir, key),
            std::filesystem::file_time_type::clock::now() - age);
        return key;
    }

    [[nodiscard]] bool has_action(types::action_kind kind,
        const types::blob_id& key) const {
        return io::read_action(io::actions_dir(
            io::git_common_dir(repo_.root()), kind), key).has_value();
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
        repo_.git("worktree add -q '" + linked.string() + "'");
        std::ofstream(linked / "b.txt", std::ios::binary) << "B";
        repo_.git("-C '" + linked.string() + "' add b.txt");
        return linked;
    }

private:
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

    const auto r = f.collect(0, 1h, true);

    CHECK(r.victims == std::vector<std::string>{five, three});
    CHECK(r.removed.objects == 0);
    CHECK(f.has(three));
    CHECK(f.has(five));
}

TEST_CASE("an old temporary file goes whatever the cap, a recent one stays",
    "[io][collection]") {
    store_fixture f;
    const auto dir = f.objects() / "ab";
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "tmp_old") << "x";
    std::ofstream(dir / "tmp_new") << "x";
    std::ofstream(dir / "not_an_object") << "x";
    std::filesystem::last_write_time(dir / "tmp_old",
        std::filesystem::file_time_type::clock::now() - 2h);
    std::filesystem::last_write_time(dir / "not_an_object",
        std::filesystem::file_time_type::clock::now() - 2h);

    const auto r = f.collect(1'000'000);

    CHECK_FALSE(std::filesystem::exists(dir / "tmp_old"));
    CHECK(std::filesystem::exists(dir / "tmp_new"));
    CHECK(std::filesystem::exists(dir / "not_an_object"));
    CHECK(r.plan.temporaries.objects == 2);
    CHECK(r.removed.objects == 1);
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

    CHECK(f.has_action(types::action_kind::manifest, named));
    CHECK(f.has_action(types::action_kind::result, result));
    CHECK_FALSE(f.has_action(types::action_kind::result, old));
    CHECK(f.has_action(types::action_kind::manifest, recent));
    CHECK(r.live_keys == 2);
    CHECK(r.actions.objects == 4);
    CHECK(r.live_actions.objects == 2);
    CHECK(r.removed.objects == 1);
    REQUIRE(r.victims.size() == 1);
    CHECK(r.victims[0].starts_with("results/"));
}

TEST_CASE("an action object read under another key or damaged reads as "
    "missing", "[io][action_file]") {
    store_fixture f;
    const auto key = f.put_action(types::action_kind::result, "k");
    const auto dir = io::actions_dir(io::git_common_dir(f.repo().root()),
        types::action_kind::result);
    REQUIRE(io::read_action(dir, key) == std::string(object_size, 'x'));
    CHECK_FALSE(io::read_action(dir, ops::blob_id_of("other")));
    const auto copy = io::object_path(dir, ops::blob_id_of("copy"));
    std::filesystem::create_directories(copy.parent_path());
    std::filesystem::copy_file(io::object_path(dir, key), copy);
    CHECK_FALSE(io::read_action(dir, ops::blob_id_of("copy")));

    std::fstream file(io::object_path(dir, key),
        std::ios::binary | std::ios::in | std::ios::out);
    file.seekp(10);
    file.put('y');
    file.close();
    CHECK_FALSE(io::read_action(dir, key));
}
