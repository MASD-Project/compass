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
#include "compass.crosscheck/codecs/open_reads.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using tests::files;

}

TEST_CASE("the allow-list is the parsed table of its section",
    "[codecs][open_reads]") {
    const auto request = [](std::string_view name, std::string_view subject) {
        return "struct " + std::string(name) + "_request {\n"
            "    static constexpr std::string_view nats_subject = \"" +
            std::string(subject) + "\";\n};\n";
    };
    const auto read = [](std::string_view method, std::string_view name) {
        return "    void " + std::string(method) +
            "(ores::nats::message msg) {\n        decode<" +
            std::string(name) + "_request>(msg);\n    }\n";
    };
    const files t{
        {"doc/knowledge/security/authorised-reads.org",
         "* Detail\n** The contract\n| =refdata.v1.books.get= | other |\n"
         "** The allow-list\n| Subject | Why |\n|---+---|\n"
         "| =refdata.v1.currencies.list= | why |\n"
         "| =refdata.v1.parties.get= and =refdata.v1.books.get= | guarded |\n"
         "*** Self reads\n|=refdata.v1.ops.get_my_desk=|self|\n"},
        {"projects/ores.refdata/api/include/ores.refdata.api/messaging/"
         "refdata_protocol.hpp",
         request("currencies", "refdata.v1.currencies.list") +
         request("parties", "refdata.v1.parties.get") +
         request("books", "refdata.v1.books.get") +
         request("desk", "refdata.v1.ops.get_my_desk")},
        {"projects/ores.refdata/core/include/ores.refdata.core/messaging/"
         "refdata_handler.hpp",
         "// Template: cpp_nats_handler.hpp.mustache\n" +
         read("list_currencies", "currencies") +
         read("get_parties", "parties") + read("get_my_desk", "desk")}};
    CHECK(tests::messages_of(tests::run(codecs::open_reads, t)) ==
        std::vector<std::string>{
            "get_parties serves refdata.v1.parties.get with no permission "
            "check, and Authorised reads does not allow-list it"});
}

TEST_CASE("an open read that is not allow-listed is reported",
    "[codecs][open_reads]") {
    const files t{
        {"doc/knowledge/security/authorised-reads.org",
         "* Reads\n** The allow-list\n| =refdata.v1.currencies.list= | why |\n"
         "| =refdata.v1.gone.get= | stale |\n** Next\n"
         "| =refdata.v1.parties.get= | not in the section |\n"},
        {"projects/ores.refdata/api/include/ores.refdata.api/messaging/"
         "currency_protocol.hpp",
         "struct get_currencies_request {\n"
         "    static constexpr std::string_view nats_subject = "
         "\"refdata.v1.currencies.list\";\n};\n"
         "struct get_parties_request {\n"
         "    static constexpr std::string_view nats_subject = "
         "\"refdata.v1.parties.get\";\n};\n"},
        {"projects/ores.refdata/core/include/ores.refdata.core/messaging/"
         "currency_handler.hpp",
         "// Template: cpp_nats_handler.hpp.mustache\n"
         "    /**\n     * @brief Serves refdata.v1.currencies.list.\n     */\n"
         "    void list_currencies(ores::nats::message msg) {\n"
         "        decode<get_currencies_request>(msg);\n    }\n"
         "    void fetch(ores::nats::message msg) {\n"
         "        decode<get_parties_request>(msg);\n    }\n"
         "    void get_guarded(ores::nats::message msg) {\n"
         "        if (!has_permission(msg)) return;\n    }\n"
         "    void get_unnamed(ores::nats::message msg) {\n    }\n"
         "    void put_thing(ores::nats::message msg) {\n    }\n"},
        {"projects/ores.refdata/core/src/messaging/"
         "refdata_history_provider_registrar.cpp",
         "// Template: cpp_history_provider_registrar.cpp.mustache\n"
         "register_history_provider(\"a\", \"refdata::read\", f);\n"
         "register_history_provider(\"b\", \"\", f);\n"
         "register_history_provider(name, code, f);\n"}};
    CHECK(tests::messages_of(tests::run(codecs::open_reads, t)) ==
        std::vector<std::string>{
            "fetch serves refdata.v1.parties.get with no permission check, "
            "and Authorised reads does not allow-list it",
            "get_unnamed serves an unnamed subject with no permission check, "
            "and Authorised reads does not allow-list it",
            "registers 3 history provider(s) but 2 name a literal read code",
            "registers a history with no read code",
            "allow-lists refdata.v1.gone.get, which no protocol header "
            "defines"});
}
