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
#include "compass.docs/codecs/heading_numbers.hpp"

#include <algorithm>
#include <array>
#include <string>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/decoding.hpp"

namespace compass::docs::codecs {

namespace {

namespace ct = compass::codec::types;
namespace ot = compass::org::types;
namespace pt = compass::platform::text;

constexpr std::array<std::string_view, 1> numbered_heading_sources{
    "177EDB1C-6241-45AA-81B1-46C4B9D6329D"};

constexpr std::array<ct::rule, 2> rules{{
    {"numbered_heading", "No org heading carries its own number.",
        numbered_heading_sources},
    {"unreadable_document", "Every document under doc/ and projects/ is "
        "UTF-8, which the script's strict read needs."}}};

constexpr std::array<std::string_view, 6> skipped{"build", "external", "venv",
    "node_modules", ".git", ".cache"};

bool is_searched(std::string_view path) {
    if (!path.starts_with("doc/") && !path.starts_with("projects/"))
        return false;
    return std::ranges::none_of(compass::cpp::ops::components_of(path),
        [](auto part) {
            return std::ranges::find(skipped, part) != skipped.end();
        });
}

/**
 * @brief True when @p title starts with a number such as 2.1. or 3),
 * then a space and more text.
 */
bool is_numbered(std::string_view title) {
    std::size_t i = 0;
    for (;;) {
        const auto digits = i;
        while (i < title.size() && title[i] >= '0' && title[i] <= '9')
            ++i;
        if (i == digits)
            return false;
        if (i + 1 < title.size() && title[i] == '.' &&
            title[i + 1] >= '0' && title[i + 1] <= '9') {
            ++i;
            continue;
        }
        break;
    }
    if (i >= title.size() || (title[i] != '.' && title[i] != ')'))
        return false;
    const auto rest = title.substr(i + 1);
    return !rest.empty() && compass::cpp::ops::is_space(rest.front()) &&
        !compass::cpp::ops::strip(rest).empty();
}

void run(const ct::document& d, const ct::corpus_view&,
    ct::diagnostic_table& out) {
    if (!is_searched(d.path))
        return;
    if (!pt::is_utf8(d.source)) {
        out.add({d.row, {0, 0}, ct::rule_id{1}, ct::severity::error,
            "the document is not UTF-8, so its headings cannot be read",
            "Save the document as UTF-8."});
        return;
    }
    const auto& t = d.tables;
    for (std::uint32_t i = 0; i < t.headings.size(); ++i) {
        const ot::heading_id h{i};
        if (!is_numbered(t.headings.titles[h].of(d.source)))
            continue;
        const auto begin = t.elements.ranges[t.headings.elements[h]].begin;
        out.add({d.row, {begin, begin}, ct::rule_id{0},
            ct::severity::error, "the heading carries its own number",
            "Remove the number: the exporter numbers headings, and a "
            "written number goes stale when a section is inserted above. "
            "build/scripts/check_heading_numbers.py --write strips them."});
    }
}

}

const ct::descriptor heading_numbers{
    .name = "heading_numbers",
    .version = 2,
    .summary = "Reports each org heading under doc/ and projects/ that "
        "carries its own number.",
    .reads = ct::inputs::every_document(),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::document_function<ct::diagnostic_table>{run}};

}
