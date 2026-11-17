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
#include "compass.docs/codecs/skill_tables.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.docs/ops/regions.hpp"
#include "compass.docs/ops/skills.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::docs::codecs {

namespace {

namespace ct = compass::codec::types;
namespace pt = compass::platform::text;

using level_entry = std::pair<std::string_view, std::string_view>;

/**
 * @brief The script's LEVELS: each skill and the System it serves, sorted
 * by skill.
 */
constexpr std::array<level_entry, 76> levels{{
        {"compass-agile-add-release-notes", "s4"},
        {"compass-agile-add-timeline-snapshot", "s3"},
        {"compass-agile-brainstorm-idea", "s5"},
        {"compass-agile-close-task", "s1"},
        {"compass-agile-find-bearings", "cross"},
        {"compass-agile-find-heading", "s3"},
        {"compass-agile-open-sprint", "s3"},
        {"compass-agile-plan-sprint", "s3"},
        {"compass-agile-refine-backlog", "s3"},
        {"compass-agile-review-sprint", "s3"},
        {"compass-agile-start-hotfix", "s1"},
        {"compass-agile-start-story", "s2"},
        {"compass-agile-start-task", "s1"},
        {"compass-code-add-domain-type", "s1"},
        {"compass-code-add-tests", "s1"},
        {"compass-code-investigate-test-failure", "s1"},
        {"compass-code-review-comments", "s1"},
        {"compass-code-review-component", "s1"},
        {"compass-code-review-data-model", "s1"},
        {"compass-code-review-pr", "s1"},
        {"compass-code-run-build", "s1"},
        {"compass-code-run-feature-test", "s1"},
        {"compass-code-start-branch", "s1"},
        {"compass-codegen-add-component", "s1"},
        {"compass-codegen-add-entity", "s1"},
        {"compass-codegen-add-sql-schema", "s1"},
        {"compass-codegen-add-surface-entity", "s1"},
        {"compass-codegen-fix-drift", "s1"},
        {"compass-codegen-sync-cmake-sources", "s1"},
        {"compass-devops-deploy-manual", "s1"},
        {"compass-devops-deploy-settings", "s1"},
        {"compass-devops-deploy-site", "s1"},
        {"compass-devops-deploy-skills", "s1"},
        {"compass-devops-deprovision-environment", "s2"},
        {"compass-devops-provision-environment", "s2"},
        {"compass-devops-recreate-db", "s1"},
        {"compass-devops-run-environment", "s1"},
        {"compass-devops-run-shell", "s1"},
        {"compass-devops-run-sql", "s1"},
        {"compass-devops-setup-environment", "s1"},
        {"compass-devops-start-services", "s1"},
        {"compass-devops-stop-services", "s1"},
        {"compass-devops-update-environment", "s1"},
        {"compass-doc-add", "cross"},
        {"compass-doc-add-chart", "s1"},
        {"compass-doc-add-class-diagram", "s1"},
        {"compass-doc-add-component-model", "s1"},
        {"compass-doc-add-entity-chapter", "s1"},
        {"compass-doc-add-er-diagram", "s1"},
        {"compass-doc-add-memory", "cross"},
        {"compass-doc-add-recipe", "s1"},
        {"compass-doc-find", "cross"},
        {"compass-doc-review-academic", "cross"},
        {"compass-doc-review-ste100", "cross"},
        {"compass-doc-run-manual-screenshots", "s1"},
        {"compass-doc-show", "cross"},
        {"compass-doc-sync-index", "cross"},
        {"compass-doc-sync-recipes", "s1"},
        {"compass-doc-update-manual", "s1"},
        {"compass-helm", "cross"},
        {"compass-method-bug-fix", "s1"},
        {"compass-method-feature", "s1"},
        {"compass-method-figure-it-out", "s2"},
        {"compass-method-investigation", "cross"},
        {"compass-method-prototype", "s1"},
        {"compass-method-refactoring", "s1"},
        {"compass-pr-address-review", "s1"},
        {"compass-pr-merge", "s1"},
        {"compass-pr-raise", "s1"},
        {"compass-pr-sync-branch", "s1"},
        {"compass-principle-never-block-on-the-human", "cross"},
        {"compass-principle-type-system-discipline", "s1"},
        {"compass-skill-add", "cross"},
        {"compass-skill-delete", "cross"},
        {"compass-skill-review", "cross"},
        {"compass-skill-show-catalogue", "cross"},
}};

struct system_row {
    std::string_view level;
    std::string_view system;
    std::string_view horizon;
};

constexpr std::array<system_row, 6> systems{{
    {"s1", "Agent", "Hours"},
    {"s2", "Orchestrator", "Days"},
    {"s3", "Sprint Planner", "Weeks"},
    {"s3star", "Auditor", "Continuous"},
    {"s4", "Version Planner", "Months"},
    {"s5", "Identity Steward", "Indefinite"}}};

constexpr std::string_view levels_doc =
    "doc/llm/skills/skill_systems_levels.org";
constexpr std::string_view inventory_begin =
    "# BEGIN generated inventory (build/scripts/apply_skill_levels.py)";
constexpr std::string_view old_inventory_begin =
    "# BEGIN generated inventory (build/scripts/generate_skills_catalogue.py)";
constexpr std::string_view inventory_end = "# END generated inventory";

std::string_view level_of(std::string_view name) {
    const auto it = std::ranges::lower_bound(levels, name, {},
        &level_entry::first);
    return it != levels.end() && it->first == name ? it->second
        : std::string_view{};
}

/**
 * @brief True when a session at @p level sees @p name: at s3star the
 * skills whose verb, the third segment of the name, is show or find; at
 * any other level the skills of that level and the cross ones.
 */
bool in_scope(std::string_view name, std::string_view level) {
    if (level == "s3star") {
        std::string_view rest = name;
        for (int i = 0; i < 2; ++i) {
            const auto dash = rest.find('-');
            if (dash == std::string_view::npos)
                return false;
            rest.remove_prefix(dash + 1);
        }
        const auto verb = rest.substr(0, rest.find('-'));
        return verb == "show" || verb == "find";
    }
    const auto declared = level_of(name);
    return declared == level || declared == "cross";
}

/**
 * @brief @p text with its level set to @p level, as set_level leaves it.
 */
std::string with_level(std::string text, std::string_view level) {
    static const pt::regex current(R"(^#\+level:\s*(\S+)\s*$)",
        {.multiline = true});
    static const pt::regex line(R"(^#\+level:.*$)", {.multiline = true});
    const auto m = current.search(text);
    if (m && m->group(1) == level)
        return text;
    const auto wanted = "#+level: " + std::string(level);
    if (const auto l = line.search(text))
        return text.replace(l->start(), l->end() - l->start(), wanted);
    constexpr std::string_view anchor = "#+export_exclude_tags: noexport\n";
    if (const auto at = text.find(anchor); at != std::string::npos)
        text.insert(at + anchor.size(), wanted + "\n");
    return text;
}

std::string inventory(const std::vector<std::string>& names) {
    std::vector<std::string> lines{std::string(inventory_begin), "",
        "What each System sees by default, counted from the levels the "
        "skills carry. Attenuation is advisory everywhere except the audit "
        "channel, where it is binding and enforced by what is deployed.",
        "", "#+ATTR_HTML: :class hug-leading",
        "| Level | System | Horizon | Sees | Attenuation |",
        "|-------+--------+---------+------+-------------|"};
    for (const auto& s : systems) {
        const auto n = std::ranges::count_if(names,
            [&](const std::string& name) { return in_scope(name, s.level); });
        lines.push_back("| =" + std::string(s.level) + "= | " +
            std::string(s.system) + " | " + std::string(s.horizon) + " | " +
            std::to_string(n) + " skills | " +
            (s.level == "s3star" ? "binding" : "advisory") + " |");
    }
    const auto cross = std::ranges::count_if(names,
        [](const std::string& name) { return level_of(name) == "cross"; });
    lines.emplace_back();
    lines.push_back(std::to_string(cross) + " skills are tagged =cross= and "
        "appear in every advisory slice: the documents and the skills "
        "themselves, and the orientation any System starts from.");
    lines.emplace_back();
    lines.emplace_back(inventory_end);
    return ops::join_lines(lines);
}

void apply_levels(const ct::corpus_view& v, ct::file_table& out) {
    const auto names = ops::skill_names(v);
    const bool complete = std::ranges::all_of(names,
        [](const std::string& n) { return !level_of(n).empty(); }) &&
        std::ranges::all_of(levels, [&](const level_entry& l) {
            return std::ranges::binary_search(names, l.first);
        });
    if (!complete)
        return;
    for (const auto& name : names) {
        const auto path = std::string(ops::skills_dir) + name + "/SKILL.org";
        if (const auto text = v.read_file(path))
            out.add(path, with_level(*text, level_of(name)));
    }
    const auto text = v.read_file(levels_doc);
    if (!text)
        return;
    const auto block = inventory(names);
    auto fresh = ops::replace_region(*text, inventory_begin, inventory_end,
        block, ops::replace::every);
    if (!fresh)
        fresh = ops::replace_region(*text, old_inventory_begin, inventory_end,
            block, ops::replace::every);
    if (fresh)
        out.add(levels_doc, *fresh);
}

}

std::span<const std::pair<std::string_view, std::string_view>>
skill_level_table() noexcept {
    return levels;
}

const ct::descriptor skill_levels{
    .name = "skill_levels",
    .version = 1,
    .summary = "Writes the level of every skill and the inventory of what "
        "each System sees.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = {},
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::file_table>{apply_levels}};

}
