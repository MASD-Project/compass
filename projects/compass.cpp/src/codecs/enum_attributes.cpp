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
#include <span>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codec/types/fact.hpp"
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

std::vector<std::string_view> scanned(const ct::corpus_view& v) {
    std::vector<std::string_view> paths;
    for (const auto suffix : suffixes) {
        for (const auto p : v.list_files("projects/", suffix)) {
            if (ops::suffix_of(p) == suffix)
                paths.push_back(p);
        }
    }
    std::ranges::sort(paths, ops::component_less);
    return paths;
}

/**
 * @brief The fact of a file: the visibility macros it defines, then each
 * enum that one of them, or any other file's, would decorate.
 */
void map(std::string_view, std::string_view bytes, const ct::option_values&,
    ct::file_scan& out) {
    const auto erased = ops::erase_comments_and_literals(bytes);
    std::set<std::string, std::less<>> macros;
    ops::add_visibility_macros(erased, macros);
    const auto sites = ops::macro_enum_sites(erased);
    ct::fact_writer fact(out.fact);
    fact.number(static_cast<std::uint32_t>(macros.size()));
    for (const auto& m : macros)
        fact.text(m);
    for (const auto& site : sites)
        fact.number(site.offset).text(site.macro).text(site.source);
}

void reduce(std::span<const ct::scanned_file> files, const ct::corpus_view&,
    ct::diagnostic_table& out) {
    std::set<std::string, std::less<>> macros{"BOOST_SYMBOL_EXPORT",
        "BOOST_SYMBOL_IMPORT"};
    for (const auto& f : files) {
        ct::fact_reader fact(f.fact);
        for (auto n = fact.number(); n > 0; --n)
            macros.emplace(fact.text());
    }
    for (const auto& f : files) {
        ct::fact_reader fact(f.fact);
        for (auto n = fact.number(); n > 0; --n)
            (void)fact.text();
        while (!fact.done()) {
            const auto offset = fact.number();
            const auto macro = fact.text();
            const auto source = fact.text();
            if (!macros.contains(macro))
                continue;
            out.add({.document = compass::org::types::document_id::none(),
                .range = {offset, offset},
                .rule = ct::rule_id{0},
                .level = ct::severity::error,
                .message = "the visibility macro " + std::string(macro) +
                    " decorates an enum, and the attribute does not apply "
                    "to one: " + std::string(source),
                .fix = "drop the macro; an enum needs no export attribute "
                    "to cross a binary boundary",
                .file = f.path});
        }
    }
}

}

const ct::descriptor enum_attributes{
    .name = "enum_attributes",
    .version = 2,
    .summary = "Reports each enum declaration that a visibility macro "
        "decorates.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::scan_function{scanned, map, reduce}};

}
