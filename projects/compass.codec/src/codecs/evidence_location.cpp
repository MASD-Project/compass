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
#include "compass.codec/codecs/evidence_location.hpp"

#include <array>
#include <string_view>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.org/ops/document_fields.hpp"

namespace compass::codec::codecs {

namespace {

constexpr types::rule_id outside_evidence_directory{0};

constexpr std::array<std::string_view, 1> sources{
    "8B29A8D8-E004-4E87-B81D-ABEAF68F2B98"};

constexpr std::array<types::rule, 1> rules{{
    {"outside_evidence_directory",
        "A document tagged evidence sits under an evidence/ directory.",
        sources, types::blame_method::line}}};

bool under_evidence(std::string_view path) {
    return path.starts_with("evidence/") || path.contains("/evidence/");
}

void run(const types::document& d, const types::corpus_view&,
    types::diagnostic_table& out) {
    if (!org::ops::is_evidence(d.source, d.tables) || under_evidence(d.path))
        return;
    const auto keyword = org::ops::find_keyword(d.tables, "filetags");
    out.add({d.row, org::ops::trimmed(d.source,
        d.tables.keywords.values[keyword]), outside_evidence_directory,
        types::severity::error, "evidence document outside an evidence/ "
        "directory", "Move the page under an evidence/ directory, or drop "
        "the evidence tag if it is a live page."});
}

}

const types::descriptor evidence_location{
    .name = "evidence_location",
    .version = 1,
    .summary = "Reports each evidence document outside an evidence/ "
        "directory.",
    .reads = types::inputs::every_document().with_evidence(),
    .options = {},
    .rules = rules,
    .enabled_by_default = true,
    .run = types::document_function<types::diagnostic_table>{run}};

}
