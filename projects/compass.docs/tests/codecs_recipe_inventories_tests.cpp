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
#include <catch2/catch_test_macros.hpp>

#include <string>
#include "compass.docs/codecs/recipe_inventories.hpp"
#include "test_tree.hpp"

namespace {

using namespace compass::docs;

const std::string shell_index = "doc/recipes/shell/shell.org";
const std::string http_index = "doc/recipes/http/generated_recipes.org";

std::string recipe(std::string_view id, std::string_view title,
    std::string_view tags) {
    return ":PROPERTIES:\n:ID: " + std::string(id) + "\n:END:\n#+title: " +
        std::string(title) + "\n#+filetags: " + std::string(tags) +
        "\n\n* Body\n#+title: Not the title\n";
}

}

TEST_CASE("the shell inventory keeps its prose and notes, sorts each list "
    "by title and appends a new category", "[codecs][recipe_inventories]") {
    const auto out = tests::generate(codecs::recipe_inventories, {
        {shell_index, "Head.\n\n* Market data\n\nPrices.\n\n"
            "- [[id:B][old]]   a note\n\n* Gone\n\n- [[id:Z][z]]\n"},
        {"doc/recipes/shell/marketdata/b.org",
            recipe("B", "beta", ":recipe:shell:marketdata:")},
        {"doc/recipes/shell/marketdata/a.org",
            recipe("A", "Alpha", ":recipe:shell:marketdata:")},
        {"doc/recipes/shell/fx_rates/c.org", recipe("C", "Gamma", ":x:")},
        {"doc/recipes/shell/top.org", recipe("T", "Top", ":x:")}});

    // The script keeps the first #+title:, before the first heading, and a
    // recipe outside any category directory or filetag is not listed.
    CHECK(out.at(shell_index) == "Head.\n\n* Market data\n\nPrices.\n\n"
        "- [[id:A][Alpha]]\n- [[id:B][beta]] a note\n\n* Fx Rates\n\n"
        "- [[id:C][Gamma]]\n");
}

TEST_CASE("the HTTP inventory lists recipes one directory deep in path "
    "order, under their paragraphs", "[codecs][recipe_inventories]") {
    const auto out = tests::generate(codecs::recipe_inventories, {
        {"doc/recipes/shell/shell.org", "Head.\n"},
        {http_index, "old\n* Book Keeping\n\nKept paragraph.\n\n"
            "- [[id:Y][y]] noted\n"},
        {"doc/recipes/http/book-x/z.org",
            recipe("X", "Zed", ":recipe:http:book_keeping:")},
        {"doc/recipes/http/book/y.org",
            recipe("Y", "Why", ":recipe:http:book_keeping:")},
        {"doc/recipes/http/flat.org",
            recipe("F", "Flat", ":recipe:http:book_keeping:")}});

    const auto& text = out.at(http_index);
    CHECK(text.starts_with(":PROPERTIES:\n:ID: 3EB4B454-B837-41FA-8A8D-"));
    // pathlib sorts book/ before book-x/, though '-' sorts before '/', and
    // the HTTP script keeps the last #+title: of a file.
    CHECK(text.ends_with("=doc/recipes/http/http.org=.\n\n"
        "* Book Keeping\n\nKept paragraph.\n\n"
        "- [[id:Y][Not the title]] noted\n- [[id:X][Not the title]]\n"));
}

TEST_CASE("an inventory with a merge conflict marker is not written",
    "[codecs][recipe_inventories]") {
    const auto out = tests::generate(codecs::recipe_inventories, {
        {shell_index, "Head.\n=======\n"},
        {http_index, "Head.\n>>>>>>> theirs\n"}});
    CHECK(out.empty());
}
