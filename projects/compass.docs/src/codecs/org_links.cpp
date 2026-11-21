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
#include "compass.docs/codecs/org_links.hpp"

#include <array>
#include <string>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::docs::codecs {

namespace {

namespace ct = compass::codec::types;
namespace ot = compass::org::types;
namespace pt = compass::platform::text;

constexpr std::array<ct::rule, 1> rules{{
    {"colon_link", "No link's raw target begins with a colon, which no "
        "link type does.", {}, ct::blame_method::line}}};

/**
 * @brief True for a document the site build publishes: no .packages,
 * vcpkg, build or tmp directory and no .claude/worktrees on its path.
 */
bool is_published(std::string_view path) {
    static const pt::regex excluded(
        R"((^|/)(\.packages|vcpkg|build|tmp|\.claude/worktrees)/)");
    return !excluded.search(path);
}

void run(const ct::document& d, const ct::corpus_view&,
    ct::diagnostic_table& out) {
    if (!is_published(d.path))
        return;
    const auto& links = d.tables.links;
    for (std::uint32_t i = 0; i < links.size(); ++i) {
        const ot::link_id l{i};
        if (links.formats[l] != ot::link_format::bracket)
            continue;
        const auto range = d.tables.objects.ranges[links.objects[l]];
        const auto raw = d.source.substr(range.begin + 2);
        if (!raw.starts_with(':'))
            continue;
        const auto target = raw.substr(0, raw.find(']'));
        out.add({d.row, range, ct::rule_id{0}, ct::severity::error,
            "link org cannot resolve: [[" + std::string(target) + "]]",
            "A link's target never begins with a colon: this one is usually "
            "a verbatim span closed early, as in =grep '[[:space:]]*='; "
            "quote the text in a src block or with ~...~."});
    }
}

}

const ct::descriptor org_links{
    .name = "org_links",
    .version = 1,
    .summary = "Reports each link whose raw target begins with a colon.",
    .reads = ct::inputs::every_document(),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::document_function<ct::diagnostic_table>{run}};

}
