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
#include "compass.codec/io/dependency_file.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.codec/codecs/catalogue.hpp"
#include "compass.codec/ops/dependencies.hpp"
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "test_corpus.hpp"

namespace {

using namespace compass::codec;
using Catch::Matchers::ContainsSubstring;
using counts = std::vector<std::uint64_t>;

/**
 * @brief A fresh git directory stand-in under the system temporary
 * directory, removed on destruction.
 */
class scoped_directory final {
public:
    scoped_directory() {
        static int counter = 0;
        root_ = std::filesystem::temp_directory_path() /
            ("compass.codec.tests." + std::to_string(::getpid()) + "." +
                std::to_string(counter++));
        std::filesystem::create_directories(root_);
    }
    ~scoped_directory() { std::filesystem::remove_all(root_); }
    scoped_directory(const scoped_directory&) = delete;
    scoped_directory& operator=(const scoped_directory&) = delete;

    [[nodiscard]] const std::filesystem::path& root() const { return root_; }

private:
    std::filesystem::path root_;
};

std::string read(const std::filesystem::path& p) {
    std::ifstream is(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(is), {}};
}

void write(const std::filesystem::path& p, const std::string& bytes) {
    std::ofstream os(p, std::ios::binary | std::ios::trunc);
    os << bytes;
}

ino_t inode_of(const std::filesystem::path& p) {
    struct stat st {};
    REQUIRE(::stat(p.c_str(), &st) == 0);
    return st.st_ino;
}

void fill(tests::test_corpus& t) {
    t.add("story.org", "#+type: story\n* Goal\n* Status\n");
    t.add("task.org", "#+type: task\n* Goal\n");
    t.add("readme.org", "Plain text.\n");
}

}

TEST_CASE("the dependency file lives under compass in the git directory",
    "[io][dependency_file]") {
    CHECK(io::dependency_file_path("/r/.git") == "/r/.git/compass/deps");
}

TEST_CASE("a missing dependency file is an empty record with no problem",
    "[io][dependency_file]") {
    scoped_directory dir;
    const auto loaded = io::read_dependency_file(dir.root() / "deps");
    CHECK(loaded.record.size() == 0);
    CHECK(loaded.problem.empty());
}

TEST_CASE("the dependency file is written when it changes and left alone "
    "when it does not", "[io][dependency_file]") {
    scoped_directory dir;
    tests::test_corpus t;
    fill(t);
    const auto r = ops::make_registry(codecs::catalogue(), t.types(), {});
    const auto path = io::dependency_file_path(dir.root());
    const auto first = ops::run(r, t.corpus(), t.types(), 2);

    CHECK(io::write_dependency_file(path, first.record));
    const auto inode = inode_of(path);
    const auto loaded = io::read_dependency_file(path);
    CHECK(loaded.problem.empty());
    CHECK(loaded.bytes == std::filesystem::file_size(path));
    const auto second = ops::run(r, t.corpus(), t.types(), 2, loaded.record);

    CHECK(second.ran == counts{0, 0, 0, 0});
    CHECK_FALSE(io::write_dependency_file(path, second.record));
    CHECK(inode_of(path) == inode);

    tests::test_corpus edited;
    edited.add("story.org", "#+type: story\n* Goal\n");
    edited.add("task.org", "#+type: task\n* Goal\n");
    edited.add("readme.org", "Plain text.\n");
    const auto third = ops::run(r, edited.corpus(), edited.types(), 2,
        io::read_dependency_file(path).record);
    CHECK(third.ran == counts{1, 1, 0, 0});
    CHECK(io::write_dependency_file(path, third.record));
    CHECK(read(path) == ops::encode_dependency_record(third.record));
}

TEST_CASE("a damaged dependency file, or one of another version, runs "
    "every unit again", "[io][dependency_file]") {
    scoped_directory dir;
    tests::test_corpus t;
    fill(t);
    const auto r = ops::make_registry(codecs::catalogue(), t.types(), {});
    const auto path = io::dependency_file_path(dir.root());
    const auto first = ops::run(r, t.corpus(), t.types(), 2);
    REQUIRE(io::write_dependency_file(path, first.record));
    const auto good = read(path);

    auto damaged = good;
    damaged[damaged.size() / 2] ^= 0x01;
    write(path, damaged);
    const auto loaded = io::read_dependency_file(path);
    CHECK_THAT(loaded.problem, ContainsSubstring("checksum"));
    CHECK(loaded.record.size() == 0);
    CHECK(ops::run(r, t.corpus(), t.types(), 2, loaded.record).ran ==
        counts{1, 2, 0, 0});

    auto other_version = good.substr(0, good.size() - 20);
    other_version[8] = static_cast<char>(ops::dependency_record_version + 1);
    const auto digest = compass::store::ops::sha1_of(other_version);
    other_version.append(reinterpret_cast<const char*>(digest.data()),
        digest.size());
    write(path, other_version);
    const auto stale = io::read_dependency_file(path);
    CHECK_THAT(stale.problem, ContainsSubstring("version"));
    CHECK(stale.record.size() == 0);

    CHECK(io::write_dependency_file(path, first.record));
    CHECK(read(path) == good);
}
