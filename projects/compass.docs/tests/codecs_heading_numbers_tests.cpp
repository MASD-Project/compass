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
#include "compass.docs/codecs/heading_numbers.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_tree.hpp"

namespace {

using namespace compass::docs;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

}

TEST_CASE("a numbered heading outside a block is reported on its line",
    "[codecs][heading_numbers]") {
    const files t{
        {"doc/a.org",
         "* 1. Intro\n"
         "** 2.1. The shape\r\n"
         "*** 3) Third\n"
         "* 2024 roadmap\n"
         "* 1.Tight\n"
         "#+BEGIN_SRC org\n* 4. Inside\n#+end_src\n"
         "* 5. After\n"
         "text #+begin_src is not an opening\n* 6. Still read\n"
         "#+begin_example\n#+end_example glued * 7. no\n* 8. Read again\n"},
        {"projects/ores.a/modeling/b.org", "* 1. Model\n"},
        {"projects/ores.a/build/c.org", "* 1. Built\n"},
        {"other/d.org", "* 1. Outside\n"}};
    CHECK(tests::places(tests::run(codecs::heading_numbers, t)) ==
        places{{"doc/a.org", 1}, {"doc/a.org", 2}, {"doc/a.org", 3},
            {"doc/a.org", 9}, {"doc/a.org", 11}, {"doc/a.org", 14},
            {"projects/ores.a/modeling/b.org", 1}});
}

TEST_CASE("an unclosed block hides the headings after it",
    "[codecs][heading_numbers]") {
    const files t{{"doc/a.org",
        "#+begin_quote\n* 1. Hidden\n"}};
    CHECK(tests::run(codecs::heading_numbers, t).empty());
}
