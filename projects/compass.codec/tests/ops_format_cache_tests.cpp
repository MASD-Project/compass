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
#include "compass.codec/ops/format_cache.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.store/ops/blob_hash.hpp"

namespace {

using namespace compass::codec;
using compass::store::ops::blob_id_of;

types::formatter clang_format() {
    return {.name = "clang-format", .program = "/usr/bin/clang-format",
        .extensions = {".cpp", ".hpp"},
        .arguments = {"-i", "--style=file:{style}"}, .passes = 2,
        .style = "/checkout/.clang-format"};
}

types::formatter prettier() {
    return {.name = "prettier", .program = "/usr/bin/prettier",
        .extensions = {".ts"}, .arguments = {"--write"}, .passes = 1,
        .style = "/checkout/.prettierrc.yaml"};
}

}

TEST_CASE("formatter_of selects by the file name's last suffix",
    "[format_cache]") {
    const std::array formatters{clang_format(), prettier()};
    CHECK(ops::formatter_of(formatters, "a/b/x.hpp") == 0u);
    CHECK(ops::formatter_of(formatters, "a/b.ts/x.d.ts") == 1u);
    CHECK(ops::formatter_of(formatters, "a/b.cpp/x.sql") == std::nullopt);
    CHECK(ops::formatter_of(formatters, "a/b.cpp/Makefile") == std::nullopt);
    CHECK(ops::formatter_of(formatters, "a/.hpp") == std::nullopt);
}

TEST_CASE("formatter_of lets the first formatter listed win",
    "[format_cache]") {
    auto other = prettier();
    other.extensions = {".hpp"};
    const std::array formatters{clang_format(), other};
    CHECK(ops::formatter_of(formatters, "x.hpp") == 0u);
}

TEST_CASE("formatter_key changes with what the formatted bytes depend on",
    "[format_cache]") {
    const auto style = blob_id_of("BasedOnStyle: LLVM\n");
    const auto f = clang_format();
    const auto key = ops::formatter_key(f, "clang-format version 21", style);

    CHECK(ops::formatter_key(f, "clang-format version 18", style) != key);
    CHECK(ops::formatter_key(f, "clang-format version 21",
        blob_id_of("BasedOnStyle: Google\n")) != key);
    auto two = f;
    two.passes = 1;
    CHECK(ops::formatter_key(two, "clang-format version 21", style) != key);
    auto args = f;
    args.arguments.push_back("--sort-includes");
    CHECK(ops::formatter_key(args, "clang-format version 21", style) != key);
}

TEST_CASE("formatter_key does not depend on where the program and style are",
    "[format_cache]") {
    const auto style = blob_id_of("BasedOnStyle: LLVM\n");
    const auto f = clang_format();
    auto moved = f;
    moved.program = "/opt/llvm/bin/clang-format";
    moved.style = "/other/worktree/.clang-format";
    CHECK(ops::formatter_key(moved, "v", style) ==
        ops::formatter_key(f, "v", style));
}

TEST_CASE("formatted_key separates raw bytes, paths and formatters",
    "[format_cache]") {
    const auto a = blob_id_of("a");
    const auto b = blob_id_of("b");
    const auto key = ops::formatted_key(a, "x/y.hpp", b);
    CHECK(ops::formatted_key(a, "x/y.hpp", a) != key);
    CHECK(ops::formatted_key(b, "x/y.hpp", b) != key);
    CHECK(ops::formatted_key(b, "x/y.hpp", a) != key);
    CHECK(ops::formatted_key(a, "x/z.hpp", b) != key);
}

TEST_CASE("program_key changes when the program file is replaced",
    "[format_cache]") {
    const auto key = ops::program_key("/usr/bin/clang-format", 10, 100);
    CHECK(ops::program_key("/usr/bin/clang-format", 11, 100) != key);
    CHECK(ops::program_key("/usr/bin/clang-format", 10, 101) != key);
    CHECK(ops::program_key("/usr/bin/clang-format-18", 10, 100) != key);
}

TEST_CASE("format_command puts the style path in place of the placeholder",
    "[format_cache]") {
    const std::vector<std::string> files{"a.hpp", "b/c.cpp"};
    CHECK(ops::format_command(clang_format(), files) ==
        std::vector<std::string>{"/usr/bin/clang-format", "-i",
            "--style=file:/checkout/.clang-format", "a.hpp", "b/c.cpp"});
}
