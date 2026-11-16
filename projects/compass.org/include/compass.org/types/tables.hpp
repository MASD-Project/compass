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
#ifndef COMPASS_ORG_TYPES_TABLES_HPP
#define COMPASS_ORG_TYPES_TABLES_HPP

#include <cstddef>
#include <span>
#include <vector>
#include "compass.org/types/affiliated_table.hpp"
#include "compass.org/types/block_table.hpp"
#include "compass.org/types/column.hpp"
#include "compass.org/types/document_table.hpp"
#include "compass.org/types/element_table.hpp"
#include "compass.org/types/heading_table.hpp"
#include "compass.org/types/keyword_table.hpp"
#include "compass.org/types/link_table.hpp"
#include "compass.org/types/list_table.hpp"
#include "compass.org/types/name_table.hpp"
#include "compass.org/types/object_table.hpp"
#include "compass.org/types/property_table.hpp"

namespace compass::org::types {

/**
 * @brief Everything org documents become: one document, or a corpus of them
 * end to end.
 *
 * The source text is not here. Every source_range is read against the bytes
 * of its document, which the caller keeps mapped. documents is empty for a
 * single document and has one row per document for a corpus.
 *
 * tables owns its columns and is what a parser fills. tables_view has the
 * same members over spans and is what readers take, so a reader works the
 * same over a fresh parse and over a mapped object. name_bytes is the only
 * member that is not a table: the bytes of every interned name, end to end.
 */
template <template <typename, column_value> class Column>
struct basic_tables {
    basic_element_table<Column> elements;
    basic_object_table<Column> objects;
    basic_heading_table<Column> headings;
    basic_heading_tag_table<Column> heading_tags;
    basic_keyword_table<Column> keywords;
    basic_property_table<Column> properties;
    basic_affiliated_table<Column> affiliated;
    basic_block_table<Column> blocks;
    basic_list_table<Column> lists;
    basic_item_table<Column> items;
    basic_link_table<Column> links;
    basic_name_table<Column> names;
    decltype(Column<name_id, char>::values) name_bytes;
    basic_document_table<Column> documents;
};

using tables = basic_tables<column>;
using tables_view = basic_tables<column_view>;

/**
 * @brief Calls f once per table, in the order tables lists them.
 *
 * Given several table sets, f gets the same table of each. name_bytes is
 * not a table and is not visited.
 */
template <typename F, typename... Tables>
constexpr void for_each_table(F&& f, Tables&&... t) {
    f(t.elements...);
    f(t.objects...);
    f(t.headings...);
    f(t.heading_tags...);
    f(t.keywords...);
    f(t.properties...);
    f(t.affiliated...);
    f(t.blocks...);
    f(t.lists...);
    f(t.items...);
    f(t.links...);
    f(t.names...);
    f(t.documents...);
}

inline constexpr std::size_t table_count = [] {
    std::size_t n = 0;
    for_each_table([&](const auto&) { ++n; }, tables_view{});
    return n;
}();

inline constexpr std::size_t column_count = [] {
    std::size_t n = 0;
    for_each_table([&](const auto& table) {
        for_each_column([&](const auto&) { ++n; }, table);
    }, tables_view{});
    return n;
}();

static_assert(sizeof(tables_view) ==
    (column_count + 1) * sizeof(std::span<const char>),
    "list every table in for_each_table and every column in the "
    "for_each_column of its table");

/**
 * @brief The view of tables that own their columns.
 *
 * It points into @p t and is stale once a column of @p t grows.
 */
[[nodiscard]] inline tables_view view(const tables& t) noexcept {
    tables_view v;
    for_each_table([](auto& to, const auto& from) {
        for_each_column([](auto& column, const auto& owned) {
            column.values = owned.values;
        }, to, from);
    }, v, t);
    v.name_bytes = t.name_bytes;
    return v;
}

}

#endif
