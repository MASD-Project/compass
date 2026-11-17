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
#include "compass.codec/codecs/required_sections.hpp"

#include <array>
#include <cstdint>
#include <format>
#include <string_view>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.org/ops/document_fields.hpp"
#include "compass.org/ops/type_lookup.hpp"

namespace compass::codec::codecs {

namespace {

namespace ot = compass::org::types;

constexpr types::rule_id missing_section{0};

constexpr std::array<std::string_view, 1> missing_section_sources{
    "8B29A8D8-E004-4E87-B81D-ABEAF68F2B98"};

constexpr std::array<types::rule, 1> rules{{
    {"missing_section",
        "A document has every section its type requires.",
        missing_section_sources}}};

bool has_section(const types::document& d, std::string_view title) {
    const auto& h = d.tables.headings;
    for (std::uint32_t i = 0; i < h.size(); ++i) {
        if (h.levels[ot::heading_id{i}] != 1)
            continue;
        const auto range = h.titles[ot::heading_id{i}];
        if (org::ops::trimmed(d.source, range).of(d.source) == title)
            return true;
    }
    return false;
}

void run(const types::document& d, const types::corpus_view& corpus,
    types::diagnostic_table& out) {
    const auto& registry = corpus.types();
    const auto type =
        org::ops::name_text(registry, registry.types.names[d.type]);
    const auto keyword = org::ops::find_keyword(d.tables, "type");
    const auto range = org::ops::trimmed(d.source,
        d.tables.keywords.values[keyword]);
    for (const auto n : org::ops::required_sections(registry, d.type).values) {
        const auto section = org::ops::name_text(registry, n);
        if (has_section(d, section))
            continue;
        const auto message = std::format(
            "{} document has no \"{}\" section", type, section);
        const auto fix = std::format(
            "Add the heading \"* {}\"; the {} contract requires it.",
            section, type);
        out.add({d.row, range, missing_section, types::severity::error,
            message, fix});
    }
}

}

const types::descriptor required_sections{
    .name = "required_sections",
    .version = 1,
    .summary = "Reports each required section a document lacks.",
    .reads = types::inputs::every_typed_document(),
    .options = {},
    .rules = rules,
    .enabled_by_default = true,
    .run = types::document_function<types::diagnostic_table>{run}};

}
