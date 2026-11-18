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
#include "compass.codec/ops/unified_diff.hpp"

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include "compass.platform/process/environment.hpp"
#include "compass.platform/process/run_program.hpp"

namespace {

using compass::codec::ops::unified_diff;

/**
 * @brief Lines numbered from @p first to @p last, one number per line, so
 * git finds no function name to put in a hunk header.
 */
std::string numbers(int first, int last) {
    std::string out;
    for (int i = first; i <= last; ++i)
        out += std::to_string(i) + "\n";
    return out;
}

/**
 * @brief A diff from its ---/+++ header or binary notice on, leaving out
 * the git-only lines before it: diff --git, file modes and index.
 */
std::string body(const std::string& diff) {
    const auto header = diff.starts_with("--- ") ? 0 : diff.find("\n--- ");
    if (header != std::string::npos)
        return diff.substr(header == 0 ? 0 : header + 1);
    const auto binary = diff.find("Binary files ");
    return binary == std::string::npos ? "" : diff.substr(binary);
}

void replace_all(std::string& text, const std::string& from,
    const std::string& to) {
    for (auto at = text.find(from); at != std::string::npos;
        at = text.find(from, at + to.size()))
        text.replace(at, from.size(), to);
}

/**
 * @brief What git diff --no-index prints for @p before and @p after, with
 * an absent side given as /dev/null and each file named doc/f.org.
 */
std::string git_diff(const std::optional<std::string>& before,
    const std::optional<std::string>& after) {
    static int counter = 0;
    const auto dir = std::filesystem::temp_directory_path() /
        ("compass.codec.diff." +
            std::to_string(
                compass::platform::process::current_process_id()) +
            "." + std::to_string(counter++));
    std::filesystem::create_directories(dir);
    const auto put = [&](const std::optional<std::string>& bytes,
        const char* name) -> std::string {
        if (!bytes)
            return "/dev/null";
        std::ofstream(dir / name, std::ios::binary) << *bytes;
        return name;
    };
    const auto a = put(before, "before");
    const auto b = put(after, "after");
    using namespace compass::platform::process;
    set_environment_variable("GIT_CONFIG_NOSYSTEM", "1");
    set_environment_variable("HOME", dir.generic_string());
    const auto r = run_program({{"git", "diff", "--no-index", "--no-color",
        "--no-ext-diff", "--diff-algorithm=myers", "--no-indent-heuristic",
        "-U3", a, b}, dir, {}});
    REQUIRE(r.exit_code >= 0);
    REQUIRE(r.exit_code <= 1);
    auto text = r.output;
    std::filesystem::remove_all(dir);
    if (before)
        replace_all(text, "a/" + a, "a/doc/f.org");
    if (after)
        replace_all(text, "b/" + b, "b/doc/f.org");
    return text;
}

/**
 * @brief Checks that unified_diff prints what git does for @p before and
 * @p after, from the ---/+++ header on, and returns its diff.
 */
std::string agrees(const std::optional<std::string>& before,
    const std::optional<std::string>& after) {
    const std::optional<std::string_view> a(before);
    const std::optional<std::string_view> b(after);
    const auto ours = unified_diff("doc/f.org", a, b);
    const auto theirs = git_diff(before, after);
    CHECK(ours == body(theirs));
    return ours;
}

}

TEST_CASE("an added file is a diff from /dev/null", "[ops][unified_diff]") {
    const auto d = agrees(std::nullopt, numbers(1, 3));
    CHECK(d == "--- /dev/null\n+++ b/doc/f.org\n@@ -0,0 +1,3 @@\n"
               "+1\n+2\n+3\n");
}

TEST_CASE("a removed file is a diff to /dev/null", "[ops][unified_diff]") {
    const auto d = agrees(numbers(1, 2), std::nullopt);
    CHECK(d == "--- a/doc/f.org\n+++ /dev/null\n@@ -1,2 +0,0 @@\n-1\n-2\n");
}

TEST_CASE("a change in the middle has three lines of context",
    "[ops][unified_diff]") {
    auto after = numbers(1, 9) + "ten\n" + numbers(11, 20);
    const auto d = agrees(numbers(1, 20), after);
    CHECK(d == "--- a/doc/f.org\n+++ b/doc/f.org\n@@ -7,7 +7,7 @@\n"
               " 7\n 8\n 9\n-10\n+ten\n 11\n 12\n 13\n");
}

TEST_CASE("changes at both ends make two hunks", "[ops][unified_diff]") {
    const auto after = "0\n" + numbers(2, 19) + "21\n";
    const auto d = agrees(numbers(1, 20), after);
    CHECK(d == "--- a/doc/f.org\n+++ b/doc/f.org\n"
               "@@ -1,4 +1,4 @@\n-1\n+0\n 2\n 3\n 4\n"
               "@@ -17,4 +17,4 @@\n 17\n 18\n 19\n-20\n+21\n");
}

TEST_CASE("a missing newline at the end is marked on either side",
    "[ops][unified_diff]") {
    (void)agrees(std::string("1\n2\n3"), std::string("1\n2\n4"));
    (void)agrees(std::string("1\n2\n3"), std::string("1\n2\n3\n"));
    const auto d = agrees(std::string("1\n2\n3\n"), std::string("1\n2\n3"));
    CHECK(d == "--- a/doc/f.org\n+++ b/doc/f.org\n@@ -1,3 +1,3 @@\n"
               " 1\n 2\n-3\n+3\n\\ No newline at end of file\n");
}

TEST_CASE("empty files diff against content and are equal to each other",
    "[ops][unified_diff]") {
    (void)agrees(std::string(), std::string("1\n"));
    (void)agrees(std::string("1\n"), std::string());
    (void)agrees(std::string("1"), std::string());
    CHECK(unified_diff("f", std::string_view(), std::string_view()).empty());
    CHECK(unified_diff("f", std::nullopt, std::string_view()) ==
        "--- /dev/null\n+++ b/f\n");
}

TEST_CASE("equal content has no diff", "[ops][unified_diff]") {
    const auto text = numbers(1, 50);
    CHECK(unified_diff("f", std::string_view(text), std::string_view(text))
        .empty());
    CHECK(git_diff(text, text).empty());
}

TEST_CASE("a large file with a few changes matches git's hunks",
    "[ops][unified_diff]") {
    const auto before = numbers(1, 5000);
    // A change, a deletion six lines on that shares its hunk, an insertion
    // seven lines on that does not, and a change near the end.
    const auto after = numbers(1, 99) + "100 changed\n" + numbers(101, 106) +
        numbers(108, 114) + "114.5\n" + numbers(115, 2000) + "2000.5\n" +
        numbers(2001, 4994) + "4995 changed\n" + numbers(4996, 5000);
    const auto d = agrees(before, after);
    CHECK(d.find("@@ -97,14 +97,13 @@\n") != std::string::npos);
    CHECK(d.find("@@ -112,6 +111,7 @@\n") != std::string::npos);
}

TEST_CASE("many changes, insertions and deletions match git",
    "[ops][unified_diff]") {
    std::string before;
    std::string after;
    for (int i = 0; i < 400; ++i) {
        const auto n = std::to_string(i);
        if (i % 7 != 3)
            before += n + "\n";
        if (i % 11 != 5)
            after += (i % 13 == 0 ? n + " changed" : n) + "\n";
    }
    (void)agrees(before, after);
}

TEST_CASE("a NUL in the first 8000 bytes makes a file binary",
    "[ops][unified_diff]") {
    const std::string binary("1\n\0\n", 4);
    const auto d = agrees(std::string("1\n"), binary);
    CHECK(d == "Binary files a/doc/f.org and b/doc/f.org differ\n");
    CHECK(agrees(std::nullopt, binary) ==
        "Binary files /dev/null and b/doc/f.org differ\n");
    const auto late = std::string(8000, '1') + std::string(1, '\0');
    CHECK(unified_diff("f", std::string_view("1\n"), std::string_view(late))
        .starts_with("--- a/f\n"));
}
