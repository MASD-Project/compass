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
#include "compass.cpp/codecs/core_link_edges.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::cpp;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

files tree() {
    return {
        {"projects/CMakeLists.txt", "add_subdirectory(ores.a)\n"},
        {"projects/ores.a/core/src/CMakeLists.txt",
         "set(name \"ores.a.core\")\n"
         "set(lib_target_name ${name}.lib)\n"
         "# target_link_libraries(${lib_target_name} ores.c.core.lib)\n"
         "target_link_libraries(${lib_target_name}\n"
         "    PUBLIC\n"
         "        ores.a.api.lib\n"
         "        ores.b.core.lib # a trailing comment\n"
         "        ores.${other}.core.lib\n"
         "        ores.a.service.lib\n"
         "    PRIVATE\n"
         "        ores.b.service.lib\n"
         "        ores.zz.core.lib)\n"
         "target_link_libraries(ores.a.tests ores.b.core.lib)\n"},
        {"projects/ores.b/core/src/CMakeLists.txt",
         "set(name ores.b.core)\n"},
        {"projects/ores.b/service/src/CMakeLists.txt",
         "set(name ores.b.service)\n"},
        {"projects/ores.c/core/src/CMakeLists.txt",
         "add_library(x)\n"}};
}

}

TEST_CASE("a core that links another component's core or service is "
    "reported on the link", "[codecs][core_link_edges]") {
    const auto r = tests::run(codecs::core_link_edges, tree());
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.a/core/src/CMakeLists.txt", 7},
        {"projects/ores.a/core/src/CMakeLists.txt", 8},
        {"projects/ores.a/core/src/CMakeLists.txt", 11},
        {"projects/ores.c/core/src/CMakeLists.txt", 1}});
}
