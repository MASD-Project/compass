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
#include "compass.org/types/column.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <catch2/catch_test_macros.hpp>
#include "compass.org/types/tables.hpp"

namespace {

using namespace compass::org::types;

struct wide_table {
    column<keyword_id, std::uint8_t> flags;
    column<keyword_id, std::uint64_t> hashes;
};

static_assert(row_bytes(&wide_table::flags, &wide_table::hashes) == 9);

template <typename... Columns>
concept row_bytes_accepts = requires(Columns... c) { row_bytes(c...); };

struct mixed_table {
    column<keyword_id, std::uint8_t> flags;
    column<block_id, std::uint8_t> kinds;
};

static_assert(row_bytes_accepts<decltype(&wide_table::flags),
    decltype(&wide_table::hashes)>);
static_assert(!row_bytes_accepts<decltype(&mixed_table::flags),
    decltype(&mixed_table::kinds)>);

static_assert(element_row_bytes == 30);
static_assert(object_row_bytes == 22);
static_assert(heading_row_bytes == 29);
static_assert(heading_tag_row_bytes == 4);
static_assert(keyword_row_bytes == 16);
static_assert(property_row_bytes == 16);
static_assert(affiliated_row_bytes == 28);
static_assert(block_row_bytes == 24);
static_assert(list_row_bytes == 5);
static_assert(item_row_bytes == 13);
static_assert(link_row_bytes == 17);
static_assert(name_row_bytes == 4);
static_assert(document_row_bytes == 60);

constexpr std::string_view first_source = "#+title: One\n#+type: task\n";
constexpr std::string_view second_source = "#+title: Two\n";

/**
 * The keyword rows of two documents end to end, as a corpus holds them.
 */
tables two_documents() {
    tables t;
    t.documents.source_firsts.values = {0, first_source.size()};
    t.documents.keyword_firsts.values = {0, 2};
    t.keywords.elements.values = {element_id{1}, element_id{2}, element_id{1}};
    t.keywords.names.values = {name_id{0}, name_id{1}, name_id{0}};
    t.keywords.values.values = {{9, 12}, {21, 25}, {9, 12}};
    return t;
}

std::size_t keyword_count(const tables& t, document_id d) {
    const auto first = t.documents.keyword_firsts[d];
    const auto end = d.next().value < t.documents.size()
        ? t.documents.keyword_firsts[d.next()]
        : static_cast<std::uint32_t>(t.keywords.size());
    return end - first;
}

}

TEST_CASE("a slice indexes one document's rows from zero",
    "[types][column]") {
    const auto t = two_documents();
    const document_id second{1};
    const auto first_row = t.documents.keyword_firsts[second];
    const auto values =
        t.keywords.values.slice(first_row, keyword_count(t, second));

    REQUIRE(values.size() == 1);
    CHECK(values[keyword_id{0}].of(second_source) == "Two");
}

TEST_CASE("the same local handle names a different row in each document",
    "[types][column]") {
    const auto t = two_documents();
    const keyword_id title{0};

    const auto first = t.keywords.values.slice(
        t.documents.keyword_firsts[document_id{0}],
        keyword_count(t, document_id{0}));
    const auto second = t.keywords.values.slice(
        t.documents.keyword_firsts[document_id{1}],
        keyword_count(t, document_id{1}));

    REQUIRE(first.size() == 2);
    CHECK(first[title].of(first_source) == "One");
    CHECK(first[keyword_id{1}].of(first_source) == "task");
    CHECK(second[title].of(second_source) == "Two");
}

TEST_CASE("an empty source range stands for absent text", "[types][column]") {
    const source_range absent;
    CHECK(absent.empty());
    CHECK(absent.of(first_source).empty());
    CHECK(source_range{9, 12}.size() == 3);
}
