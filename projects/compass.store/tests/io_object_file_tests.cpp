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
#include "compass.store/io/object_file.hpp"

#include <atomic>
#include <cstddef>
#include <span>
#include <string>
#include <thread>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "scoped_directory.hpp"

namespace {

using namespace compass::store;
using tests::scoped_directory;

std::span<const std::byte> bytes_of(const std::string& s) {
    return std::as_bytes(std::span(s));
}

std::vector<std::string> names_in(const std::filesystem::path& directory) {
    std::vector<std::string> names;
    for (const auto& entry : std::filesystem::directory_iterator(directory))
        names.push_back(entry.path().filename().string());
    return names;
}

}

TEST_CASE("a written object is read back and nothing else is left",
    "[io][object_file]") {
    scoped_directory d;
    const auto target = d.root() / "objects" / "ab" / "cdef";

    io::write_object(target, bytes_of("content"));

    CHECK(tests::read(target) == "content");
    CHECK(names_in(target.parent_path()) ==
        std::vector<std::string>{"cdef"});
}

TEST_CASE("writing an object again replaces it", "[io][object_file]") {
    scoped_directory d;
    const auto target = d.root() / "ab" / "cdef";
    io::write_object(target, bytes_of("a stale and longer object"));

    io::write_object(target, bytes_of("fresh"));

    CHECK(tests::read(target) == "fresh");
    CHECK(names_in(target.parent_path()).size() == 1);
}

TEST_CASE("a write that cannot land throws and leaves no temporary file",
    "[io][object_file]") {
    scoped_directory d;
    const auto target = d.root() / "ab" / "cdef";
    std::filesystem::create_directories(target / "in the way");

    CHECK_THROWS_AS(io::write_object(target, bytes_of("content")),
        std::filesystem::filesystem_error);
    CHECK(names_in(target.parent_path()) ==
        std::vector<std::string>{"cdef"});
}

TEST_CASE("concurrent writers of one object all succeed and leave it whole",
    "[io][object_file]") {
    constexpr unsigned writers = 8;
    constexpr unsigned rounds = 50;
    scoped_directory d;
    const auto target = d.root() / "ab" / "cdef";
    const std::string content(64 * 1024, 'x');
    io::write_object(target, bytes_of(content));

    std::atomic<unsigned> failures = 0;
    std::atomic<unsigned> torn = 0;
    std::atomic<bool> done = false;
    {
        std::jthread reader([&] {
            while (!done)
                if (tests::read(target) != content)
                    ++torn;
        });
        {
            std::vector<std::jthread> threads;
            for (unsigned i = 0; i < writers; ++i) {
                threads.emplace_back([&] {
                    for (unsigned round = 0; round < rounds; ++round) {
                        try {
                            io::write_object(target, bytes_of(content));
                        } catch (const std::exception&) {
                            ++failures;
                        }
                    }
                });
            }
        }
        done = true;
    }

    CHECK(failures == 0);
    CHECK(torn == 0);
    CHECK(tests::read(target) == content);
    CHECK(names_in(target.parent_path()) ==
        std::vector<std::string>{"cdef"});
}
