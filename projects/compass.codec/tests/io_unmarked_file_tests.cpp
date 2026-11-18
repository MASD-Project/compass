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
#include "compass.codec/io/unmarked_file.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.platform/process/environment.hpp"
#include "compass.store/ops/blob_hash.hpp"

namespace {

using namespace compass::codec;
using compass::store::types::blob_id;

class scoped_directory final {
public:
    scoped_directory() {
        static int counter = 0;
        root_ = std::filesystem::temp_directory_path() /
            ("compass.codec.unmarked." +
                std::to_string(
                    compass::platform::process::current_process_id()) +
                "." + std::to_string(counter++));
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

const std::vector<blob_id> ids{
    compass::store::ops::blob_id_of("b"),
    compass::store::ops::blob_id_of("a"),
    compass::store::ops::blob_id_of("c")};

std::vector<blob_id> sorted(std::vector<blob_id> v) {
    std::ranges::sort(v);
    return v;
}

}

TEST_CASE("the unmarked record reads back the IDs written, sorted",
    "[io_unmarked_file]") {
    scoped_directory dir;
    const auto path = io::unmarked_file_path(dir.root());
    CHECK(io::read_unmarked_file(path).empty());
    CHECK(io::write_unmarked_file(path, ids));
    CHECK(io::read_unmarked_file(path) == sorted(ids));
    CHECK_FALSE(io::write_unmarked_file(path, sorted(ids)));
}

TEST_CASE("a damaged unmarked record holds no ID", "[io_unmarked_file]") {
    scoped_directory dir;
    const auto path = io::unmarked_file_path(dir.root());
    REQUIRE(io::write_unmarked_file(path, ids));
    const auto good = read(path);

    SECTION("a flipped byte") {
        auto bad = good;
        bad[bad.size() / 2] ^= 1;
        write(path, bad);
    }
    SECTION("a truncated file") {
        write(path, good.substr(0, good.size() - 7));
    }
    CHECK(io::read_unmarked_file(path).empty());
}
