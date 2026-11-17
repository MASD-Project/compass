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
#include "compass.bench/ops/warm_stage.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.bench/io/report.hpp"
#include "compass.ingest/org/parser.hpp"
#include "compass.org/io/dump.hpp"
#include "compass.org/io/object.hpp"
#include "compass.platform/filesystem/mapped_file.hpp"
#include "compass.store/io/collection.hpp"
#include "compass.store/io/location.hpp"
#include "compass.store/io/pack_store.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "scoped_git_repo.hpp"

namespace {

using namespace compass::bench;

using tests::scoped_git_repo;

types::warm_result run(const scoped_git_repo& repo, unsigned repetitions) {
    return ops::run_warm_stage(repo.checkout(), repo.list(), repo.packs(),
        2, repetitions);
}

/**
 * @brief Two org files git's index names, two it cannot because an
 * attributes rule may convert them, and a file that is not org.
 */
void fill(const scoped_git_repo& repo) {
    repo.write(".gitattributes", "converted/*.org text\n");
    repo.write("a.org", "* A\ntext\n");
    repo.write("doc/b.org", "#+title: b\n\n- item\n");
    repo.write("converted/c.org", "* C\n");
    repo.write("converted/d.org", "* D\n** E\n");
    repo.write("e.cpp", "int x;\n");
    repo.git("add -A");
}

const char* const org_paths[] = {
    "a.org", "doc/b.org", "converted/c.org", "converted/d.org"};

/**
 * @brief The dump of the stored object of each org file, decoded against
 * the file's current bytes, by path. A file with no object that decodes
 * dumps as empty.
 */
std::map<std::string, std::string> stored_dumps(const scoped_git_repo& repo) {
    using compass::platform::filesystem::mapped_file;
    std::map<std::string, std::string> dumps;
    for (const auto* path : org_paths) {
        const mapped_file source(repo.checkout() / path);
        const auto id = compass::store::ops::blob_id_of(source.text());
        const compass::store::io::pack_reader store(repo.packs());
        const auto object = store.find(
            compass::store::types::entry_kind::object, id);
        std::ostringstream os;
        if (object) {
            const auto view = compass::org::io::decode(*object,
                {id.bytes, compass::ingest::org::parser_version});
            if (view)
                compass::org::io::dump(os, source.text(), *view);
        }
        dumps[path] = os.str();
    }
    return dumps;
}

}

TEST_CASE("the first run fills the store and the worktree index and the "
    "next ones hash and write nothing", "[ops][warm_stage]") {
    scoped_git_repo repo;
    fill(repo);

    const auto r = run(repo, 3);

    REQUIRE(r.runs.size() == 3);
    CHECK(r.files == 4);
    const auto& cold = r.runs[0];
    CHECK(cold.from_git_index == 2);
    CHECK(cold.hashed == 2);
    CHECK(cold.loaded == 0);
    CHECK(cold.written == 4);
    for (std::size_t i = 1; i < r.runs.size(); ++i) {
        CHECK(r.runs[i].from_git_index == 2);
        CHECK(r.runs[i].from_worktree_index == 2);
        CHECK(r.runs[i].hashed == 0);
        CHECK(r.runs[i].loaded == 4);
        CHECK(r.runs[i].written == 0);
        CHECK(r.runs[i].errors == 0);
    }
    CHECK(r.index_entries == 2);
    CHECK(std::filesystem::exists(compass::store::io::worktree_index_path(
        repo.checkout() / ".git")));
    CHECK_FALSE(io::failed(r));
}

TEST_CASE("a changed file is hashed and its object written on the next run",
    "[ops][warm_stage]") {
    scoped_git_repo repo;
    fill(repo);
    (void)run(repo, 1);

    repo.write("converted/c.org", "* C changed\n");
    const auto r = run(repo, 2);

    CHECK(r.runs[0].hashed == 1);
    CHECK(r.runs[0].from_worktree_index == 1);
    CHECK(r.runs[0].written == 1);
    CHECK(r.runs[0].loaded == 3);
    CHECK(r.runs[1].hashed == 0);
    CHECK(r.runs[1].written == 0);
    CHECK_FALSE(io::failed(r));
}

TEST_CASE("a damaged object is rewritten", "[ops][warm_stage]") {
    scoped_git_repo repo;
    fill(repo);
    (void)run(repo, 1);
    const auto packs = compass::store::io::open_packs(repo.packs());
    REQUIRE(packs.size() == 1);
    const auto first = compass::store::ops::entry_at(packs[0].view, 0);
    {
        std::fstream os(packs[0].path,
            std::ios::binary | std::ios::in | std::ios::out);
        os.seekp(static_cast<std::streamoff>(first.offset));
        os << "not an object";
    }

    const auto r = run(repo, 1);
    CHECK(r.runs[0].loaded == 3);
    CHECK(r.runs[0].written == 1);
}

TEST_CASE("a warm run that hashes or writes fails the stage",
    "[ops][warm_stage]") {
    types::warm_result r;
    r.files = 1;
    r.runs.resize(2);
    r.runs[0].hashed = 1;
    r.runs[0].written = 1;
    CHECK_FALSE(io::failed(r));

    r.runs[1].hashed = 1;
    CHECK(io::failed(r));
    r.runs[1].hashed = 0;
    r.runs[1].written = 1;
    CHECK(io::failed(r));

    std::ostringstream os;
    io::write_warm_report(os, r, {}, "store");
    CHECK_THAT(os.str(), Catch::Matchers::ContainsSubstring(
        "FAILED: warm run 2 hashed 0 file(s) and wrote 1 object(s)"));
}

TEST_CASE("a run after collection parses the removed objects again and they "
    "dump as before", "[ops][warm_stage]") {
    scoped_git_repo repo;
    fill(repo);
    (void)run(repo, 1);
    const auto before = stored_dumps(repo);
    for (const auto* path : org_paths)
        repo.write(path, std::string("* Rewritten ") + path + "\n");
    repo.git("add -A");
    CHECK(run(repo, 1).runs[0].written == 4);
    const auto collected = compass::store::io::collect(repo.checkout(),
        {0, std::chrono::seconds(0), false}, [](const std::filesystem::path&) {
            return std::vector<compass::store::types::blob_id>{};
        });
    CHECK(collected.plan.stored.objects == 8);
    CHECK(collected.removed.objects == 4);

    fill(repo);
    const auto r = run(repo, 2);
    CHECK(r.runs[0].loaded == 0);
    CHECK(r.runs[0].written == 4);
    CHECK(r.runs[1].loaded == 4);
    CHECK(r.runs[1].written == 0);
    CHECK_FALSE(io::failed(r));
    CHECK(stored_dumps(repo) == before);
    CHECK_FALSE(before.at("a.org").empty());
}
