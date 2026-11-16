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
#include "compass.cpp/codecs/bff_list_requests.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::cpp;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

files tree() {
    return {
        {"projects/ores.books/api/include/ores.books/messaging/"
         "book_protocol.hpp",
         "struct book_filter {\n"
         "    std::optional<std::string> name;\n"
         "    std::optional<std::string> code;\n"
         "};\n"
         "struct list_book_request {\n"
         "    static constexpr std::string_view nats_subject = \"books.list\";\n"
         "    std::optional<book_filter> filter;\n"
         "    std::optional<std::string> as_of;\n"
         "    int offset = 0;\n"
         "};\n"
         "struct list_book_response {\n"
         "    std::vector<book> books;\n"
         "    int total = 0;\n"
         "};\n"},
        {"projects/ores.web/packages/bff/src/books.ts",
         "const a = send(list_book_request, { offset: 0, as_of: x,\n"
         "    filter: { name: 'a' } });\n"
         "const b = send(list_book_request, { offset: 0, filter });\n"
         "const c = send(list_book_request, { as_of: x, filter: { name: 'a', "
         "code: 'b', colour: 'c' } });\n"
         "const d = readAll(nats, subjects.list_book_request, 'books', "
         "{ offset: 0 });\n"
         "const e = readAll(nats, subjects.list_book_request, 'rows');\n"
         "const table = { books: list_book_request, other: 1 };\n"},
        {"projects/ores.web/packages/bff/src/books.test.ts",
         "send(list_book_request, { offset: 0 });\n"}};
}

}

TEST_CASE("a list request that disagrees with the protocol is reported",
    "[codecs][bff_list_requests]") {
    const auto r = tests::run(codecs::bff_list_requests, tree());
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.web/packages/bff/src/books.ts", 1},
        {"projects/ores.web/packages/bff/src/books.ts", 3},
        {"projects/ores.web/packages/bff/src/books.ts", 4},
        {"projects/ores.web/packages/bff/src/books.ts", 6},
        {"projects/ores.web/packages/wire-protocol/src/generated/"
         "list_requests.ts", 0}});
}
