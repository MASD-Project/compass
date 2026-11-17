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
#include "compass.cpp/codecs/registrars.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::cpp;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

files tree() {
    return {
        {"projects/ores.dq/core/include/ores.dq/messaging/a_registrar.hpp",
         "void register_a_handlers(registrar& r);\n"},
        {"projects/ores.dq/core/include/ores.dq/messaging/b_registrar.hpp",
         "\nvoid register_b_handlers(registrar& r);\n"},
        {"projects/ores.dq/core/include/ores.dq/messaging/c_registrar.hpp",
         "void register_c_handlers(registrar& r);\n"},
        {"projects/ores.dq/core/include/ores.dq/messaging/"
         "d_history_provider_registrar.hpp",
         "void register_d_handlers(registrar& r);\n"},
        {"projects/ores.dq/core/src/messaging/b_registrar.cpp",
         "void register_b_handlers(registrar& r) {\n"
         "  r.on(get_b_request::nats_subject);\n}\n"},
        {"projects/ores.dq/core/src/messaging/c_registrar.cpp",
         "void register_c_handlers(registrar& r) {\n"
         "  r.on(get_c_request::nats_subject);\n}\n"},
        {"projects/ores.dq/core/src/messaging/registrar.cpp",
         "void all() { register_a_handlers(r); }\n"
         "void by_hand() { subscribe(get_b_request::nats_subject); }\n"},
        {"projects/ores.dq/core/tests/calls.cpp",
         "void t() { register_c_handlers(r); }\n"},
        {"projects/ores.dq/service/include/ores.dq/x_event_registrar.hpp",
         "void register_x_event_mapping(m& e);\n"},
        {"projects/ores.dq/service/include/ores.dq/y_event_registrar.hpp",
         "void register_y_event_mapping(m& e);\n"},
        {"projects/ores.dq/service/src/messaging/event_registrar.cpp",
         "void all() { register_x_event_mapping(e); }\n"},
        {"projects/ores.refdata/core/include/ores.refdata/messaging/"
         "z_history_provider_registrar.hpp",
         "void register_z_history_provider(r& h);\n"}};
}

}

TEST_CASE("a handler registrar nothing calls and nobody else answers is "
    "reported", "[codecs][handler_registrars]") {
    const auto r = tests::run(codecs::handler_registrars, tree());
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.dq/core/include/ores.dq/messaging/c_registrar.hpp",
         1}});
}

TEST_CASE("an event registrar the composition does not call is reported",
    "[codecs][event_registrars]") {
    const auto r = tests::run(codecs::event_registrars, tree());
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.dq/service/include/ores.dq/y_event_registrar.hpp",
         1}});
}

TEST_CASE("a component with history registrars and no composition file is "
    "reported once", "[codecs][history_registrars]") {
    const auto r = tests::run(codecs::history_registrars, tree());
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.refdata/core/include/ores.refdata/messaging/"
         "z_history_provider_registrar.hpp", 1}});
}

TEST_CASE("an edit that drops a registrar call reports the registrar, and "
    "one that adds it back clears it", "[codecs][handler_registrars]") {
    auto dropped = tree();
    dropped["projects/ores.dq/core/src/messaging/registrar.cpp"] =
        "void by_hand() { subscribe(get_b_request::nats_subject); }\n";

    CHECK(tests::places(tests::run_after_edit(codecs::handler_registrars,
        tree(), dropped).findings) == places{
            {"projects/ores.dq/core/include/ores.dq/messaging/a_registrar.hpp",
             1},
            {"projects/ores.dq/core/include/ores.dq/messaging/c_registrar.hpp",
             1}});
    CHECK(tests::run_after_edit(codecs::handler_registrars, dropped, tree())
        .findings.size() == 1);
}
