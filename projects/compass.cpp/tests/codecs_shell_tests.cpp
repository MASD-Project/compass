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
#include "compass.cpp/codecs/shell.hpp"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::cpp;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

files tree() {
    return {
        {"projects/ores.shell/application/src/repl.cpp",
         "void wire() {\n"
         "    accounts_commands::register_commands(root);\n"
         "    accounts_commands::register_commands(root);\n"
         "    auto m = std::make_unique<cli::Menu>(\"accounts\");\n"
         "    root_menu.Insert(\n        \"quit\", quit);\n"
         "}\n"},
        {"projects/ores.shell/refdata/src/books.cpp",
         "auto a = std::make_unique<cli::Menu>(\"books\");\n"
         "auto b = std::make_unique<cli::Menu>( \"books\" );\n"
         "auto c = std::make_unique<cli::Menu>(\"accounts\");\n"
         "auto d = std::make_unique<cli::Menu>(\"roles\", \"Roles\");\n"},
        {"projects/ores.shell/refdata/tests/menu.cpp",
         "auto a = std::make_unique<cli::Menu>(\"accounts\");\n"},
        {"projects/ores.shell/refdata/include/ores.shell/app/commands/"
         "refdata/accounts_commands.hpp",
         "// AUTO-GENERATED FILE\n"},
        {"projects/ores.shell/refdata/include/ores.shell/app/commands/"
         "refdata/books_commands.hpp",
         "// AUTO-GENERATED FILE\n"},
        {"projects/ores.shell/refdata/include/ores.shell/app/commands/"
         "refdata/hand_commands.hpp",
         "// written by hand\n"}};
}

}

TEST_CASE("a menu built twice or by two files, and a unit registered "
    "twice, are reported", "[codecs][shell_command_units]") {
    const auto r = tests::run(codecs::shell_command_units, tree());
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.shell/application/src/repl.cpp", 3},
        {"projects/ores.shell/refdata/src/books.cpp", 2},
        {"projects/ores.shell/refdata/src/books.cpp", 3}});
}

TEST_CASE("a menu with a prompt and a name two units own are reported",
    "[codecs][shell_menus]") {
    const auto r = tests::run(codecs::shell_menus, tree());
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.shell/refdata/src/books.cpp", 4},
        {"projects/ores.shell/refdata/src/books.cpp", 3},
        {"projects/ores.shell/refdata/src/books.cpp", 2}});
}

TEST_CASE("a root command inserted on one line owns its name",
    "[codecs][shell_menus]") {
    auto all = tree();
    all["projects/ores.shell/refdata/src/quit.cpp"] =
        "void wire() { root_menu.Insert(\"quit\", quit); }\n";
    const auto r = tests::run(codecs::shell_menus, all);
    CHECK(std::ranges::count(tests::places(r.findings),
        std::pair<std::string, std::uint32_t>{
            "projects/ores.shell/refdata/src/quit.cpp", 1}) == 1);
}

TEST_CASE("a generated command unit no application source registers is "
    "reported", "[codecs][shell_command_composition]") {
    const auto r = tests::run(codecs::shell_command_composition, tree());
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.shell/refdata/include/ores.shell/app/commands/"
         "refdata/books_commands.hpp", 1}});
}

TEST_CASE("a root command in a comment owns no name",
    "[codecs][shell_menus]") {
    auto all = tree();
    all["projects/ores.shell/refdata/src/quit.cpp"] =
        "// root_menu.Insert(\"quit\", quit);\n"
        "/* root_menu.Insert(\"exit\", quit); */\n";
    CHECK(tests::places(tests::run(codecs::shell_menus, all).findings) ==
        tests::places(tests::run(codecs::shell_menus, tree()).findings));
}
