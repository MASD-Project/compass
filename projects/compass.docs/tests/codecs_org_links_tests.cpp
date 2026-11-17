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
#include "compass.docs/codecs/org_links.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_tree.hpp"

namespace {

using namespace compass::docs;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

}

TEST_CASE("a link whose raw target begins with a colon is reported on its "
    "line, as Emacs parses it", "[codecs][org_links]") {
    const files t{
        {"doc/a.org",
         "* Shell\n"
         "Run =grep '[[:space:]]*= on it.\n"
         "A [[id:ABC][fine link]] and [[file:x.org]] and [[*Shell]].\n"
         "Text [[:alpha:]] again.\n"
         "=[[:verbatim:]]= stays verbatim.\n"
         "#+begin_src sh\ngrep '[[:digit:]]'\n#+end_src\n"
         "Use =grep -E '^[[:space:]]*$'= and =x= [[:upper:]]\n"
         "Close =a [[:lower:]]=b= early.\n"
         "- item ~x [[:cntrl:]]~ fine\n"},
        {"build/b.org", "[[:space:]]\n"},
        {"doc/tmp/c.org", "[[:space:]]\n"},
        {"doc/d.txt", "[[:space:]]\n"}};
    const auto r = tests::run(codecs::org_links, t);
    CHECK(tests::places(r) == places{{"doc/a.org", 4}, {"doc/a.org", 9}});
    CHECK(tests::messages(r).front() == "link org cannot resolve: [[:alpha:]]");
}
