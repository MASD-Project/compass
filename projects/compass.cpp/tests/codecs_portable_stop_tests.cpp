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
#include "compass.cpp/codecs/portable_stop.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::cpp;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

}

TEST_CASE("a stop type outside ores.platform is reported on its line",
    "[codecs][portable_stop]") {
    const files t{
        {"projects/ores.comms/src/worker.cpp",
         "// std::jthread in a comment is not code\n"
         "const char* s = \"std::stop_token\";\n"
         "std::jthread worker;\n"
         "#include <stop_token>\n"
         "auto r = R\"x(std::stop_source)x\";\n"
         "void f(std::stop_callback<int> c);\n"},
        {"projects/ores.comms/include/ores.comms/a.hxx",
         "  #  include <stop_token>\n"},
        {"projects/ores.platform/include/stoppable_thread.hpp",
         "std::jthread allowed;\n"},
        {"projects/ores.comms/build/gen.cpp", "std::jthread skipped;\n"},
        {"projects/ores.comms/src/notes.txt", "std::jthread\n"},
        {"other/x.cpp", "std::jthread outside;\n"}};
    const auto r = tests::run(codecs::portable_stop, t);
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.comms/src/worker.cpp", 3},
        {"projects/ores.comms/src/worker.cpp", 4},
        {"projects/ores.comms/src/worker.cpp", 6},
        {"projects/ores.comms/include/ores.comms/a.hxx", 1}});
}

TEST_CASE("an invalid byte is dropped, as errors=\"ignore\" drops it",
    "[codecs][portable_stop]") {
    const files t{{"projects/ores.a/src/a.cpp", "std::j\xfft" "hread w;\n"}};
    CHECK(tests::run(codecs::portable_stop, t).findings.size() == 1);
}
