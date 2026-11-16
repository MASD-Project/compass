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
#include "compass.cpp/codecs/forward_declarable_includes.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::cpp;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

files tree() {
    return {
        {"build/scripts/forward_declarable_types.list",
         "# name keyword defining forwarding\n"
         "ores::db::pq::context class ores.db/pq/context.hpp "
         "ores.db/pq/context_fwd.hpp\n"
         "ores::db::pool struct ores.db/pool.hpp -\n"},
        {"build/scripts/forward_declarable_includes.baseline",
         "ores::db::pool:projects/ores.x/include/ores.x/pointer.hpp\n"},
        {"projects/ores.db/include/ores.db/pq/context.hpp",
         "namespace ores::db::pq { class context { int x; }; }\n"},
        {"projects/ores.x/include/ores.x/pointer.hpp",
         "#pragma once\n"
         "#include \"ores.db/pq/context.hpp\"\n"
         "#include \"ores.db/pool.hpp\"\n"
         "void f(const ores::db::pq::context* c);\n"
         "void g(std::shared_ptr<db::pool> p);\n"},
        {"projects/ores.x/include/ores.x/value.hpp",
         "#include \"ores.db/pq/context.hpp\"\n"
         "struct holder { pq::context c; };\n"},
        {"projects/ores.x/include/ores.x/calls.hpp",
         "#include \"ores.db/pq/context.hpp\"\n"
         "inline int h(const pq::context& ctx) { return ctx.size(); }\n"},
        {"projects/ores.x/include/ores.x/comment.hpp",
         "#include \"ores.db/pq/context.hpp\"\n"
         "// pq::context* in a comment\n"},
        {"projects/ores.x/include/ores.x/opened.hpp",
         "#include \"ores.db/pool.hpp\"\n"
         "namespace ores::db {\n"
         "class pool;\n"
         "void use(pool& p);\n"
         "}\n"},
        {"projects/ores.x/build/skipped.hpp",
         "#include \"ores.db/pool.hpp\"\nvoid g(db::pool* p);\n"}};
}

}

TEST_CASE("a header that includes a definition it only names is reported "
    "on the include, baseline or not",
    "[codecs][forward_declarable_includes]") {
    const auto r = tests::run(codecs::forward_declarable_includes, tree());
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.x/include/ores.x/opened.hpp", 1},
        {"projects/ores.x/include/ores.x/pointer.hpp", 2},
        {"projects/ores.x/include/ores.x/pointer.hpp", 3}});
}

TEST_CASE("an empty type list turns the guard off, which is reported",
    "[codecs][forward_declarable_includes]") {
    auto t = tree();
    t["build/scripts/forward_declarable_types.list"] = "# nothing\n";
    CHECK(tests::places(tests::run(codecs::forward_declarable_includes,
        t).findings) == places{
        {"build/scripts/forward_declarable_types.list", 1}});
}
