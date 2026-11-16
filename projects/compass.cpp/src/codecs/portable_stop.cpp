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
#include "compass.cpp/codecs/portable_stop.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::cpp::codecs {

namespace {

namespace ct = compass::codec::types;
namespace pt = compass::platform::text;

constexpr std::array<ct::rule, 1> rules{{
    {"stop_type_outside_platform", "No code outside ores.platform names a "
        "C++20 stop type or includes <stop_token>."}}};

constexpr std::array<std::string_view, 8> suffixes{".cpp", ".cc", ".cxx",
    ".hpp", ".h", ".hxx", ".ipp", ".inc"};

constexpr std::array<std::string_view, 5> skipped{"node_modules", "venv",
    ".venv", "build", "external"};

/**
 * @brief True for a source under projects/ that the script's walk reaches:
 * no directory on its way is skipped, its suffix is a C++ one, and it is
 * not in ores.platform. The script lower-cases the suffix; listing by
 * suffix does not, and ORE Studio has no source with a capital in one.
 */
bool is_scanned(std::string_view path) {
    const auto parts = ops::components_of(path);
    if (parts.size() < 2 || parts[1] == "ores.platform")
        return false;
    for (std::size_t i = 1; i + 1 < parts.size(); ++i) {
        if (std::ranges::find(skipped, parts[i]) != skipped.end())
            return false;
    }
    return std::ranges::find(suffixes, ops::suffix_of(path)) !=
        suffixes.end();
}

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    static const pt::regex noise(
        R"re(//[^\n]*|/\*.*?\*/|R"(?P<d>[^(\s]*)\(.*?\)(?P=d)"|"(?:\\.|[^"\\\n])*")re",
        {.dotall = true});
    static const pt::regex stop_type(
        R"(\bstd::(jthread|stop_token|stop_source|stop_callback)\b)");
    static const pt::regex stop_include(R"(^\s*#\s*include\s*<stop_token>)");

    std::vector<std::string_view> paths;
    for (const auto suffix : suffixes) {
        for (const auto p : v.list_files("projects/", suffix)) {
            if (is_scanned(p))
                paths.push_back(p);
        }
    }
    for (const auto path : paths) {
        const auto bytes = v.read_file(path).value_or("");
        const auto text = ops::universal_newlines(ops::decode_ignoring(bytes));
        const auto blanked = noise.substitute(text, [](const auto& m) {
            return pt::blank_keeping_newlines(m.group());
        });
        std::uint32_t number = 0;
        for (const auto line : ops::split_lines(blanked)) {
            ++number;
            std::string name;
            if (const auto m = stop_type.search(line))
                name = "std::" + std::string(m->group(1));
            else if (stop_include.match(line))
                name = "<stop_token>";
            else
                continue;
            ops::report(out, path, ops::offset_of_line(bytes, number), 0,
                name + " is not portable: Apple's libc++ has no C++20 stop "
                "types", "use the same name in ores::platform::concurrency "
                "from stoppable_thread.hpp");
        }
    }
}

}

const ct::descriptor portable_stop{
    .name = "portable_stop",
    .version = 1,
    .summary = "Reports each line outside ores.platform that names a C++20 "
        "stop type.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}
