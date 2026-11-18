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
#include "compass.bench/ops/include_resolve.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.bench/ops/listing.hpp"
#include "compass.bench/ops/search_paths.hpp"
#include "compass.platform/process/environment.hpp"

namespace {

using namespace compass::bench;

constexpr auto quoted = types::include_kind::quoted;
constexpr auto angle = types::include_kind::angle;

class scoped_directory final {
public:
    scoped_directory() {
        static int counter = 0;
        root_ = std::filesystem::temp_directory_path() /
            ("compass.bench.resolve." +
                std::to_string(
                    compass::platform::process::current_process_id()) +
                "." + std::to_string(counter++));
        std::filesystem::create_directories(root_);
    }
    ~scoped_directory() { std::filesystem::remove_all(root_); }
    scoped_directory(const scoped_directory&) = delete;
    scoped_directory& operator=(const scoped_directory&) = delete;

    void touch(const std::string& relative) {
        const auto path = root_ / relative;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path) << "\n";
    }

    std::string path() const { return root_.generic_string(); }

private:
    std::filesystem::path root_;
};

/**
 * @brief A file table plus helpers to resolve one include at a time.
 */
struct fixture {
    types::file_table table;
    types::search_paths paths;

    explicit fixture(std::vector<std::string_view> files) {
        for (const auto f : files)
            ops::add_file(table, f, 1);
    }

    void build(const std::vector<types::compile_command>& commands,
        const std::string& source_root,
        const std::vector<std::string>& implicit = {}) {
        paths = ops::build_search_paths(commands, source_root, implicit,
            table);
    }

    std::uint32_t id(std::string_view path) const {
        const auto it = std::ranges::find(table.paths, path);
        REQUIRE(it != table.paths.end());
        return static_cast<std::uint32_t>(it - table.paths.begin());
    }

    std::uint32_t resolve(std::string_view includer, types::include_kind kind,
        std::string_view name) const {
        types::include_table includes;
        includes.includers.push_back(id(includer));
        includes.lines.push_back(1);
        includes.kinds.push_back(kind);
        includes.name_offsets.push_back(0);
        includes.name_sizes.push_back(static_cast<std::uint32_t>(name.size()));
        includes.names.assign(name);
        ops::resolve_includes(includes, table, paths);
        return includes.targets.at(0);
    }
};

}

TEST_CASE("join_normal folds dot segments and refuses to leave the root",
    "[ops][include_resolve]") {
    std::string out;
    CHECK(ops::join_normal(out, "a/b", "../c/./d.hpp"));
    CHECK(out == "a/c/d.hpp");
    CHECK(ops::join_normal(out, "", "x.hpp"));
    CHECK(out == "x.hpp");
    CHECK(ops::join_normal(out, "/usr/include", "../../../x.h"));
    CHECK(out == "/x.h");
    CHECK_FALSE(ops::join_normal(out, "a", "../../x.hpp"));
}

TEST_CASE("quoted includes search the includer's directory and -iquote; "
          "angle includes do not",
    "[ops][include_resolve]") {
    fixture f({"src/a.cpp", "src/x.hpp", "quote/x.hpp", "quote/q.hpp",
        "user/x.hpp", "user/y.hpp", "user/z.hpp", "sys/z.hpp", "sys/s.hpp"});
    f.build({{"/ws/build", "/ws/src/a.cpp",
                {"c++", "-iquote", "/ws/quote", "-I/ws/user", "-isystem",
                    "/ws/sys", "-c", "/ws/src/a.cpp"}}},
        "/ws");

    CHECK(f.resolve("src/a.cpp", quoted, "x.hpp") == f.id("src/x.hpp"));
    CHECK(f.resolve("src/a.cpp", angle, "x.hpp") == f.id("user/x.hpp"));
    CHECK(f.resolve("src/a.cpp", quoted, "q.hpp") == f.id("quote/q.hpp"));
    CHECK(f.resolve("src/a.cpp", angle, "q.hpp") == types::unresolved_target);
    CHECK(f.resolve("src/a.cpp", quoted, "y.hpp") == f.id("user/y.hpp"));
    CHECK(f.resolve("src/a.cpp", angle, "z.hpp") == f.id("user/z.hpp"));
    CHECK(f.resolve("src/a.cpp", angle, "s.hpp") == f.id("sys/s.hpp"));
    CHECK(f.resolve("src/a.cpp", quoted, "s.hpp") == f.id("sys/s.hpp"));
    CHECK(f.resolve("src/a.cpp", quoted, "../quote/q.hpp") ==
        f.id("quote/q.hpp"));
    CHECK(f.resolve("src/a.cpp", quoted, "../../a.cpp") ==
        types::unresolved_target);
    CHECK(f.resolve("user/y.hpp", quoted, "x.hpp") == f.id("user/x.hpp"));
}

TEST_CASE("include directories are rebased onto the checkout and contexts "
          "follow the component",
    "[ops][include_resolve]") {
    scoped_directory external;
    external.touch("e.hpp");
    scoped_directory implicit;
    implicit.touch("vector");

    fixture f({"projects/p/src/p.cpp", "projects/p/include/p/p.hpp",
        "projects/q/src/q.cpp", "projects/q/include/q/q.hpp",
        "projects/shared/include/s.hpp", "tools/t.cpp",
        "build/projects/p/generated/gen.hpp"});
    f.build({{"/old/ws/build",
                 "/old/ws/build/projects/p/src/CMakeFiles/p.dir/Unity/"
                 "unity_0_cxx.cxx",
                 {"c++", "-I/old/ws/projects/p/src/../include",
                     "-I/old/ws/build/projects/p/generated", "-isystem",
                     external.path(), "-c", "unity_0_cxx.cxx"}},
                {"/old/ws/build", "/old/ws/projects/q/src/q.cpp",
                    {"c++", "-I", "/old/ws/projects/q/include",
                        "-I../projects/shared/include", "-c", "q.cpp"}}},
        "/old/ws/", {implicit.path()});

    const auto has_directory = [&](std::string_view path, bool internal) {
        return std::ranges::any_of(f.paths.directories, [&](const auto& d) {
            return d.path == path && d.internal == internal;
        });
    };
    CHECK(has_directory("projects/p/include", true));
    CHECK(has_directory("projects/shared/include", true));
    CHECK(has_directory("/old/ws/build/projects/p/generated", false));
    CHECK(has_directory(external.path(), false));

    CHECK(f.resolve("projects/p/src/p.cpp", angle, "p/p.hpp") ==
        f.id("projects/p/include/p/p.hpp"));
    CHECK(f.resolve("projects/p/include/p/p.hpp", angle, "p/p.hpp") ==
        f.id("projects/p/include/p/p.hpp"));
    CHECK(f.resolve("projects/q/src/q.cpp", angle, "p/p.hpp") ==
        types::unresolved_target);
    CHECK(f.resolve("projects/q/src/q.cpp", angle, "s.hpp") ==
        f.id("projects/shared/include/s.hpp"));
    CHECK(f.resolve("projects/p/src/p.cpp", angle, "gen.hpp") ==
        types::unresolved_target);
    CHECK(f.resolve("projects/p/src/p.cpp", angle, "e.hpp") ==
        types::external_target);
    CHECK(f.resolve("projects/q/src/q.cpp", angle, "e.hpp") ==
        types::unresolved_target);
    CHECK(f.resolve("projects/q/src/q.cpp", angle, "vector") ==
        types::external_target);
    CHECK(f.resolve("tools/t.cpp", angle, "p/p.hpp") ==
        f.id("projects/p/include/p/p.hpp"));
    CHECK(f.resolve("tools/t.cpp", angle, "q/q.hpp") ==
        f.id("projects/q/include/q/q.hpp"));
}
