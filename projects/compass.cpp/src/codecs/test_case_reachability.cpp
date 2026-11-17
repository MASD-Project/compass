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
#include "compass.cpp/codecs/test_case_reachability.hpp"

#include <array>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codec/types/fact.hpp"
#include "compass.cpp/ops/test_case_reachability.hpp"
#include "compass.cpp/ops/text.hpp"

namespace compass::cpp::codecs {

namespace {

namespace ct = compass::codec::types;

constexpr std::array<ct::rule, 4> rules{{
    {"case_in_conditional",
        "No test case sits inside a conditional compilation block."},
    {"unbalanced_conditional",
        "Every conditional block a file opens, it closes."},
    {"raw_string_moves",
        "No raw string moves when the backslash-newline splices go."},
    {"no_test_case", "The test sources declare at least one case."}}};

constexpr std::string_view projects = "projects";

void add(ct::diagnostic_table& out, std::string_view file,
    std::uint32_t at, std::uint32_t rule, const std::string& message,
    std::string_view fix) {
    out.add({.document = compass::org::types::document_id::none(),
        .range = {at, at},
        .rule = ct::rule_id{rule},
        .level = ct::severity::error,
        .message = message,
        .fix = fix,
        .file = file});
}

void report(std::string_view path, std::string_view text,
    const ops::scan_result& r, bool readable, ct::diagnostic_table& out) {
    if (readable) {
        std::set<std::pair<std::uint32_t, std::uint32_t>> listed;
        for (const auto& c : r.cases) {
            if (!listed.emplace(c.line, c.opened_line).second)
                continue;
            add(out, path, ops::offset_of_line(text, c.line), 0,
                "a test case sits inside the conditional block opened by `" +
                    c.opened_text + "`, so a build can compile it away and "
                    "the suite still reports green",
                "declare the case unconditionally and move the condition "
                "into its body, with SKIP(...) in the branch that cannot "
                "run");
        }
    }
    for (const auto& p : r.problems) {
        const auto at = ops::offset_of_line(text, p.line);
        switch (p.kind) {
        case ops::reading_problem::unopened_endif:
            add(out, path, at, 1, "closes a conditional that nothing "
                "opened", "close the guard in the file that opens it");
            break;
        case ops::reading_problem::unclosed_conditional:
            add(out, path, at, 1, "opens a conditional that never closes, "
                "and the guard leaks into whatever includes the file",
                "close the guard in the file that opens it");
            break;
        case ops::reading_problem::raw_string_moves:
            add(out, path, at, 2, "a raw string literal here begins or ends "
                "somewhere else once the backslash-newline splices are "
                "removed, so the file's conditionals cannot be read from it "
                "alone and its cases are left out",
                "remove the trailing backslash, or move the literal out of "
                "the test source");
            break;
        }
    }
}

std::vector<std::string_view> test_sources(const ct::corpus_view& v) {
    auto paths = v.list_files("projects/");
    std::erase_if(paths, [](std::string_view path) {
        return !ops::is_test_source(path);
    });
    return paths;
}

/**
 * @brief Reports what is wrong with one test source. Its fact is the
 * number of cases it declares, none when its conditionals cannot be read.
 */
void map(std::string_view path, std::string_view text,
    const ct::option_values&, ct::file_scan& out) {
    const auto r = ops::scan_source(text);
    bool readable = true;
    for (const auto& p : r.problems)
        readable = readable &&
            p.kind != ops::reading_problem::raw_string_moves;
    report(path, text, r, readable, out.findings);
    ct::fact_writer(out.fact).number(
        readable ? static_cast<std::uint32_t>(r.declared) : 0);
}

void reduce(std::span<const ct::scanned_file> sources,
    const ct::corpus_view&, ct::diagnostic_table& out) {
    std::uint64_t declared = 0;
    for (const auto& s : sources)
        declared += ct::fact_reader(s.fact).number();
    if (sources.empty())
        add(out, projects, 0, 3, "no test source is found under projects/",
            "check that the test sources are where the rule looks");
    else if (declared == 0)
        add(out, projects, 0, 3, "no test case is declared in any test "
            "source", "check that the test sources use the Catch2 macros "
            "the rule looks for");
}

}

const ct::descriptor test_case_reachability{
    .name = "test_case_reachability",
    .version = 2,
    .summary = "Reports each test case inside a conditional compilation "
        "block and each test source whose conditionals cannot be read.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::scan_function{test_sources, map, reduce}};

}
