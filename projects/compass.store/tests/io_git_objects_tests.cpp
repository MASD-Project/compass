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
#include "compass.store/io/git_objects.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.platform/filesystem/mapped_file.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "compass.store/ops/git_object.hpp"
#include "git_repository.hpp"

namespace {

using namespace compass::store;
using compass::store::tests::git_repository;

types::object_id id_of(const std::string& hex) {
    const auto id = ops::from_hex(hex);
    REQUIRE(id);
    return *id;
}

/**
 * @brief The bytes `git cat-file <type> <hex>` prints, unaltered.
 */
std::string cat_file(const git_repository& repo, const std::string& type,
    const std::string& hex) {
    const auto out = repo.root().parent_path() / "cat-file.out";
    repo.git("cat-file " + type + " " + hex + " > '" + out.string() + "'");
    REQUIRE(repo.last_status() == 0);
    std::ifstream in(out, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

io::git_objects open(const git_repository& repo) {
    return io::git_objects(io::git_objects_dir(repo.root() / ".git"));
}

/**
 * @brief Every blob `git ls-tree -r` lists for @p rev, as (path, hex)
 * rows in byte order of the paths.
 */
std::vector<std::pair<std::string, std::string>> ls_tree(
    const git_repository& repo, const std::string& rev) {
    std::istringstream in(repo.git("ls-tree -r -z --full-tree " + rev +
        " | tr '\\0' '\\n'"));
    std::vector<std::pair<std::string, std::string>> out;
    for (std::string line; std::getline(in, line);) {
        const auto tab = line.find('\t');
        out.emplace_back(line.substr(tab + 1), line.substr(12, 40));
    }
    std::ranges::sort(out);
    return out;
}

/**
 * @brief Checks that every blob of @p rev reads as git cat-file prints it,
 * and that list_commit lists the paths and IDs ls-tree does.
 */
void check_commit(const git_repository& repo, const io::git_objects& db,
    const std::string& rev) {
    const auto expected = ls_tree(repo, rev);
    REQUIRE_FALSE(expected.empty());
    const auto files = io::list_commit(db, id_of(repo.git("rev-parse " + rev)));
    REQUIRE(files.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const auto& [path, hex] = expected[i];
        INFO(path);
        CHECK(files.paths[i] == path);
        CHECK(ops::to_hex(files.ids[i]) == hex);
        const auto object = db.read(files.ids[i]);
        REQUIRE(object);
        CHECK(object->type == types::object_type::blob);
        CHECK(object->bytes == cat_file(repo, "blob", hex));
    }
}

/**
 * @brief Bytes that change a little more with each version, so each
 * version is closest to the one before and git chains their deltas.
 */
std::string version(int v, std::size_t lines) {
    std::string s;
    for (std::size_t i = 0; i < lines; ++i) {
        s += "line " + std::to_string(i);
        if (i % 61 < static_cast<std::size_t>(v))
            s += " edited";
        s += "\n";
    }
    return s;
}

/**
 * @brief Commits @p versions versions of two files, one of them larger
 * than the 64 KiB a single delta copy can name.
 */
void commit_versions(const git_repository& repo, int versions) {
    for (int v = 0; v < versions; ++v) {
        repo.write("small.txt", version(v, 200));
        repo.write("dir/large.txt", version(v, 20000));
        repo.git("add -A");
        repo.git("commit -q -m v" + std::to_string(v));
    }
}

/**
 * @brief How many entries of each kind the packs of @p repo hold, read
 * with the pack parser: [whole, offset delta, reference delta].
 */
std::array<std::size_t, 3> entry_kinds(const git_repository& repo) {
    std::array<std::size_t, 3> out{};
    const auto dir = repo.root() / ".git" / "objects" / "pack";
    for (const auto& e : std::filesystem::directory_iterator(dir)) {
        if (e.path().extension() != ".idx")
            continue;
        auto pack_path = e.path();
        pack_path.replace_extension(".pack");
        const compass::platform::filesystem::mapped_file idx(e.path());
        const compass::platform::filesystem::mapped_file pack(pack_path);
        const auto index = ops::parse_pack_index(idx.bytes());
        REQUIRE(index);
        for (std::uint32_t i = 0; i < index->count; ++i) {
            types::object_id id;
            std::memcpy(id.bytes.data(), index->ids.data() + i * 20, 20);
            const auto offset = ops::find_offset(*index, id);
            REQUIRE(offset);
            const auto entry = ops::parse_pack_entry(pack.bytes(), *offset);
            REQUIRE(entry);
            if (entry->kind == ops::pack_entry_kind::offset_delta)
                ++out[1];
            else if (entry->kind == ops::pack_entry_kind::ref_delta)
                ++out[2];
            else
                ++out[0];
        }
    }
    return out;
}

/**
 * @brief The longest delta chain `git verify-pack -v` reports.
 */
std::size_t deepest_chain(const git_repository& repo) {
    const auto out = repo.git("verify-pack -v '" + repo.root().string() +
        "'/.git/objects/pack/*.idx");
    std::size_t deepest = 0;
    std::istringstream in(out);
    for (std::string line; std::getline(in, line);) {
        constexpr std::string_view prefix = "chain length = ";
        if (!line.starts_with(prefix))
            continue;
        deepest = std::max<std::size_t>(deepest,
            std::stoul(line.substr(prefix.size())));
    }
    return deepest;
}

bool has_loose_objects(const git_repository& repo) {
    for (const auto& e : std::filesystem::directory_iterator(
             repo.root() / ".git" / "objects")) {
        const auto name = e.path().filename().string();
        if (name.size() == 2 && !std::filesystem::is_empty(e.path()))
            return true;
    }
    return false;
}

}

TEST_CASE("loose objects read as git cat-file prints them",
    "[io][git_objects]") {
    git_repository repo;
    repo.write("a.org", "#+title: A\n");
    repo.write("dir/sub/b.txt", std::string(1000, 'b'));
    repo.write("dir/empty", "");
    repo.write("dir.x", "sorts before dir/ in git's tree order\n");
    repo.git("add -A");
    repo.git("commit -q -m one");
    REQUIRE(has_loose_objects(repo));

    const auto db = open(repo);
    CHECK(db.packs() == 0);
    check_commit(repo, db, "HEAD");

    const auto head = repo.git("rev-parse HEAD");
    const auto commit = db.read(id_of(head));
    REQUIRE(commit);
    CHECK(commit->type == types::object_type::commit);
    CHECK(commit->bytes == cat_file(repo, "commit", head));
}

TEST_CASE("packed objects read as git cat-file prints them after git gc",
    "[io][git_objects]") {
    git_repository repo;
    commit_versions(repo, 5);
    repo.git("gc -q");
    REQUIRE_FALSE(has_loose_objects(repo));

    const auto db = open(repo);
    CHECK(db.packs() == 1);
    for (int v = 0; v < 5; ++v)
        check_commit(repo, db, "HEAD~" + std::to_string(v));
}

TEST_CASE("objects at the end of deep offset delta chains read as git "
          "prints them", "[io][git_objects]") {
    git_repository repo;
    commit_versions(repo, 60);
    repo.git("repack -adfq --depth=50 --window=250");
    const auto kinds = entry_kinds(repo);
    REQUIRE(kinds[1] > 0);
    REQUIRE(kinds[2] == 0);
    REQUIRE(deepest_chain(repo) >= 20);

    const auto db = open(repo);
    for (int v = 0; v < 60; v += 7)
        check_commit(repo, db, "HEAD~" + std::to_string(v));
}

TEST_CASE("objects stored as reference deltas read as git prints them",
    "[io][git_objects]") {
    git_repository repo;
    commit_versions(repo, 30);
    repo.git("-c repack.useDeltaBaseOffset=false repack -adfq --depth=50 "
             "--window=250");
    const auto kinds = entry_kinds(repo);
    REQUIRE(kinds[1] == 0);
    REQUIRE(kinds[2] > 0);
    REQUIRE(deepest_chain(repo) >= 10);

    const auto db = open(repo);
    for (int v = 0; v < 30; v += 3)
        check_commit(repo, db, "HEAD~" + std::to_string(v));
}

TEST_CASE("a delta cached by one read is still right for the next",
    "[io][git_objects]") {
    git_repository repo;
    commit_versions(repo, 20);
    repo.git("repack -adfq --depth=50 --window=250");

    const auto db = open(repo);
    for (int pass = 0; pass < 2; ++pass) {
        for (int v = 19; v >= 0; v -= 2)
            check_commit(repo, db, "HEAD~" + std::to_string(v));
    }
}

TEST_CASE("objects of both loose and packed kinds read in one database",
    "[io][git_objects]") {
    git_repository repo;
    commit_versions(repo, 3);
    repo.git("gc -q");
    repo.write("new.txt", "after the pack\n");
    repo.git("add -A");
    repo.git("commit -q -m loose");
    REQUIRE(has_loose_objects(repo));

    const auto db = open(repo);
    check_commit(repo, db, "HEAD");
    check_commit(repo, db, "HEAD~1");
}

TEST_CASE("objects of a repository's alternates are read",
    "[io][git_objects]") {
    git_repository repo;
    commit_versions(repo, 3);
    repo.git("gc -q");
    const auto shared = repo.root().parent_path() / "shared";
    repo.git("clone -q --shared . '" + shared.string() + "'");
    REQUIRE(std::filesystem::exists(
        shared / ".git" / "objects" / "info" / "alternates"));

    const io::git_objects db(io::git_objects_dir(shared / ".git"));
    const auto files = io::list_commit(db, id_of(repo.git("rev-parse HEAD")));
    CHECK(files.size() == 2);
    const auto small = files.find("small.txt");
    REQUIRE(small);
    CHECK(db.read(files.ids[*small])->bytes == version(2, 200));
}

TEST_CASE("a missing object is nothing and a missing commit throws",
    "[io][git_objects]") {
    git_repository repo;
    repo.write("a.txt", "a\n");
    repo.git("add -A");
    repo.git("commit -q -m one");
    const auto missing = id_of("0123456789abcdef0123456789abcdef01234567");

    for (const auto packed : {false, true}) {
        if (packed)
            repo.git("gc -q");
        const auto db = open(repo);
        CHECK_FALSE(db.read(missing));
        CHECK_THROWS(io::list_commit(db, missing));
        CHECK_THROWS(io::read_commit_file(db, missing, "a.txt"));
    }
}

TEST_CASE("a commit's file is read without listing the commit",
    "[io][git_objects]") {
    git_repository repo;
    repo.write(".compass.org", "#+title: C\n");
    repo.write("doc/meta/x.org", "x\n");
    repo.git("add -A");
    repo.git("commit -q -m one");
    const auto db = open(repo);
    const auto head = id_of(repo.git("rev-parse HEAD"));

    CHECK(io::read_commit_file(db, head, ".compass.org") == "#+title: C\n");
    CHECK(io::read_commit_file(db, head, "doc/meta/x.org") == "x\n");
    CHECK_FALSE(io::read_commit_file(db, head, "doc/meta"));
    CHECK_FALSE(io::read_commit_file(db, head, "doc/meta/y.org"));
    CHECK_FALSE(io::read_commit_file(db, head, "x.org/doc"));
}

TEST_CASE("a damaged loose object throws naming it", "[io][git_objects]") {
    git_repository repo;
    repo.write("a.txt", "a\n");
    repo.git("add -A");
    repo.git("commit -q -m one");
    const auto hex = repo.git("rev-parse HEAD:a.txt");
    const auto file = repo.root() / ".git" / "objects" / hex.substr(0, 2) /
        hex.substr(2);
    std::filesystem::permissions(file, std::filesystem::perms::owner_write,
        std::filesystem::perm_options::add);
    std::ofstream(file, std::ios::binary | std::ios::trunc) << "not zlib";

    const auto db = open(repo);
    CHECK_THROWS_WITH(db.read(id_of(hex)),
        Catch::Matchers::ContainsSubstring(hex));
}
