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
#include "compass.org/io/object.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.org/io/dump.hpp"

namespace {

using namespace compass::org::types;
namespace io = compass::org::io;

constexpr std::string_view source = "#+title: T\n* TODO H  :a:b:\n[[id:x]]\n";

constexpr io::object_key key{
    .blob_id = {0xDE, 0xAD, 0xBE, 0xEF, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
        13, 14, 15, 16},
    .parser_version = 7};

name_id name(tables& t, std::string_view text) {
    t.names.firsts.values.push_back(
        static_cast<std::uint32_t>(t.name_bytes.size()));
    t.name_bytes.insert(t.name_bytes.end(), text.begin(), text.end());
    return {static_cast<std::uint32_t>(t.names.size() - 1)};
}

void element(tables& t, element_kind kind, source_range range,
    source_range contents, std::uint32_t parent, std::uint32_t subtree_end) {
    auto& e = t.elements;
    e.kinds.values.push_back(kind);
    e.post_blanks.values.push_back(0);
    e.ranges.values.push_back(range);
    e.contents.values.push_back(contents);
    e.parents.values.push_back({parent});
    e.subtree_ends.values.push_back({subtree_end});
    e.object_firsts.values.push_back(
        {static_cast<std::uint32_t>(t.objects.size())});
}

/**
 * The tables of source: a keyword, a heading with a TODO keyword and two
 * tags, and a paragraph that holds one link.
 */
tables document() {
    tables t;
    element(t, element_kind::document, {0, 36}, {0, 36},
        element_id::none_value, 6);
    element(t, element_kind::section, {0, 11}, {0, 11}, 0, 3);
    element(t, element_kind::keyword, {0, 11}, {}, 1, 3);
    t.keywords.elements.values.push_back({2});
    t.keywords.names.values.push_back(name(t, "title"));
    t.keywords.values.values.push_back({9, 10});
    element(t, element_kind::heading, {11, 36}, {27, 36}, 0, 6);
    t.headings.elements.values.push_back({3});
    t.headings.levels.values.push_back(1);
    t.headings.todos.values.push_back(name(t, "TODO"));
    t.headings.todo_types.values.push_back(todo_type::todo);
    t.headings.priorities.values.push_back(0);
    t.headings.comments.values.push_back(comment_state::live);
    t.headings.titles.values.push_back({18, 19});
    t.headings.tag_firsts.values.push_back({0});
    t.headings.property_firsts.values.push_back({0});
    t.heading_tags.names.values.push_back(name(t, "a"));
    t.heading_tags.names.values.push_back(name(t, "b"));
    element(t, element_kind::section, {27, 36}, {27, 36}, 3, 6);
    element(t, element_kind::paragraph, {27, 36}, {27, 36}, 4, 6);
    t.objects.kinds.values.push_back(object_kind::link);
    t.objects.post_blanks.values.push_back(0);
    t.objects.ranges.values.push_back({27, 35});
    t.objects.contents.values.push_back({});
    t.objects.subtree_ends.values.push_back({1});
    t.links.objects.values.push_back({0});
    t.links.types.values.push_back(name(t, "id"));
    t.links.formats.values.push_back(link_format::bracket);
    t.links.paths.values.push_back({32, 33});
    return t;
}

std::string dump_of(const tables_view& t) {
    std::ostringstream os;
    io::dump(os, source, t);
    return os.str();
}

std::vector<std::byte> encoded() {
    std::vector<std::byte> bytes;
    io::encode(view(document()), key, bytes);
    return bytes;
}

io::object_header header_of(const std::vector<std::byte>& bytes) {
    io::object_header h;
    std::memcpy(&h, bytes.data(), sizeof(h));
    return h;
}

/**
 * The object with its header replaced, as a writer of another build or a
 * damaged disk could leave it.
 */
template <typename Change>
std::vector<std::byte> with_header(Change change) {
    auto bytes = encoded();
    auto h = header_of(bytes);
    change(h);
    std::memcpy(bytes.data(), &h, sizeof(h));
    return bytes;
}

}

TEST_CASE("a decoded object dumps as the tables it was encoded from",
    "[io][object]") {
    const auto t = document();
    const auto bytes = encoded();

    const auto loaded = io::decode(bytes, key);

    REQUIRE(loaded.has_value());
    CHECK(dump_of(*loaded) == dump_of(view(t)));
    CHECK(dump_of(*loaded) ==
        "org-data 0 36 0 36 0\n"
        "  section 0 11 0 11 0\n"
        "    keyword 0 11 - - 0 key=title value=T\n"
        "  headline 11 36 27 36 0 level=1 todo=TODO todo-type=todo "
        "priority=- commented=0 tags=a:b title=H\n"
        "    section 27 36 27 36 0\n"
        "      paragraph 27 36 27 36 0\n"
        "        link 27 35 - - 0 type=id format=bracket path=x\n");
    CHECK(io::checksum_matches(bytes));
}

TEST_CASE("a decoded object holds every column byte for byte",
    "[io][object]") {
    tables t;
    std::size_t rows = 0;
    std::uint8_t fill = 0;
    for_each_table([&](auto& table) {
        ++rows;
        for_each_column([&](auto& column) {
            column.values.resize(rows);
            std::memset(column.values.data(), ++fill,
                column.values.size() * sizeof(column.values[0]));
        }, table);
    }, t);
    t.name_bytes = {'x', 'y', 'z'};
    std::vector<std::byte> bytes;
    io::encode(view(t), key, bytes);

    const auto loaded = io::decode(bytes, key);

    REQUIRE(loaded.has_value());
    std::size_t columns = 0;
    for_each_table([&](const auto& table, const auto& original) {
        for_each_column([&](const auto& column, const auto& expected) {
            REQUIRE(column.size() == expected.size());
            CHECK(std::memcmp(column.values.data(), expected.values.data(),
                column.size() * sizeof(expected.values[0])) == 0);
            CHECK(reinterpret_cast<std::uintptr_t>(column.values.data()) %
                io::object_alignment == 0);
            ++columns;
        }, table, original);
    }, *loaded, t);
    CHECK(columns == column_count);
    CHECK(std::string_view(loaded->name_bytes.data(),
        loaded->name_bytes.size()) == "xyz");
}

TEST_CASE("the same tables and key always encode to the same bytes",
    "[io][object]") {
    CHECK(encoded() == encoded());
}

TEST_CASE("an object of another format version is rejected", "[io][object]") {
    const auto bytes = with_header([](io::object_header& h) {
        h.format_version = io::object_format_version + 1;
    });

    const auto loaded = io::decode(bytes, key);

    REQUIRE(!loaded.has_value());
    CHECK(loaded.error() == io::object_error::format_version_mismatch);
}

TEST_CASE("an object of another parser version is rejected", "[io][object]") {
    auto newer = key;
    ++newer.parser_version;

    const auto loaded = io::decode(encoded(), newer);

    REQUIRE(!loaded.has_value());
    CHECK(loaded.error() == io::object_error::parser_version_mismatch);
}

TEST_CASE("an object of another blob is rejected", "[io][object]") {
    auto other = key;
    other.blob_id[19] ^= 1;

    const auto loaded = io::decode(encoded(), other);

    REQUIRE(!loaded.has_value());
    CHECK(loaded.error() == io::object_error::blob_id_mismatch);
}

TEST_CASE("bytes that are not an object are rejected", "[io][object]") {
    const auto wrong_magic = with_header(
        [](io::object_header& h) { h.magic[0] = 'X'; });
    const auto swapped = with_header(
        [](io::object_header& h) { h.byte_order = 0x04030201; });

    CHECK(io::decode(wrong_magic, key).error() ==
        io::object_error::bad_magic);
    CHECK(io::decode(swapped, key).error() ==
        io::object_error::foreign_byte_order);
}

TEST_CASE("an object cut short at any length is rejected", "[io][object]") {
    const auto bytes = encoded();

    for (std::size_t size = 0; size < bytes.size(); ++size) {
        const auto loaded =
            io::decode(std::span(bytes).first(size), key);
        REQUIRE(!loaded.has_value());
        CHECK(loaded.error() == (size < sizeof(io::object_header)
            ? io::object_error::truncated
            : io::object_error::size_mismatch));
    }
}

TEST_CASE("a column that starts past the end is rejected", "[io][object]") {
    for (std::size_t column = 0; column < column_count; ++column) {
        const auto bytes = with_header([&](io::object_header& h) {
            h.offsets[column] = (h.size / io::object_alignment + 1) *
                io::object_alignment;
        });

        const auto loaded = io::decode(bytes, key);

        REQUIRE(!loaded.has_value());
        CHECK(loaded.error() == io::object_error::column_out_of_bounds);
    }
}

TEST_CASE("a column that runs past the end is rejected", "[io][object]") {
    for (std::size_t table = 0; table < table_count; ++table) {
        const auto bytes = with_header([&](io::object_header& h) {
            h.rows[table] = h.size;
        });

        const auto loaded = io::decode(bytes, key);

        REQUIRE(!loaded.has_value());
        CHECK(loaded.error() == io::object_error::column_out_of_bounds);
    }
}

TEST_CASE("a row count too large to multiply is rejected", "[io][object]") {
    const auto bytes = with_header([](io::object_header& h) {
        h.rows[0] = 0xFFFF'FFFF'FFFF'FFFF;
    });

    CHECK(io::decode(bytes, key).error() ==
        io::object_error::column_out_of_bounds);
}

TEST_CASE("a column that starts inside the header is rejected",
    "[io][object]") {
    const auto bytes = with_header([](io::object_header& h) {
        h.offsets[0] = 0;
    });

    CHECK(io::decode(bytes, key).error() ==
        io::object_error::column_out_of_bounds);
}

TEST_CASE("a column that starts off the alignment is rejected",
    "[io][object]") {
    const auto bytes = with_header([](io::object_header& h) {
        h.offsets[2] += 4;
    });

    CHECK(io::decode(bytes, key).error() ==
        io::object_error::column_misaligned);
}

TEST_CASE("name bytes that run past the end are rejected", "[io][object]") {
    const auto bytes = with_header([](io::object_header& h) {
        h.name_bytes_size = h.size;
    });

    CHECK(io::decode(bytes, key).error() ==
        io::object_error::column_out_of_bounds);
}

TEST_CASE("a buffer that starts off the alignment is rejected",
    "[io][object]") {
    const auto bytes = encoded();
    std::vector<std::byte> shifted(bytes.size() + 1);
    std::memcpy(shifted.data() + 1, bytes.data(), bytes.size());

    CHECK(io::decode(std::span(shifted).subspan(1), key).error() ==
        io::object_error::misaligned_buffer);
}

TEST_CASE("the checksum finds a changed byte anywhere after it",
    "[io][object]") {
    const auto bytes = encoded();
    const auto first = offsetof(io::object_header, checksum) +
        sizeof(io::object_header::checksum);

    for (std::size_t i = first; i < bytes.size(); ++i) {
        auto damaged = bytes;
        damaged[i] ^= std::byte{0x80};
        CHECK_FALSE(io::checksum_matches(damaged));
    }
}
