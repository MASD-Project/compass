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
#include "compass.store/ops/git_settings.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace compass::store;

bool converts(std::string_view attributes, std::string_view path) {
    types::attribute_rules rules;
    ops::read_attributes(attributes, "", rules);
    return ops::converts(rules, path);
}

types::git_config config(std::string_view text) {
    types::git_config c;
    ops::read_git_config(text, c);
    return c;
}

}

TEST_CASE("attributes that set a conversion match by git's pattern rules",
    "[ops][git_settings]") {
    CHECK(converts("*.txt eol=lf\n", "a/b/c.txt"));
    CHECK_FALSE(converts("*.txt eol=lf\n", "a/b/c.txtx"));
    CHECK(converts("a/** text\n", "a/b/c"));
    CHECK_FALSE(converts("a/** text\n", "b/a/c"));
    CHECK(converts("**/gen/*.sql filter=lfs\n", "x/y/gen/t.sql"));
    CHECK(converts("**/gen/*.sql filter=lfs\n", "gen/t.sql"));
    CHECK_FALSE(converts("**/gen/*.sql filter=lfs\n", "gen/x/t.sql"));
    CHECK(converts("a/**/z text\n", "a/z"));
    CHECK(converts("a/**/z text\n", "a/b/c/z"));
    CHECK(converts("/top.c text\n", "top.c"));
    CHECK_FALSE(converts("/top.c text\n", "sub/top.c"));
    CHECK_FALSE(converts("src/*.c text\n", "src/x/y.c"));
    CHECK(converts("file[0-9].c ident\n", "d/file7.c"));
    CHECK_FALSE(converts("file[!0-9].c ident\n", "d/file7.c"));
    CHECK(converts("\"with space.c\" text\n", "with space.c"));
    CHECK(converts("*.[[:alpha:]] text\n", "anything"));
}

TEST_CASE("attributes that do not set a conversion are ignored",
    "[ops][git_settings]") {
    CHECK_FALSE(converts("*.png binary\n", "a.png"));
    CHECK_FALSE(converts("*.c -text !eol diff\n", "a.c"));
    CHECK_FALSE(converts("# *.c text\n", "a.c"));
    CHECK_FALSE(converts("!*.c text\n", "a.c"));
    CHECK_FALSE(converts("dir/ text\n", "dir/a.c"));
}

TEST_CASE("a macro that sets a conversion converts where it is used",
    "[ops][git_settings]") {
    CHECK(converts("[attr]unix text eol=lf\n*.sh unix\n", "run.sh"));
    CHECK_FALSE(converts("[attr]plain -text\n*.sh plain\n", "run.sh"));
}

TEST_CASE("a rule of a nested attributes file applies below its directory",
    "[ops][git_settings]") {
    types::attribute_rules rules;
    ops::read_attributes("*.c text\n", "sub/", rules);
    CHECK(ops::converts(rules, "sub/a.c"));
    CHECK(ops::converts(rules, "sub/deep/a.c"));
    CHECK_FALSE(ops::converts(rules, "a.c"));
    CHECK_FALSE(ops::converts(rules, "subway/a.c"));
}

TEST_CASE("the configuration reader finds autocrlf, the object format and "
    "includes", "[ops][git_settings]") {
    CHECK_FALSE(config("[core]\n\tautocrlf = false\n").autocrlf);
    CHECK(config("[core]\n\tautocrlf = true\n").autocrlf);
    CHECK(config("[CORE]\n  AutoCRLF=\"input\" # comment\n").autocrlf);
    CHECK(config("[core]\n\tautocrlf\n").autocrlf);
    CHECK_FALSE(config("[other]\n\tautocrlf = true\n").autocrlf);
    CHECK_FALSE(config("[core]\n\tautocrlfx = true\n").autocrlf);
    CHECK_FALSE(config("[core]\n\tbare = false\n").autocrlf);
    CHECK_FALSE(config("[extensions]\n\tobjectFormat = sha256\n").sha1);
    CHECK(config("[extensions]\n\tobjectformat = sha1\n").sha1);

    const auto c = config("[include]\n\tpath = a.conf\n"
                          "[includeIf \"gitdir:~/x/\"]\n\tpath = b.conf\n"
                          "[core]\n\tattributesFile = ~/attrs\n");
    CHECK(c.includes == std::vector<std::string>{"a.conf", "b.conf"});
    CHECK(c.attributes_files == std::vector<std::string>{"~/attrs"});
}
