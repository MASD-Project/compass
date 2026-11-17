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
#include "compass.docs/codecs/tree_gates.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/glob.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::docs::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

std::string read(const ct::corpus_view& v, std::string_view path) {
    return pt::universal_newlines(pt::decode_replacing(
        v.read_file(path).value_or("")));
}

/**
 * @brief Whether @p path names a file of the checkout as the operating
 * system resolves it: "." and empty components are dropped, and ".." goes
 * up only from a directory that exists.
 */
bool names_a_file(const ct::corpus_view& v, std::string_view path) {
    if (path.starts_with('/'))
        return false;
    std::vector<std::string_view> parts;
    std::string so_far;
    for (const auto part : co::components_of(path)) {
        if (part.empty() || part == ".")
            continue;
        if (part == "..") {
            if (parts.empty() || v.list_files(so_far + "/").empty())
                return false;
            parts.pop_back();
        } else {
            parts.push_back(part);
        }
        so_far.clear();
        for (const auto p : parts)
            so_far += (so_far.empty() ? "" : "/") + std::string(p);
    }
    return v.file_exists(so_far);
}

constexpr std::array<ct::rule, 1> recipe_rules{{
    {"orphan_recipe", "Every shell recipe names, in its GENERATED header, "
        "the document it was tangled from, and that document exists."}}};

void shell_recipes(const ct::corpus_view& v, ct::diagnostic_table& out) {
    static const pt::regex provenance(
        "^# GENERATED from (\\S+) \xe2\x80\x94 do not edit by hand\\.$",
        {.multiline = true});
    for (const auto recipe : co::glob(v,
             "projects/ores.shell/scripts/library/*/*.ores")) {
        const auto text = read(v, recipe);
        const auto m = provenance.search(text);
        std::string problem;
        if (!m)
            problem = "header names no source document";
        else if (!names_a_file(v, m->group(1)))
            problem = "source " + std::string(m->group(1)) + " does not exist";
        else
            continue;
        co::report(out, recipe, 0, 0, problem, "Delete the recipe, or "
            "restore the document it was tangled from.");
    }
}

constexpr std::array<ct::rule, 1> skill_rules{{
    {"outside_namespace", "Every skill under doc/llm/skills is named "
        "compass-..."}}};

void skill_names(const ct::corpus_view& v, ct::diagnostic_table& out) {
    for (const auto path : co::glob(v, "doc/llm/skills/*/SKILL.org")) {
        const auto name = co::components_of(path)[3];
        if (name.starts_with("compass-"))
            continue;
        co::report(out, path, 0, 0, "skill " + std::string(name) + " is "
            "outside the compass- namespace", "Run "
            "build/scripts/rename_skills_to_compass_namespace.py.");
    }
}

constexpr std::array<std::string_view, 2> pull_request_trigger_sources{
    "B83064E5-6357-4FC6-A8BE-395DD429ED2A",
    "BC85C0DF-99BE-4079-A649-88A236AE0ACF"};

constexpr std::array<ct::rule, 2> trigger_rules{{
    {"pull_request_trigger", "No workflow triggers on a pull request.",
        pull_request_trigger_sources},
    {"no_workflows", "The repository has workflow files to read."}}};

bool is_pull_request_event(std::string_view word) {
    return word == "pull_request" || word == "pull_request_target";
}

/**
 * @brief @p line without a YAML comment: from a # at its start or after a
 * blank.
 */
std::string_view uncommented(std::string_view line) {
    for (std::size_t i = 0; i < line.size(); ++i)
        if (line[i] == '#' && (i == 0 || co::is_space(line[i - 1])))
            return line.substr(0, i);
    return line;
}

/**
 * @brief The 1-based lines of @p lines that name a pull request event of
 * the workflow's top-level on key, stated as a scalar, a flow sequence or
 * mapping, or a block sequence or mapping at any indentation.
 */
std::vector<std::uint32_t> pull_request_events(
    const std::vector<std::string_view>& lines) {
    static const pt::regex on_key(R"(^(?:on|"on"|'on')\s*:(.*)$)");
    static const pt::regex word(R"([A-Za-z0-9_-]+)");
    std::vector<std::uint32_t> out;
    for (std::size_t n = 0; n < lines.size(); ++n) {
        const auto m = on_key.match(lines[n]);
        if (!m)
            continue;
        const auto inline_value = co::strip(uncommented(m->group(1)));
        if (!inline_value.empty()) {
            const auto words = word.find_all(inline_value);
            if (std::ranges::any_of(words, [](const auto& w) {
                    return is_pull_request_event(w.group());
                }))
                out.push_back(static_cast<std::uint32_t>(n + 1));
            continue;
        }
        std::size_t child = 0;
        for (auto k = n + 1; k < lines.size(); ++k) {
            const auto content = co::strip(uncommented(lines[k]));
            if (content.empty())
                continue;
            const auto indent = co::skip_spaces(lines[k], 0);
            if (indent == 0)
                break;
            if (child == 0)
                child = indent;
            if (indent != child)
                continue;
            auto key = content.starts_with("- ")
                ? co::strip(content.substr(2))
                : content.substr(0, content.find(':'));
            if (key.size() >= 2 && (key.front() == '"' || key.front() == '\''))
                key = key.substr(1, key.size() - 2);
            if (is_pull_request_event(key))
                out.push_back(static_cast<std::uint32_t>(k + 1));
        }
    }
    return out;
}

void workflow_triggers(const ct::corpus_view& v, ct::diagnostic_table& out) {
    std::set<std::string_view> files;
    for (const auto suffix : {"*.yml", "*.yaml"}) {
        for (const auto p : co::glob(v, std::string(".github/workflows/") +
                 suffix))
            files.insert(p);
    }
    if (files.empty()) {
        co::report(out, ".github/workflows", 0, 1, "no workflow files under "
            ".github/workflows", "The check reads .github/workflows/*.yml "
            "and *.yaml.");
        return;
    }
    for (const auto path : files) {
        const auto bytes = v.read_file(path).value_or("");
        const auto text = pt::universal_newlines(pt::decode_replacing(bytes));
        for (const auto number : pull_request_events(co::split_lines(text))) {
            co::report(out, path, co::offset_of_line(bytes, number), 0,
                "the workflow triggers on a pull request", "GitHub runs no "
                "pull-request check: the local runner is the gate. Drop the "
                "pull_request trigger and keep push to main and dispatch.");
        }
    }
}

}

const ct::descriptor shell_recipe_provenance{
    .name = "shell_recipe_provenance",
    .version = 1,
    .summary = "Reports each shell recipe that names no existing document "
        "as its source.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = recipe_rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{shell_recipes}};

const ct::descriptor skill_namespace{
    .name = "skill_namespace",
    .version = 1,
    .summary = "Reports each skill outside the compass- namespace.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = skill_rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{skill_names}};

const ct::descriptor no_pr_triggers{
    .name = "no_pr_triggers",
    .version = 1,
    .summary = "Reports each workflow line that triggers on a pull request.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = trigger_rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{workflow_triggers}};

}
