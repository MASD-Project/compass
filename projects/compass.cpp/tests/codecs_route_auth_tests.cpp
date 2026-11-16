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
#include "compass.cpp/codecs/route_auth.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::cpp;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

files tree() {
    return {
        {"projects/ores.http/src/declared.cpp",
         "void a(router& r) {\n"
         "r.add_route(r.get(\"/1\").auth_required().build());\n"
         "r.add_route(r.get(\"/2\").auth_required().build());\n"
         "r.add_route(r.get(\"/3\").auth_required().build());\n"
         "r.add_route(r.get(\"/4\").auth_required().build());\n"
         "r.add_route(r.get(\"/5\").auth_required().build());\n"
         "r.add_route(r.get(\"/6\").auth_required().build());\n"
         "r.add_route(r.get(\"/7\").auth_required().build());\n"
         "r.add_route(r.get(\"/8\").auth_required().build());\n"
         "r.add_route(r.get(\"/9\").auth_required().build());\n"
         "r.add_route(r.get(\"/10\").roles({\"admin\"}).build());\n"
         "r.add_route(r.get(\"/11\").auth_optional().build());\n"
         "r.add_route(r.get(\"/12\").auth_optional().build());\n"
         "r.add_route(r.get(\"/13\").auth_optional().build());\n"
         "r.add_route(r.get(\"/14\").auth_optional().build());\n"
         "r.add_route(r.get(\"/15\").auth_optional().build());\n"
         "r.add_route(r.get(\"/16\").auth_optional().build());\n"
         "}\n"},
        {"projects/ores.http/src/routes.cpp",
         "void b(router& r) {\n"
         "    r.add_route(r.post(\"/open\").build());\n"
         "    auto p = r.put(\"/aliased\");\n"
         "    auto built = p.build();\n"
         "    r.add_route(built);\n"
         "    auto q = r.get(\"/ok\").auth_optional();\n"
         "    r.add_route(q.build());\n"
         "    // r.add_route(r.get(\"/comment\").build());\n"
         "    r.add_route(unknown.build());\n"
         "    r.add_route(r.delete_(\"/quoted\").description(\".auth_required(\").build());\n"
         "}\n"},
        {"projects/ores.http/tests/t.cpp",
         "void t(router& r) { r.add_route(r.get(\"/t\").build()); }\n"}};
}

}

TEST_CASE("a route registered with no authentication position is reported",
    "[codecs][route_auth]") {
    const auto r = tests::run(codecs::route_auth, tree());
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.http/src/routes.cpp", 2},
        {"projects/ores.http/src/routes.cpp", 5},
        {"projects/ores.http/src/routes.cpp", 10}});
}

TEST_CASE("a scan that finds fewer than 20 routes fails",
    "[codecs][route_auth]") {
    auto t = tree();
    t.erase("projects/ores.http/src/declared.cpp");
    const auto r = tests::run(codecs::route_auth, t);
    CHECK(r.findings.size() == 4);
}
