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
#include "compass.store/io/id_resolver.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.platform/filesystem/file_status.hpp"
#include "compass.store/io/git_files.hpp"
#include "compass.store/io/location.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "compass.store/ops/worktree_index.hpp"
#include "git_repository.hpp"

namespace {

using namespace compass::store;
using tests::git_repository;

/**
 * @brief Resolves @p paths with a fresh resolver of @p checkout, writes its
 * worktree index, and returns the resolutions.
 */
std::vector<io::resolved_id> run(const std::filesystem::path& checkout,
    const std::vector<std::string_view>& paths) {
    const io::id_resolver r(checkout);
    std::vector<io::resolved_id> resolved;
    for (const auto path : paths)
        resolved.push_back(r.resolve(path));
    r.write_worktree_index(paths, resolved);
    return resolved;
}

std::filesystem::path index_of(const std::filesystem::path& checkout) {
    return io::worktree_index_path(io::git_dir(checkout));
}

types::worktree_index read_index(const std::filesystem::path& checkout) {
    return ops::decode_worktree_index(tests::read(index_of(checkout)));
}

void overwrite(const std::filesystem::path& file, std::string_view bytes) {
    std::ofstream os(file, std::ios::binary | std::ios::trunc);
    os.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void set_mtime(const std::filesystem::path& file, types::index_time t) {
    compass::platform::filesystem::set_modified_time(file, {t.sec, t.nsec});
}

io::id_source source_of(const std::filesystem::path& checkout,
    std::string_view path) {
    return io::id_resolver(checkout).resolve(path).source;
}

/**
 * @brief Writes an untracked file that git's index cannot name, aged so
 * that it is not racily clean against a run that starts now.
 */
void add_untracked(const git_repository& repo, const std::string& path,
    std::string_view content) {
    repo.write(path, content);
    repo.age(path);
}

}

TEST_CASE("a hashed file is resolved from the worktree index without being "
    "read", "[io][worktree_index]") {
    git_repository repo;
    add_untracked(repo, "a.org", "* A\n");
    const auto first = run(repo.root(), {"a.org"});
    REQUIRE(first.at(0).source == io::id_source::hashed);
    CHECK(ops::to_hex(first.at(0).id) == repo.hash_object("a.org"));

    // The ID in the index is replaced by one no read of the file could
    // give, so getting it back proves the file was not read.
    auto index = read_index(repo.root());
    REQUIRE(index.paths == std::vector<std::string>{"a.org"});
    index.ids.at(0).bytes.fill(0xAB);
    overwrite(index_of(repo.root()), ops::encode_worktree_index(index));

    const io::id_resolver r(repo.root());
    const auto known = r.known_id("a.org");
    REQUIRE(known);
    CHECK(known->source == io::id_source::worktree_index);
    CHECK(known->id == index.ids.at(0));
    CHECK(r.resolve("a.org").id == index.ids.at(0));
}

TEST_CASE("a run that changes no entry leaves the worktree index untouched",
    "[io][worktree_index][posix]") {
    git_repository repo;
    add_untracked(repo, "a.org", "* A\n");
    run(repo.root(), {"a.org"});
    const auto inode = [&] {
        const auto s = compass::platform::filesystem::regular_file_status(
            index_of(repo.root()));
        REQUIRE(s);
        return s->inode;
    };
    const auto inode_before = inode();

    run(repo.root(), {"a.org"});
    CHECK(inode() == inode_before);

    overwrite(repo.root() / "a.org", "* B, changed\n");
    run(repo.root(), {"a.org"});
    CHECK(inode() != inode_before);
}

TEST_CASE("a file modified since it was hashed is hashed again, even with "
    "its size and mtime unchanged", "[io][worktree_index][posix]") {
    git_repository repo;
    add_untracked(repo, "a.org", "aaaa\n");
    run(repo.root(), {"a.org"});
    const auto entry = read_index(repo.root()).stats.at(0);

    overwrite(repo.root() / "a.org", "bbbb\n");
    set_mtime(repo.root() / "a.org", entry.mtime);
    const auto now = io::stat_of(repo.root() / "a.org");
    REQUIRE(now->size == entry.size);
    REQUIRE(now->mtime == entry.mtime);

    const auto second = run(repo.root(), {"a.org"});
    CHECK(second.at(0).source == io::id_source::hashed);
    CHECK(ops::to_hex(second.at(0).id) == repo.hash_object("a.org"));
    CHECK(source_of(repo.root(), "a.org") == io::id_source::worktree_index);
}

TEST_CASE("a racily clean entry is hashed again", "[io][worktree_index]") {
    git_repository repo;
    repo.write("racy.org", "racy\n");
    repo.age("racy.org", std::chrono::hours(1));
    add_untracked(repo, "calm.org", "calm\n");
    run(repo.root(), {"racy.org", "calm.org"});
    REQUIRE(read_index(repo.root()).size() == 2);

    CHECK(source_of(repo.root(), "racy.org") == io::id_source::hashed);
    CHECK(source_of(repo.root(), "calm.org") == io::id_source::worktree_index);
}

TEST_CASE("an empty file is resolved from the worktree index",
    "[io][worktree_index]") {
    git_repository repo;
    add_untracked(repo, "empty.org", "");
    run(repo.root(), {"empty.org"});
    CHECK(source_of(repo.root(), "empty.org") ==
        io::id_source::worktree_index);
}

TEST_CASE("a damaged worktree index is ignored", "[io][worktree_index]") {
    git_repository repo;
    add_untracked(repo, "a.org", "* A\n");
    run(repo.root(), {"a.org"});
    const auto file = index_of(repo.root());
    const auto good = tests::read(file);

    SECTION("a flipped byte") {
        auto bytes = good;
        bytes[30] = static_cast<char>(bytes[30] ^ 0x01);
        overwrite(file, bytes);
    }
    SECTION("cut short") {
        overwrite(file, good.substr(0, good.size() - 7));
    }
    SECTION("another version, with a checksum that matches") {
        auto bytes = good.substr(0, good.size() - 20);
        bytes[8] = static_cast<char>(ops::worktree_index_version + 1);
        const auto digest = ops::sha1_of(bytes);
        bytes.append(reinterpret_cast<const char*>(digest.data()),
            digest.size());
        overwrite(file, bytes);
    }

    const io::id_resolver r(repo.root());
    CHECK_FALSE(r.worktree_index_problem().empty());
    CHECK(r.worktree_index().size() == 0);
    CHECK(r.resolve("a.org").source == io::id_source::hashed);
}

TEST_CASE("the entries of removed and changed files are dropped, others are "
    "kept", "[io][worktree_index]") {
    git_repository repo;
    add_untracked(repo, "kept.org", "kept\n");
    add_untracked(repo, "removed.org", "removed\n");
    add_untracked(repo, "changed.org", "changed\n");
    add_untracked(repo, "resolved.org", "resolved\n");
    run(repo.root(), {"kept.org", "removed.org", "changed.org",
        "resolved.org"});

    std::filesystem::remove(repo.root() / "removed.org");
    overwrite(repo.root() / "changed.org", "changed again\n");
    run(repo.root(), {"resolved.org"});

    CHECK(read_index(repo.root()).paths ==
        std::vector<std::string>{"kept.org", "resolved.org"});
}

TEST_CASE("files whose ID comes from git's index get no entry",
    "[io][worktree_index]") {
    git_repository repo;
    repo.write("tracked.org", "tracked\n");
    repo.age("tracked.org");
    repo.git("add -A");
    add_untracked(repo, "untracked.org", "untracked\n");

    const auto resolved = run(repo.root(), {"tracked.org", "untracked.org"});
    REQUIRE(resolved.at(0).source == io::id_source::git_index);
    CHECK(read_index(repo.root()).paths ==
        std::vector<std::string>{"untracked.org"});
}

TEST_CASE("a linked worktree writes its own worktree index",
    "[io][worktree_index]") {
    git_repository repo;
    repo.write("a.txt", "a\n");
    repo.git("add -A");
    repo.git("commit -q -m base");
    const auto linked = repo.root().parent_path() / "linked";
    repo.git("worktree add -q '" + linked.generic_string() + "'");
    {
        std::ofstream os(linked / "only_here.org");
        os << "here\n";
    }
    std::filesystem::last_write_time(linked / "only_here.org",
        std::filesystem::file_time_type::clock::now() - std::chrono::hours(1));

    run(linked, {"only_here.org"});
    CHECK(index_of(linked) ==
        repo.root() / ".git" / "worktrees" / "linked" / "compass" / "index");
    CHECK(std::filesystem::exists(index_of(linked)));
    CHECK_FALSE(std::filesystem::exists(index_of(repo.root())));
    CHECK(source_of(linked, "only_here.org") ==
        io::id_source::worktree_index);
}
