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
#include "compass.platform/filesystem/mapped_file.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.platform/process/environment.hpp"

namespace {

using compass::platform::filesystem::mapped_file;

class scoped_temp_file final {
public:
    explicit scoped_temp_file(std::span<const std::byte> content) {
        static int counter = 0;
        path_ = std::filesystem::temp_directory_path() /
            ("compass.platform.tests." +
                std::to_string(
                    compass::platform::process::current_process_id()) +
                "." + std::to_string(counter++));
        std::ofstream os(path_, std::ios::binary);
        os.write(reinterpret_cast<const char*>(content.data()),
            static_cast<std::streamsize>(content.size()));
    }
    ~scoped_temp_file() { std::filesystem::remove(path_); }
    scoped_temp_file(const scoped_temp_file&) = delete;
    scoped_temp_file& operator=(const scoped_temp_file&) = delete;

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

std::vector<std::byte> patterned_bytes(std::size_t n, unsigned seed) {
    std::vector<std::byte> r(n);
    for (std::size_t i = 0; i < n; ++i)
        r[i] = static_cast<std::byte>((i * 31 + seed) & 0xFF);
    return r;
}

std::span<const std::byte> as_bytes(std::string_view s) {
    return std::as_bytes(std::span(s.data(), s.size()));
}

}

TEST_CASE("mapped_file exposes exact bytes across several pages",
    "[filesystem][mapped_file]") {
    const auto expected = patterned_bytes(3 * 4096 + 17, 7);
    const scoped_temp_file f(expected);

    const mapped_file m(f.path());

    CHECK(m.size() == expected.size());
    CHECK_FALSE(m.empty());
    CHECK(std::ranges::equal(m.bytes(), expected));
}

TEST_CASE("mapped_file text keeps embedded NUL and high bytes",
    "[filesystem][mapped_file]") {
    using namespace std::string_view_literals;
    const auto expected = "a\0b\xff\n"sv;
    const scoped_temp_file f(as_bytes(expected));

    const mapped_file m(f.path());

    CHECK(m.size() == 5);
    CHECK(m.text() == expected);
}

TEST_CASE("mapped_file maps an empty file to an empty view",
    "[filesystem][mapped_file]") {
    const scoped_temp_file f({});

    const mapped_file m(f.path());

    CHECK(m.empty());
    CHECK(m.size() == 0);
    CHECK(m.bytes().data() == nullptr);
    CHECK(m.text().empty());
}

TEST_CASE("mapped_file throws ENOENT naming a missing path",
    "[filesystem][mapped_file]") {
    const auto missing = std::filesystem::temp_directory_path() /
        "compass.platform.tests.does_not_exist";

    try {
        mapped_file m(missing);
        FAIL("expected std::system_error");
    } catch (const std::system_error& e) {
        CHECK(e.code() == std::errc::no_such_file_or_directory);
        CHECK_THAT(e.what(), Catch::Matchers::ContainsSubstring(missing.string()));
    }
}

TEST_CASE("mapped_file refuses a directory",
    "[filesystem][mapped_file]") {
    const auto dir = std::filesystem::temp_directory_path();

    try {
        mapped_file m(dir);
        FAIL("expected std::system_error");
    } catch (const std::system_error& e) {
        CHECK(e.code() == std::errc::invalid_argument);
        CHECK_THAT(e.what(), Catch::Matchers::ContainsSubstring(dir.string()));
    }
}

TEST_CASE("mapped_file move construction empties the source",
    "[filesystem][mapped_file]") {
    const auto expected = patterned_bytes(100, 3);
    const scoped_temp_file f(expected);
    mapped_file source(f.path());

    const mapped_file target(std::move(source));

    CHECK(source.empty());
    CHECK(source.bytes().data() == nullptr);
    CHECK(std::ranges::equal(target.bytes(), expected));
}

TEST_CASE("mapped_file move assignment replaces the target mapping",
    "[filesystem][mapped_file]") {
    const auto first = patterned_bytes(64, 1);
    const auto second = patterned_bytes(200, 2);
    const scoped_temp_file f1(first);
    const scoped_temp_file f2(second);
    mapped_file target(f1.path());
    mapped_file source(f2.path());

    target = std::move(source);

    CHECK(source.empty());
    CHECK(source.bytes().data() == nullptr);
    CHECK(target.size() == second.size());
    CHECK(std::ranges::equal(target.bytes(), second));
}
