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
#include "compass.ingest/org/parser.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.org/io/dump.hpp"
#include "compass.org/io/object.hpp"
#include "dump_of.hpp"

namespace {

namespace io = compass::org::io;
namespace types = compass::org::types;

/**
 * One of each construct that has a detail table, and nested objects.
 */
constexpr std::string_view reference =
    ":PROPERTIES:\n"
    ":ID: 0A1B\n"
    ":END:\n"
    "#+title: Reference\n"
    "#+todo: OPEN | SHUT\n"
    "\n"
    "* OPEN [#A] Heading /one/ :alpha:beta:\n"
    ":PROPERTIES:\n"
    ":Owner: marco\n"
    ":END:\n"
    "Text with *bold /and italic/* and [[id:0A1B][a link]] and\n"
    "https://example.org plain.\n"
    "\n"
    "#+caption: A *table*\n"
    "#+name: numbers\n"
    "| a | b |\n"
    "|---+---|\n"
    "| 1 | 2 |\n"
    "\n"
    "** SHUT Child\n"
    "- [X] done item\n"
    "- [ ] open item\n"
    "  1. nested\n"
    "- tag :: description\n"
    "\n"
    "#+begin_src cpp :tangle no\n"
    "int x;\n"
    "#+end_src\n"
    "\n"
    "#+begin_quote\n"
    "Quoted =verbatim= text.\n"
    "#+end_quote\n";

constexpr io::object_key key{.blob_id = {}, .parser_version =
    compass::ingest::org::parser_version};

std::vector<std::byte> object_of(std::string_view source) {
    types::tables tables;
    compass::ingest::org::parse(source, tables);
    std::vector<std::byte> bytes;
    io::encode(types::view(tables), key, bytes);
    return bytes;
}

}

TEST_CASE("the object of a parsed document dumps as its parse does",
    "[org][stored]") {
    const auto bytes = object_of(reference);

    const auto loaded = io::decode(bytes, key);

    REQUIRE(loaded.has_value());
    REQUIRE(io::checksum_matches(bytes));
    std::ostringstream os;
    io::dump(os, reference, *loaded);
    CHECK(os.str() == compass::ingest::tests::dump_of(reference));
    CHECK(loaded->headings.size() == 2);
    CHECK(loaded->links.size() == 2);
    CHECK(loaded->blocks.size() == 2);
    CHECK(loaded->items.size() == 4);
}

TEST_CASE("the object of an empty document round trips", "[org][stored]") {
    const auto bytes = object_of("");

    const auto loaded = io::decode(bytes, key);

    REQUIRE(loaded.has_value());
    std::ostringstream os;
    io::dump(os, "", *loaded);
    CHECK(os.str() == compass::ingest::tests::dump_of(""));
}

TEST_CASE("the stored form of the reference document is the pinned one",
    "[org][stored]") {
    const auto bytes = object_of(reference);
    io::object_header header;
    std::memcpy(&header, bytes.data(), sizeof(header));

    INFO("The bytes stored for the same source changed. Objects already in "
         "a store were written by the old code and would be read as if the "
         "new code had written them. If the parser now fills the tables "
         "differently, bump parser_version in parser.hpp. If the layout "
         "changed, bump object_format_version in object.hpp. Then pin the "
         "new checksum, size and versions here.");
    CHECK(compass::ingest::org::parser_version == 1);
    CHECK(io::object_format_version == 1);
    CHECK(bytes.size() == 2302);
    CHECK(header.checksum == 0x2A7DACEA740DD6D5);
}
