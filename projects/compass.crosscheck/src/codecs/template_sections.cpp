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
#include "compass.crosscheck/codecs/template_sections.hpp"

#include <array>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

constexpr std::array<ct::rule, 1> rules{{
    {"chained_tags", "A template line that opens or closes a section "
        "spanning lines holds that one tag, so the renderer strips it."}}};

constexpr std::array<ct::option, 1> options{{
    {"templates", "projects/ores.codegen/library/templates",
        "The directory whose .mustache files are read."}}};

const pt::regex& tag() {
    static const pt::regex out(R"(\{\{([#^/])\s*([^}]+?)\s*\}\})");
    return out;
}

/**
 * @brief The 1-based lines of @p text that hold several section tags and
 * nothing else, where one of the tags opens or closes a section that
 * spans lines.
 */
std::set<std::uint32_t> chained_lines(std::string_view text) {
    std::vector<std::tuple<std::string, std::size_t, std::size_t>> stack;
    std::vector<std::array<std::size_t, 4>> pairs;
    for (const auto& m : tag().find_all(text)) {
        const auto kind = m.group(1);
        const std::string name(co::strip(m.group(2)));
        if (kind == "#" || kind == "^") {
            stack.emplace_back(name, m.start(), m.end());
            continue;
        }
        for (auto i = stack.size(); i-- > 0;) {
            if (std::get<0>(stack[i]) != name)
                continue;
            pairs.push_back({std::get<1>(stack[i]), std::get<2>(stack[i]),
                m.start(), m.end()});
            stack.erase(stack.begin() + static_cast<std::ptrdiff_t>(i));
            break;
        }
    }
    std::set<std::uint32_t> out;
    for (const auto& [open_start, open_end, close_start, close_end] : pairs) {
        if (co::line_of(text, open_start) == co::line_of(text, close_start))
            continue;
        for (const auto& [start, end] : {std::pair{open_start, open_end},
                 std::pair{close_start, close_end}}) {
            const auto line_start = text.rfind('\n', start == 0 ? 0
                : start - 1);
            const auto from = start == 0 || line_start == std::string_view::npos
                ? 0 : line_start + 1;
            auto to = text.find('\n', end);
            to = to == std::string_view::npos ? text.size() : to;
            const auto line = text.substr(from, to - from);
            if (tag().find_all(line).size() < 2 ||
                !co::strip(tag().substitute(line, "")).empty())
                continue;
            out.insert(co::line_of(text, start));
        }
    }
    return out;
}

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    const std::string dir(v.option("templates"));
    for (const auto path : v.list_files(dir + "/", ".mustache")) {
        if (path.substr(dir.size() + 1).contains('/'))
            continue;
        const auto bytes = v.read_file(path).value_or("");
        const auto text = pt::universal_newlines(pt::decode_replacing(bytes));
        for (const auto line : chained_lines(text))
            co::report(out, path, co::offset_of_line(bytes, line), 0,
                "the line chains several section tags, and a section it "
                "opens or closes spans lines", "Give each tag its own line, "
                "so the renderer can strip it.");
    }
}

}

const ct::descriptor template_sections{
    .name = "template_sections",
    .version = 1,
    .summary = "Reports each template line that chains section tags of a "
        "section spanning lines.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}
