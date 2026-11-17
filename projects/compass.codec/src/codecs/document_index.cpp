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
#include "compass.codec/codecs/document_index.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.org/ops/document_fields.hpp"

namespace compass::codec::codecs {

namespace {

namespace ot = compass::org::types;

constexpr std::string_view path_option = "path";

constexpr std::array<types::option, 1> options{{
    {path_option, "doc/document_index.tsv",
        "Where the index goes, relative to the root of the checkout."}}};

void add_field(std::string& out, std::string_view text, char end) {
    for (const char c : text)
        out.push_back(c == '\t' ? ' ' : c);
    out.push_back(end);
}

std::string_view keyword(const types::document& d, std::string_view key) {
    const auto k = org::ops::find_keyword(d.tables, key);
    if (k.is_none())
        return {};
    return org::ops::trimmed(d.source, d.tables.keywords.values[k]).of(
        d.source);
}

std::string_view id_of(const types::document& d) {
    const auto p = org::ops::find_document_property(d.tables, "ID");
    if (p.is_none())
        return {};
    return org::ops::trimmed(d.source, d.tables.properties.values[p]).of(
        d.source);
}

void run(const types::corpus_view& corpus, types::file_output& out) {
    std::vector<std::size_t> order(corpus.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::ranges::sort(order, {},
        [&](std::size_t i) { return corpus[i].path; });

    std::string text = "id\ttitle\ttype\tpath\n";
    for (const auto i : order) {
        const auto d = corpus[i];
        add_field(text, id_of(d), '\t');
        add_field(text, keyword(d, "title"), '\t');
        add_field(text, keyword(d, "type"), '\t');
        add_field(text, d.path, '\n');
    }
    out.files.add(corpus.option(path_option), text);
}

}

const types::descriptor document_index{
    .name = "document_index",
    .version = 1,
    .summary = "Lists the ID, title, type and path of every document.",
    .reads = types::inputs::every_document(),
    .options = options,
    .rules = {},
    .enabled_by_default = true,
    .run = types::corpus_function<types::file_output>{run}};

}
