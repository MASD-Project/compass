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
#include "compass.cli/config/precedence.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace compass::cli;
namespace ct = compass::codec::types;

constexpr std::array<ct::option, 2> index_options{{
    {"path", "index.tsv", "Where the index goes."},
    {"columns", "4", "How many columns."}}};

const std::array<ct::descriptor, 2> catalogue = [] {
    std::array<ct::descriptor, 2> r;
    r[0].name = "index";
    r[0].options = index_options;
    r[1].name = "lint";
    r[1].enabled_by_default = false;
    return r;
}();

constexpr types::origin file_line(std::uint32_t line) {
    return {types::origin_kind::file, line};
}

constexpr types::origin command_line{types::origin_kind::command_line};

types::configuration resolve(const types::layer& file,
    const types::layer& command) {
    return config::resolve(catalogue, "/repo", "/repo/.compass.org", file,
        command);
}

}

TEST_CASE("with no layer every value is the default",
    "[config][precedence]") {
    const auto c = resolve({}, {});
    CHECK(c.problems.empty());
    CHECK(c.output.value == "/repo/build/output/compass");
    CHECK(c.output.from.kind == types::origin_kind::defaults);
    CHECK(c.meta.value == "/repo/doc/meta");
    REQUIRE(c.codecs.size() == 2);
    CHECK(c.codecs[0].name == "index");
    CHECK(c.codecs[0].enabled.value == "true");
    CHECK(c.codecs[0].keys == std::vector<std::string>{"path", "columns"});
    CHECK(c.codecs[0].options[0].value == "index.tsv");
    CHECK(c.codecs[0].options[1].from.kind == types::origin_kind::defaults);
    CHECK(c.codecs[1].enabled.value == "false");
}

TEST_CASE("the file overrides the defaults and the command line overrides "
    "the file, key by key", "[config][precedence]") {
    types::layer file;
    file.output = {"out", file_line(2)};
    file.meta = {"contracts", file_line(3)};
    file.settings = {{"index", "enabled", "false", file_line(8)},
        {"index", "path", "file.tsv", file_line(8)},
        {"lint", "enabled", "true", file_line(9)}};
    types::layer command;
    command.output = {"/elsewhere", command_line};
    command.settings = {{"index", "enabled", "true", command_line}};

    const auto c = resolve(file, command);

    CHECK(c.problems.empty());
    CHECK(c.output.value == "/elsewhere");
    CHECK(c.output.from.kind == types::origin_kind::command_line);
    CHECK(c.meta.value == "/repo/contracts");
    CHECK(c.meta.from.line == 3);
    const auto& index = c.codecs[0];
    CHECK(index.enabled.value == "true");
    CHECK(index.enabled.from.kind == types::origin_kind::command_line);
    CHECK(index.options[0].value == "file.tsv");
    CHECK(index.options[0].from.line == 8);
    CHECK(index.options[1].value == "4");
    CHECK(index.options[1].from.kind == types::origin_kind::defaults);
    CHECK(c.codecs[1].enabled.value == "true");
    CHECK(c.codecs[1].enabled.from.line == 9);

    const auto flat = config::settings_of(c);
    REQUIRE(flat.size() == 4);
    CHECK(flat.codecs[1] == "index");
    CHECK(flat.keys[1] == "path");
    CHECK(flat.values[1] == "file.tsv");
    CHECK(flat.keys[3] == "enabled");
    CHECK(flat.values[3] == "true");
}

TEST_CASE("a setting that matches no codec or option is a problem naming "
    "where it was written", "[config][precedence]") {
    types::layer file;
    file.settings = {{"html", "enabled", "true", file_line(7)},
        {"index", "colour", "red", file_line(8)}};
    types::layer command;
    command.settings = {{"index", "enabled", "perhaps", command_line}};

    const auto c = resolve(file, command);

    REQUIRE(c.problems.size() == 3);
    CHECK(c.problems[0] == ".compass.org:7: no codec is named 'html'; fix: "
        "use one of index, lint");
    CHECK(c.problems[1] == ".compass.org:8: codec 'index' has no option "
        "'colour'; fix: use one of path, columns");
    CHECK(c.problems[2] == "--set index.enabled=perhaps: index.enabled is "
        "'perhaps'; fix: set it to true or false");
    CHECK(c.codecs[0].enabled.value == "true");
}
