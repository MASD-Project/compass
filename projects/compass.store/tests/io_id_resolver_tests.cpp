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

#include <chrono>
#include <fstream>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include "compass.platform/filesystem/file_status.hpp"
#include "compass.store/io/git_files.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "git_repository.hpp"

namespace {

using namespace compass::store;
using tests::git_repository;

/**
 * @brief Writes each file, ages it and stages it, so that every entry is
 * clean and none is racily clean.
 */
void add_clean(const git_repository& repo,
    std::initializer_list<std::pair<std::string, std::string>> files) {
    for (const auto& [path, content] : files) {
        repo.write(path, content);
        repo.age(path);
    }
    repo.git("add -A");
}

void set_mtime(const std::filesystem::path& file, types::index_time t) {
    compass::platform::filesystem::set_modified_time(file, {t.sec, t.nsec});
}

io::id_source source_of(const io::id_resolver& r, const std::string& path) {
    return r.resolve(path).source;
}

/**
 * @brief Checks that the resolver's ID for @p path is the one git gives
 * the file's bytes, and returns where it came from.
 */
io::id_source checked(const git_repository& repo, const io::id_resolver& r,
    const std::string& path) {
    const auto resolved = r.resolve(path);
    CHECK(ops::to_hex(resolved.id) == repo.hash_object(path));
    return resolved.source;
}

}

TEST_CASE("a clean file takes its ID from a version 2 index",
    "[io][id_resolver]") {
    git_repository repo;
    add_clean(repo, {{"a.txt", "alpha\n"}, {"dir/b.org", "* B\n"}});
    repo.git("update-index --index-version 2");

    const io::id_resolver r(repo.root());
    CHECK(r.index().version == 2);
    CHECK(checked(repo, r, "a.txt") == io::id_source::git_index);
    CHECK(checked(repo, r, "dir/b.org") == io::id_source::git_index);
}

TEST_CASE("a version 4 index decodes prefix-compressed paths and IDs",
    "[io][id_resolver]") {
    git_repository repo;
    // The long name makes the next entry strip more than 127 bytes, two
    // bytes of varint, and stays within Windows' 260-character paths.
    add_clean(repo, {{"src/a/one.cpp", "1\n"}, {"src/a/one.hpp", "2\n"},
        {"src/a/two.cpp", "3\n"}, {"src/b/three.cpp", "4\n"},
        {"src/b/three/four.cpp", "5\n"}, {"top.txt", "6\n"},
        {std::string(130, 'd') + "/deep.txt", "7\n"}, {"z.txt", "8\n"}});
    repo.git("update-index --index-version 4");

    const io::id_resolver r(repo.root());
    REQUIRE(r.index().version == 4);
    std::string listed;
    for (const auto& path : r.index().paths)
        listed += (listed.empty() ? "" : "\n") + path;
    CHECK(listed == repo.git("ls-files"));
    for (const auto& path : r.index().paths)
        CHECK(checked(repo, r, path) == io::id_source::git_index);
}

TEST_CASE("a file rewritten with the same size and mtime is hashed",
    "[io][id_resolver]") {
    git_repository repo;
    add_clean(repo, {{"a.txt", "aaaa\n"}});
    const auto entry = io::read_git_index(repo.index_file()).stats.at(0);
    repo.write("a.txt", "bbbb\n");
    set_mtime(repo.root() / "a.txt", entry.mtime);
    const auto now = io::stat_of(repo.root() / "a.txt");
    REQUIRE(now->size == entry.size);
    REQUIRE(now->mtime == entry.mtime);

    const io::id_resolver r(repo.root());
    CHECK(checked(repo, r, "a.txt") == io::id_source::hashed);
}

TEST_CASE("a racily clean entry is hashed even when its stat data matches",
    "[io][id_resolver]") {
    git_repository repo;
    repo.write("racy.txt", "same\n");
    repo.age("racy.txt", std::chrono::hours(1));
    repo.write("old.txt", "old\n");
    repo.age("old.txt");
    repo.git("add -A");

    const io::id_resolver r(repo.root());
    CHECK(checked(repo, r, "racy.txt") == io::id_source::hashed);
    CHECK(checked(repo, r, "old.txt") == io::id_source::git_index);
}

TEST_CASE("a modified file of the same size whose mtime is in the second the "
    "index was written is hashed", "[io][id_resolver]") {
    git_repository repo;
    repo.write("a.txt", "aaaa\n");
    repo.git("add -A");
    const auto index = io::read_git_index(repo.index_file());
    repo.write("a.txt", "bbbb\n");
    set_mtime(repo.root() / "a.txt", index.stats.at(0).mtime);
    REQUIRE(io::stat_of(repo.root() / "a.txt")->mtime.sec ==
        index.written.sec);

    const io::id_resolver r(repo.root());
    CHECK(checked(repo, r, "a.txt") == io::id_source::hashed);
}

TEST_CASE("a path an attributes rule may convert is hashed",
    "[io][id_resolver]") {
    git_repository repo;
    add_clean(repo, {
        {".gitattributes", "*.png binary\n*.txt eol=lf\n"
                           "populate/** text eol=lf\n"
                           "/rooted.md text\n"},
        {"sub/.gitattributes", "*.c filter=x\n"},
        {"a.txt", "a\n"}, {"deep/b.txt", "b\n"},
        {"image.png", "png"}, {"a.md", "md\n"},
        {"rooted.md", "r\n"}, {"deep/rooted.md", "d\n"},
        {"populate/x/y.sql", "y\n"}, {"other/populate.sql", "z\n"},
        {"sub/x.c", "c\n"}, {"x.c", "c2\n"}});

    const io::id_resolver r(repo.root());
    for (const auto* path : {"a.txt", "deep/b.txt", "rooted.md",
             "populate/x/y.sql", "sub/x.c"})
        CHECK(checked(repo, r, path) == io::id_source::hashed);
    for (const auto* path : {"image.png", "a.md", "deep/rooted.md",
             "other/populate.sql", "x.c", ".gitattributes"})
        CHECK(checked(repo, r, path) == io::id_source::git_index);
}

TEST_CASE("core.autocrlf set to true or input sends every file to hashing",
    "[io][id_resolver]") {
    for (const auto* value : {"true", "input"}) {
        git_repository repo;
        add_clean(repo, {{"a.txt", "a\n"}});
        CHECK(source_of(io::id_resolver(repo.root()), "a.txt") ==
            io::id_source::git_index);

        repo.git(std::string("config core.autocrlf ") + value);
        const io::id_resolver r(repo.root());
        CHECK(r.config().autocrlf);
        CHECK(checked(repo, r, "a.txt") == io::id_source::hashed);
    }
}

TEST_CASE("core.autocrlf set in an included file sends every file to hashing",
    "[io][id_resolver]") {
    git_repository repo;
    add_clean(repo, {{"a.txt", "a\n"}});
    repo.write("extra.config", "[core]\n\tautoCRLF = Input ; why not\n");
    repo.git("config include.path ../extra.config");

    CHECK(source_of(io::id_resolver(repo.root()), "a.txt") ==
        io::id_source::hashed);
}

TEST_CASE("core.autocrlf set to false keeps the index IDs",
    "[io][id_resolver]") {
    git_repository repo;
    add_clean(repo, {{"a.txt", "a\n"}});
    repo.git("config core.autocrlf false");

    CHECK(source_of(io::id_resolver(repo.root()), "a.txt") ==
        io::id_source::git_index);
}

TEST_CASE("a conflicted path is hashed", "[io][id_resolver]") {
    git_repository repo;
    add_clean(repo, {{"c.txt", "base\n"}, {"calm.txt", "calm\n"}});
    repo.git("commit -q -m base");
    repo.git("checkout -q -b other");
    add_clean(repo, {{"c.txt", "other\n"}});
    repo.git("commit -q -am other");
    repo.git("checkout -q -");
    add_clean(repo, {{"c.txt", "mine\n"}});
    repo.git("commit -q -am mine");
    repo.git("merge -q other");
    REQUIRE(repo.last_status() != 0);

    const io::id_resolver r(repo.root());
    CHECK(checked(repo, r, "c.txt") == io::id_source::hashed);
    CHECK(checked(repo, r, "calm.txt") == io::id_source::git_index);
}

TEST_CASE("a path added with intent to add is hashed", "[io][id_resolver]") {
    git_repository repo;
    repo.write("later.txt", "later\n");
    repo.age("later.txt");
    repo.git("add -N later.txt");

    const io::id_resolver r(repo.root());
    CHECK(checked(repo, r, "later.txt") == io::id_source::hashed);
}

TEST_CASE("an untracked file is hashed", "[io][id_resolver]") {
    git_repository repo;
    add_clean(repo, {{"a.txt", "a\n"}});
    repo.write("new.txt", "new\n");

    const io::id_resolver r(repo.root());
    CHECK(checked(repo, r, "new.txt") == io::id_source::hashed);
}

TEST_CASE("a linked worktree reads the index of its own git directory",
    "[io][id_resolver]") {
    git_repository repo;
    add_clean(repo, {{"a.txt", "a\n"}});
    repo.git("commit -q -m base");
    const auto linked = repo.root().parent_path() / "linked";
    repo.git("worktree add -q '" + linked.generic_string() + "'");
    std::filesystem::last_write_time(linked / "a.txt",
        std::filesystem::file_time_type::clock::now() - std::chrono::hours(1));
    repo.git("-C '" + linked.generic_string() + "' update-index --refresh");
    {
        std::ofstream os(linked / "only_here.txt");
        os << "here\n";
    }
    std::filesystem::last_write_time(linked / "only_here.txt",
        std::filesystem::file_time_type::clock::now() - std::chrono::hours(1));
    repo.git("-C '" + linked.generic_string() + "' add only_here.txt");

    const io::id_resolver r(linked);
    CHECK(source_of(r, "only_here.txt") == io::id_source::git_index);
    CHECK(source_of(r, "a.txt") == io::id_source::git_index);
    CHECK_FALSE(io::id_resolver(repo.root()).known_id("only_here.txt"));
}

TEST_CASE("a corrupt index is not trusted and every file is hashed",
    "[io][id_resolver]") {
    git_repository repo;
    add_clean(repo, {{"a.txt", "a\n"}, {"b.txt", "b\n"}});
    auto bytes = tests::read(repo.index_file());

    SECTION("truncated") {
        bytes.resize(bytes.size() - 7);
    }
    SECTION("a flipped byte") {
        bytes[bytes.size() / 2] ^= 0x01;
    }
    std::filesystem::remove(repo.index_file());
    {
        std::ofstream os(repo.index_file(), std::ios::binary);
        os << bytes;
    }
    const io::id_resolver r(repo.root());
    CHECK_FALSE(r.index_problem().empty());
    CHECK(r.index().size() == 0);
    CHECK(checked(repo, r, "a.txt") == io::id_source::hashed);
    CHECK(checked(repo, r, "b.txt") == io::id_source::hashed);
}

TEST_CASE("an index written under index.skipHash has no checksum to verify",
    "[io][id_resolver]") {
    git_repository repo;
    repo.git("config index.skipHash true");
    add_clean(repo, {{"a.txt", "a\n"}});
    REQUIRE(tests::read(repo.index_file()).ends_with(std::string(20, '\0')));

    const io::id_resolver r(repo.root());
    CHECK(checked(repo, r, "a.txt") == io::id_source::git_index);
}

TEST_CASE("a split index falls back to hashing every file",
    "[io][id_resolver]") {
    git_repository repo;
    add_clean(repo, {{"a.txt", "a\n"}});
    repo.git("update-index --split-index");

    const io::id_resolver r(repo.root());
    CHECK_FALSE(r.index_problem().empty());
    CHECK(r.index().size() == 0);
    CHECK(checked(repo, r, "a.txt") == io::id_source::hashed);
}

TEST_CASE("a repository with no index hashes every file",
    "[io][id_resolver]") {
    git_repository repo;
    repo.write("a.txt", "a\n");

    const io::id_resolver r(repo.root());
    CHECK(r.index().size() == 0);
    CHECK(checked(repo, r, "a.txt") == io::id_source::hashed);
}
