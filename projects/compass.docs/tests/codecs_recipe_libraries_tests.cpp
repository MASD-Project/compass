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

#include "compass.docs/codecs/recipe_libraries.hpp"
#include "test_tree.hpp"

namespace {

using namespace compass::docs;

const std::string_view header =
    "#+title: List accounts\n"
    "#+description: Lists every account\n"
    "#+filetags: :recipe:shell:accounts:\n";

}

TEST_CASE("a recipe that names no target is one script, banner first and "
    "exit dropped", "[codecs][recipe_libraries]") {
    const auto files = tests::generate(codecs::shell_recipe_library, {
        {"doc/recipes/shell/accounts/list.org", std::string(header) +
            "* Run\n"
            "#+begin_src ores-shell :exports none\naccounts list\nexit\n"
            "#+end_src\n"
            "#+begin_src sh :results output\n./ores.shell\n#+end_src\n"}});

    REQUIRE(files.size() == 1);
    CHECK(files.at("projects/ores.shell/scripts/library/accounts/list.ores") ==
        "# List accounts\n"
        "# Lists every account\n"
        "#\n"
        "# GENERATED from doc/recipes/shell/accounts/list.org \xe2\x80\x94 "
        "do not edit by hand.\n"
        "# Regenerate with: ./compass.sh build --direct tangle_shell_scripts\n"
        "#\n"
        "accounts list\n");
}

TEST_CASE("a recipe that names targets gives one script per target, led by "
    "its section's heading", "[codecs][recipe_libraries]") {
    const auto files = tests::generate(codecs::shell_recipe_library, {
        {"doc/recipes/shell/accounts/verbs.org", std::string(header) +
            "* =add= verb\n"
            "#+begin_src ores-shell :tangle accounts-add.ores :exports none\n"
            "accounts add x\n#+end_src\n"
            "* =set=\n"
            "#+begin_src ores-shell :tangle out/accounts-set.ores "
            ":exports none\naccounts set y\n#+end_src\n"
            "#+begin_src ores-shell :exports none\nnot tangled\n#+end_src\n"}});

    REQUIRE(files.size() == 2);
    const std::string library = "projects/ores.shell/scripts/library/";
    CHECK(files.at(library + "accounts/accounts-add.ores") ==
        "# =add= verb\n#\n"
        "# GENERATED from doc/recipes/shell/accounts/verbs.org \xe2\x80\x94 "
        "do not edit by hand.\n"
        "# Regenerate with: ./compass.sh build --direct tangle_shell_scripts\n"
        "#\naccounts add x\n");
    CHECK(files.at(library + "accounts/accounts-set.ores").ends_with(
        "#\naccounts set y\n"));
}

TEST_CASE("a recipe with no category tag goes to general, and a description "
    "equal to the title is not repeated", "[codecs][recipe_libraries]") {
    const auto files = tests::generate(codecs::shell_recipe_library, {
        {"doc/recipes/shell/x.org",
            "#+TITLE: Same\n#+description: Same\n"
            "#+begin_src ores-shell\nexit  \n#+end_src\n"},
        {"doc/other/y.org", "#+begin_src ores-shell\nhelp\n#+end_src\n"}});

    REQUIRE(files.size() == 1);
    CHECK(files.at("projects/ores.shell/scripts/library/general/x.ores")
              .starts_with("# Same\n#\n# GENERATED"));
    CHECK(files.at("projects/ores.shell/scripts/library/general/x.ores")
              .ends_with("tangle_shell_scripts\n#\n\n"));
}

TEST_CASE("the HTTP library keeps every line, names no description, and "
    "skips a recipe without targets", "[codecs][recipe_libraries]") {
    const auto files = tests::generate(codecs::http_recipe_library, {
        {"doc/recipes/http/tags.org",
            "#+title: Tags\n#+description: The tags\n"
            "#+filetags: :recipe:http:tags:\n"
            "#+begin_src hurl :tangle tags-list.hurl :exports none\n"
            "GET {{base}}/tags\nexit\n#+end_src\n"},
        {"doc/recipes/http/prose.org",
            "#+begin_src hurl :exports none\nGET /\n#+end_src\n"}});

    REQUIRE(files.size() == 1);
    CHECK(files.at("projects/ores.http/scripts/library/tags/tags-list.hurl") ==
        "# Tags\n#\n"
        "# GENERATED from doc/recipes/http/tags.org \xe2\x80\x94 do not edit "
        "by hand.\n"
        "# Regenerate with: ./compass.sh build --direct tangle_http_recipes\n"
        "#\nGET {{base}}/tags\nexit\n");
}

TEST_CASE("a document may set :var in header-args, which leaves the script "
    "alone, but no other argument", "[codecs][recipe_libraries]") {
    const auto files = tests::generate(codecs::shell_recipe_library, {
        {"doc/recipes/shell/var.org",
            "#+property: header-args :var login=\"me\"\n"
            "#+begin_src ores-shell\nlogin\n#+end_src\n"},
        {"doc/recipes/shell/wrap.org",
            "#+property: header-args :wrap example\n"
            "#+begin_src ores-shell\nlogin\n#+end_src\n"}});

    CHECK(files.size() == 1);
    CHECK(files.contains(
        "projects/ores.shell/scripts/library/general/var.ores"));
}
