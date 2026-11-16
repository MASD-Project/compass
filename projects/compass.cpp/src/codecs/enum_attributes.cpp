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
#include "compass.cpp/codecs/enum_attributes.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/enum_attributes.hpp"
#include "compass.cpp/ops/paths.hpp"

namespace compass::cpp::codecs {

namespace {

namespace ct = compass::codec::types;

constexpr std::array<ct::rule, 1> rules{{
    {"visibility_macro_on_enum",
        "No symbol visibility or DLL storage macro decorates an enum."}}};

constexpr std::array<std::string_view, 5> suffixes{
    ".hpp", ".h", ".cpp", ".ipp", ".cc"};

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    std::vector<std::string_view> paths;
    for (const auto suffix : suffixes) {
        for (const auto p : v.list_files("projects/", suffix)) {
            if (ops::suffix_of(p) == suffix)
                paths.push_back(p);
        }
    }
    std::ranges::sort(paths, ops::component_less);

    std::vector<std::string> erased;
    erased.reserve(paths.size());
    std::set<std::string, std::less<>> macros{"BOOST_SYMBOL_EXPORT",
        "BOOST_SYMBOL_IMPORT"};
    for (const auto p : paths) {
        erased.push_back(ops::erase_comments_and_literals(
            v.read_file(p).value_or("")));
        ops::add_visibility_macros(erased.back(), macros);
    }

    for (std::size_t f = 0; f < paths.size(); ++f) {
        for (const auto& site : ops::enum_sites(erased[f], macros)) {
            if (site.macro.empty())
                continue;
            out.add({.document = compass::org::types::document_id::none(),
                .range = {site.offset, site.offset},
                .rule = ct::rule_id{0},
                .level = ct::severity::error,
                .message = "the visibility macro " + site.macro +
                    " decorates an enum, and the attribute does not apply "
                    "to one: " + site.source,
                .fix = "drop the macro; an enum needs no export attribute "
                    "to cross a binary boundary",
                .file = paths[f]});
        }
    }
}

}

const ct::descriptor enum_attributes{
    .name = "enum_attributes",
    .version = 1,
    .summary = "Reports each enum declaration that a visibility macro "
        "decorates.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}
