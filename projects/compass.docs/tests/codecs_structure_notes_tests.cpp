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

#include "compass.docs/codecs/structure_notes.hpp"
#include "test_tree.hpp"

namespace {

using namespace compass::docs;

const std::string hub_id = "AAAAAAAA-0000-0000-0000-000000000001";
const std::string a_id = "AAAAAAAA-0000-0000-0000-000000000002";
const std::string b_id = "AAAAAAAA-0000-0000-0000-000000000003";
const std::string c_id = "AAAAAAAA-0000-0000-0000-000000000004";

std::string page(const std::string& id, std::string_view rest) {
    return ":PROPERTIES:\n:ID: " + id + "\n:END:\n" + std::string(rest);
}

}

TEST_CASE("pages a structure note orders link back to it, those it sets "
    "aside say so, and pages outside its cluster are left alone",
    "[codecs][structure_notes]") {
    const auto out = tests::generate(codecs::structure_notes, {
        {"doc/hub.org", page(hub_id, "#+title: Curves\n"
            "#+filetags: :knowledge:structure_note:\n"
            "* Order\n[[id:" + a_id + "][a]]\n"
            "* Beside\n# aside\n[[id:" + b_id + "][b]]\n"
            "* See also\n[[id:" + c_id + "][c]]\n")},
        {"doc/a.org", page(a_id, "* Body\n\n* See also\n\n- other\n")},
        {"doc/b.org", page(b_id, "* Body\n\n")},
        {"doc/c.org", page(c_id, "* Body\n")}});

    REQUIRE(out.size() == 2);
    // As in the script, \s*$ ends after the heading's own newline, so the
    // line goes in after one more blank line.
    CHECK(out.at("doc/a.org") == page(a_id, "* Body\n\n* See also\n\n\n"
        "- [[id:" + hub_id + "][Curves]] \xe2\x80\x94 the structure note that "
        "orders this cluster, and where to read this page in it.\n- other\n"));
    CHECK(out.at("doc/b.org") == page(b_id, "* Body\n\n* See also\n\n"
        "- [[id:" + hub_id + "][Curves]] \xe2\x80\x94 the structure note for "
        "this cluster; this page sits alongside its sequence rather than in "
        "it.\n"));
}

TEST_CASE("a page that links its note already is written unchanged",
    "[codecs][structure_notes]") {
    const auto a = page(a_id, "See [[id:" + hub_id + "][hub]].\n");
    const auto out = tests::generate(codecs::structure_notes, {
        {"doc/hub.org", page(hub_id, "#+filetags: :structure_note:\n"
            "[[id:" + a_id + "][a]]\n")},
        {"doc/a.org", a}});

    CHECK(out == tests::files{{"doc/a.org", a}});
}
