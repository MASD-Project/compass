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
#include "compass.codec/ops/hand_edits.hpp"

#include <string>
#include <catch2/catch_test_macros.hpp>

namespace {

using compass::codec::ops::dropped_hand_lines;

const std::string generated =
    "#include \"repository.hpp\"\n"
    "\n"
    "std::vector<fixing> repository::read(context& ctx) {\n"
    "    const auto query = select_all();\n"
    "    return execute(ctx, query);\n"
    "}\n"
    "\n"
    "void repository::write(context& ctx, const fixing& f) {\n"
    "    execute(ctx, insert(f));\n"
    "}\n";

}

TEST_CASE("a function written into a generated file is a block the write "
    "drops", "[ops][hand_edits]") {
    const std::string on_disk =
        "#include \"repository.hpp\"\n"
        "\n"
        "std::vector<fixing> repository::read(context& ctx) {\n"
        "    const auto query = select_all();\n"
        "    return execute(ctx, query);\n"
        "}\n"
        "\n"
        "std::vector<fixing> repository::read_latest_for_series(\n"
        "    context& ctx, const std::string& series) {\n"
        "    const auto query = select_all().where(series_is(series))\n"
        "        .order_by_desc(as_of()).limit(1);\n"
        "    return execute(ctx, query);\n"
        "}\n"
        "\n"
        "void repository::write(context& ctx, const fixing& f) {\n"
        "    execute(ctx, insert(f));\n"
        "}\n";

    const auto block = dropped_hand_lines(on_disk, generated);

    REQUIRE(block);
    CHECK(block->line == 8);
    CHECK(block->lines == 4);
    CHECK(block->first == "std::vector<fixing> repository::read_latest_for_"
        "series(");
}

TEST_CASE("what the output rewrites, reindents, moves or barely trims is "
    "not reported", "[ops][hand_edits]") {
    CHECK_FALSE(dropped_hand_lines(generated, generated));

    const std::string old_template =
        "#include \"repository.hpp\"\n"
        "\n"
        "std::vector<fixing> repository::read(context& ctx) {\n"
        "    const auto q = select_everything();\n"
        "    log(\"reading\");\n"
        "    return run(ctx, q);\n"
        "}\n"
        "\n"
        "void repository::write(context& ctx, const fixing& f) {\n"
        "    execute(ctx, insert(f));\n"
        "}\n";
    CHECK_FALSE(dropped_hand_lines(old_template, generated));

    const std::string reindented =
        "#include \"repository.hpp\"\n"
        "\n"
        "std::vector<fixing> repository::read(context& ctx) {\n"
        "  const auto query = select_all();\n"
        "  return execute(ctx, query);\n"
        "}\n"
        "\n"
        "void repository::write(context& ctx, const fixing& f) {\n"
        "  execute(ctx, insert(f));\n"
        "}\n";
    CHECK_FALSE(dropped_hand_lines(reindented, generated));

    const std::string moved =
        "#include \"repository.hpp\"\n"
        "\n"
        "void repository::write(context& ctx, const fixing& f) {\n"
        "    execute(ctx, insert(f));\n"
        "}\n"
        "\n"
        "std::vector<fixing> repository::read(context& ctx) {\n"
        "    const auto query = select_all();\n"
        "    return execute(ctx, query);\n"
        "}\n";
    CHECK_FALSE(dropped_hand_lines(moved, generated));

    const std::string two_more = generated + "// one\n// two\n";
    CHECK_FALSE(dropped_hand_lines(two_more, generated));
    const std::string three_more = generated + "// one\n\n// two\n// three\n";
    const auto three = dropped_hand_lines(three_more, generated);
    REQUIRE(three);
    CHECK(three->lines == 3);
    CHECK(three->line == 11);
}

TEST_CASE("a removed line that starts like a diff header still counts",
    "[ops][hand_edits]") {
    const std::string sql = "create table t (x int);\n";
    const auto block = dropped_hand_lines(sql +
        "-- hand-written view\n--- a note\n+++ another\n", sql);
    REQUIRE(block);
    CHECK(block->lines == 3);
    CHECK(block->first == "-- hand-written view");
}
