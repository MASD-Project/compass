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
#include <cstdint>
#include <expected>
#include <format>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "compass.codec/ops/data_tables.hpp"
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.docs/ops/regions.hpp"
#include "compass.docs/ops/skills.hpp"

namespace compass::docs::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace cops = compass::codec::ops;

constexpr std::string_view codec_name = "skill_levels";
constexpr std::string_view check_document = "compass/checks/skill_levels.org";

constexpr std::uint32_t skill_without_level = 1;
constexpr std::uint32_t level_without_skill = 2;
constexpr std::uint32_t unreadable = 3;

constexpr std::array<ct::rule, 4> rules{{
    co::refusal_rules[0],
    {"skill_without_level", "Every skill has a row in the table \"Levels\" "
        "of the check document: the level is a reviewed judgement, never a "
        "default."},
    {"level_without_skill", "Every row of the table \"Levels\" names a skill "
        "that exists."},
    {"unreadable", "The check document gives every table, column and "
        "setting the codec reads, and its rows agree with each other."}}};

constexpr std::array<std::string_view, 3> level_columns{"skill", "level",
    "reason"};
constexpr std::array<std::string_view, 4> system_columns{"level", "system",
    "horizon", "attenuation"};
constexpr std::array<std::string_view, 1> verb_columns{"verb"};
constexpr std::array<std::string_view, 2> setting_columns{"setting",
    "value"};

constexpr std::array<cops::table_need, 4> needed{{
    {"Levels", level_columns},
    {"Systems", system_columns},
    {"Read-only verbs", verb_columns},
    {"Settings", setting_columns}}};

/**
 * @brief What a session at a level sees by default. advisory: the skills
 * of its level and of the cross level, and it may reach outside them.
 * binding: the read-only skills and nothing else. cross: the skills of
 * this level are seen at every advisory level.
 */
enum class attenuation : std::uint8_t { advisory, binding, cross };

using attenuation_name = std::pair<std::string_view, attenuation>;

constexpr std::array<attenuation_name, 3> attenuations{{
    {"advisory", attenuation::advisory},
    {"binding", attenuation::binding},
    {"cross", attenuation::cross}}};

/**
 * @brief One row of the Systems table; the table's order is the order of
 * the levels.
 */
struct system_row {
    std::string level;
    std::string system;
    std::string horizon;
    attenuation kind = attenuation::advisory;
};

/**
 * @brief One row of the Levels table: a skill's level, and the line of the
 * check document the row is on.
 */
struct level_row {
    std::string level;
    std::uint32_t line = 0;
};

/**
 * @brief The Settings table of the check document: where the skills and
 * the levels page are, and the prose the page's inventory carries.
 */
struct settings {
    std::string skills_directory;
    std::string levels_page;
    std::string inventory_summary;
    std::string cross_summary;
};

/**
 * @brief One setting: its name in the table, where it goes, and whether it
 * may be empty or left out.
 */
struct setting_key {
    std::string_view name;
    std::string settings::* member;
    bool may_be_empty = false;
};

const std::array<setting_key, 4> setting_keys{{
    {"skills_directory", &settings::skills_directory},
    {"levels_page", &settings::levels_page},
    {"inventory_summary", &settings::inventory_summary, true},
    {"cross_summary", &settings::cross_summary, true}}};

/**
 * @brief Everything the check document states.
 */
struct project_data {
    settings paths;
    std::vector<system_row> systems;
    std::map<std::string, level_row, std::less<>> levels;
    std::set<std::string, std::less<>> read_only_verbs;

    [[nodiscard]] const system_row* system_of(std::string_view level) const {
        const auto it = std::ranges::find(systems, level, &system_row::level);
        return it == systems.end() ? nullptr : &*it;
    }

    [[nodiscard]] std::string_view level_of(std::string_view skill) const {
        const auto it = levels.find(skill);
        return it == levels.end() ? std::string_view{}
                                  : std::string_view(it->second.level);
    }
};

constexpr std::array<std::string_view, 2> former_inventory{
    "# BEGIN generated inventory (build/scripts/apply_skill_levels.py)",
    "# BEGIN generated inventory (build/scripts/generate_skills_catalogue.py)"};
constexpr ops::generated_region inventory_region{"inventory", codec_name,
    former_inventory};
constexpr ops::generated_region slices_region{"slices", codec_name, {}};

/**
 * @brief What the check document states, or why it states it wrongly,
 * written for the reader of the finding.
 */
std::expected<project_data, std::string> read_document(
    const cops::data_tables& data) {
    const auto wrong = [&](std::string_view table, std::uint32_t line,
                           const std::string& what) {
        return std::unexpected(std::format("{}:{}: {}, in the table \"{}\" "
            "{} reads", data.source, line, what, table, codec_name));
    };
    project_data out;

    const auto& s = *data.find("Settings");
    std::set<std::string, std::less<>> seen;
    for (std::size_t r = 0; r < s.rows.size(); ++r) {
        const auto& name = s.rows[r][s.column("setting")];
        const auto key = std::ranges::find(setting_keys, name,
            &setting_key::name);
        if (key == setting_keys.end())
            return wrong(s.name, s.row_lines[r], "the setting '" + name +
                "' is not one " + std::string(codec_name) + " has");
        if (!seen.insert(name).second)
            return wrong(s.name, s.row_lines[r], "the setting '" + name +
                "' is stated twice");
        const auto& value = s.rows[r][s.column("value")];
        if (value.empty() && !key->may_be_empty)
            return wrong(s.name, s.row_lines[r], "the setting '" + name +
                "' has no value");
        out.paths.*(key->member) = value;
    }
    for (const auto& key : setting_keys)
        if (!key.may_be_empty && !seen.contains(key.name))
            return wrong(s.name, s.line, "no row states the setting '" +
                std::string(key.name) + "'");
    if (!out.paths.skills_directory.ends_with('/'))
        out.paths.skills_directory.push_back('/');

    const auto& y = *data.find("Systems");
    bool has_cross = false;
    for (std::size_t r = 0; r < y.rows.size(); ++r) {
        const auto& row = y.rows[r];
        system_row system{.level = row[y.column("level")],
            .system = row[y.column("system")],
            .horizon = row[y.column("horizon")]};
        const auto& kind = row[y.column("attenuation")];
        const auto a = std::ranges::find(attenuations, kind,
            &attenuation_name::first);
        if (a == attenuations.end())
            return wrong(y.name, y.row_lines[r], "the attenuation '" + kind +
                "' is none of advisory, binding and cross");
        system.kind = a->second;
        if (system.level.empty())
            return wrong(y.name, y.row_lines[r], "the row names no level");
        if (out.system_of(system.level) != nullptr)
            return wrong(y.name, y.row_lines[r], "the level " + system.level +
                " is stated twice");
        if (system.kind == attenuation::cross && has_cross)
            return wrong(y.name, y.row_lines[r], "a second level is cross");
        has_cross = has_cross || system.kind == attenuation::cross;
        out.systems.push_back(std::move(system));
    }
    if (out.systems.empty())
        return wrong(y.name, y.line, "the table has no level");

    const auto& v = *data.find("Read-only verbs");
    for (std::size_t r = 0; r < v.rows.size(); ++r) {
        const auto& verb = v.rows[r][v.column("verb")];
        if (verb.empty())
            return wrong(v.name, v.row_lines[r], "the row names no verb");
        out.read_only_verbs.insert(verb);
    }

    const auto& l = *data.find("Levels");
    for (std::size_t r = 0; r < l.rows.size(); ++r) {
        const auto& row = l.rows[r];
        const auto& skill = row[l.column("skill")];
        const auto& level = row[l.column("level")];
        if (skill.empty())
            return wrong(l.name, l.row_lines[r], "the row names no skill");
        if (out.system_of(level) == nullptr)
            return wrong(l.name, l.row_lines[r], "the level '" + level +
                "' of " + skill + " is no row of the table \"Systems\"");
        if (row[l.column("reason")].empty())
            return wrong(l.name, l.row_lines[r], "the row gives no reason "
                "for the level of " + skill);
        if (!out.levels.emplace(skill, level_row{level, l.row_lines[r]})
                .second)
            return wrong(l.name, l.row_lines[r], "the skill " + skill +
                " is stated twice");
    }
    return out;
}

/**
 * @brief The verb of a skill named <prefix>-<domain>-<verb>[-<object>]:
 * its third segment, or nothing.
 */
std::string_view verb_of(std::string_view name) {
    for (int i = 0; i < 2; ++i) {
        const auto dash = name.find('-');
        if (dash == std::string_view::npos)
            return {};
        name.remove_prefix(dash + 1);
    }
    return name.substr(0, name.find('-'));
}

/**
 * @brief True when a session at @p level sees @p name: at a binding level
 * the skills whose verb is read-only, at any other the skills of that
 * level and of the cross level.
 */
bool in_scope(const project_data& d, std::string_view name,
    const system_row& level) {
    if (level.kind == attenuation::binding)
        return d.read_only_verbs.contains(verb_of(name));
    const auto declared = d.level_of(name);
    if (declared == level.level)
        return true;
    const auto* system = d.system_of(declared);
    return system != nullptr && system->kind == attenuation::cross;
}

std::string_view strength(const system_row& s) {
    return s.kind == attenuation::binding ? "binding" : "advisory";
}

std::string inventory(const project_data& d,
    const std::vector<std::string>& names) {
    std::vector<std::string> lines{ops::begin_marker(inventory_region), ""};
    if (!d.paths.inventory_summary.empty())
        lines.insert(lines.end(), {d.paths.inventory_summary, ""});
    lines.insert(lines.end(), {"#+ATTR_HTML: :class hug-leading",
        "| Level | System | Horizon | Sees | Attenuation |",
        "|-------+--------+---------+------+-------------|"});
    for (const auto& s : d.systems) {
        if (s.kind == attenuation::cross)
            continue;
        const auto n = std::ranges::count_if(names,
            [&](const std::string& name) { return in_scope(d, name, s); });
        lines.push_back("| =" + s.level + "= | " + s.system + " | " +
            s.horizon + " | " + std::to_string(n) + " skills | " +
            std::string(strength(s)) + " |");
    }
    for (const auto& s : d.systems) {
        if (s.kind != attenuation::cross)
            continue;
        const auto n = std::ranges::count_if(names,
            [&](const std::string& name) {
                return d.level_of(name) == s.level;
            });
        lines.emplace_back();
        lines.push_back(std::to_string(n) + " skills are tagged =" + s.level +
            "= and appear in every advisory slice" +
            (d.paths.cross_summary.empty() ? std::string(".")
                : ": " + d.paths.cross_summary));
    }
    lines.emplace_back();
    lines.push_back(ops::end_marker(inventory_region));
    return ops::join_lines(lines);
}

std::string slices(const project_data& d,
    const std::vector<std::string>& names) {
    std::vector<std::string> lines{ops::begin_marker(slices_region), "",
        "The skills a session at each level sees by default. At a binding "
        "level they are the read-only skills and nothing else; at an "
        "advisory one, the skills of that level and of the cross level."};
    for (const auto& s : d.systems) {
        std::vector<std::string_view> seen;
        for (const auto& name : names)
            if (in_scope(d, name, s))
                seen.emplace_back(name);
        lines.emplace_back();
        lines.push_back("=" + s.level + "=, " + std::string(strength(s)) +
            ": " + std::to_string(seen.size()) + " of " +
            std::to_string(names.size()) + " skills.");
        if (seen.empty())
            continue;
        lines.emplace_back();
        for (const auto name : seen)
            lines.push_back("- " + std::string(name));
    }
    lines.emplace_back();
    lines.push_back(ops::end_marker(slices_region));
    return ops::join_lines(lines);
}

/**
 * @brief @p text with its first #+level: line stating @p level, or with
 * the line put after the scaffold's #+export_exclude_tags: noexport line
 * when it has none; nothing when it has neither.
 */
std::optional<std::string> stamped(std::string_view text,
    std::string_view level) {
    constexpr std::string_view key = "#+level:";
    const auto line = std::string(key) + " " + std::string(level);
    auto at = text.starts_with(key) ? 0
        : text.find("\n" + std::string(key));
    if (at != std::string_view::npos) {
        if (text[at] == '\n')
            ++at;
        auto end = text.find('\n', at);
        if (end == std::string_view::npos)
            end = text.size();
        const auto value = text.substr(at + key.size(),
            end - at - key.size());
        if (co::strip(value) == level)
            return std::string(text);
        return std::string(text.substr(0, at)) + line +
            std::string(text.substr(end));
    }
    constexpr std::string_view anchor = "#+export_exclude_tags: noexport\n";
    const auto found = text.find(anchor);
    if (found == std::string_view::npos)
        return std::nullopt;
    const auto split = found + anchor.size();
    return std::string(text.substr(0, split)) + line + "\n" +
        std::string(text.substr(split));
}

/**
 * @brief The BEGIN marker of the first generated region of @p text, or
 * nothing: a page holding one is that generator's output, and one file has
 * one writer.
 */
std::string_view generated_by(std::string_view text) {
    constexpr std::string_view marker = "# BEGIN generated ";
    auto at = text.starts_with(marker) ? 0 : text.find("\n" +
        std::string(marker));
    if (at == std::string_view::npos)
        return {};
    if (text[at] == '\n')
        ++at;
    return text.substr(at, text.find('\n', at) - at);
}

/**
 * @brief Reports each skill with no row and each row with no skill; true
 * when there is neither.
 */
bool agree(const ct::corpus_view& v, const project_data& d,
    const std::string& source, const std::vector<std::string>& names,
    ct::file_output& out) {
    bool ok = true;
    for (const auto& name : names) {
        if (d.levels.contains(name))
            continue;
        ok = false;
        co::report(out.refusals, d.paths.skills_directory + name +
            "/SKILL.org", 0, skill_without_level, "the skill " + name +
            " has no row in the table \"Levels\" of " + source,
            "Add a row giving its level and the reason for it.");
    }
    std::optional<std::string> text;
    for (const auto& [skill, row] : d.levels) {
        if (std::ranges::binary_search(names, skill))
            continue;
        ok = false;
        if (!text)
            text = v.read_file(source).value_or("");
        co::report(out.refusals, source, co::offset_of_line(*text, row.line),
            level_without_skill, "the row names the skill " + skill +
            ", which has no " + d.paths.skills_directory + skill +
            "/SKILL.org", "Remove the row, or restore the skill.");
    }
    return ok;
}

void write_page(const ct::corpus_view& v, const project_data& d,
    const std::vector<std::string>& names, ct::file_output& out) {
    const auto& page = d.paths.levels_page;
    const auto text = v.read_file(page);
    if (!text) {
        co::refuse(out, page, "the page does not exist",
            "Restore the page with its generated inventory.");
        return;
    }
    const auto listed = inventory(d, names);
    auto fresh = ops::replace_region(*text, inventory_region, listed,
        ops::replace::every);
    if (!fresh) {
        co::refuse(out, page, "the page has no generated inventory region",
            "Restore its BEGIN and END generated inventory markers.");
        return;
    }
    const auto sliced = slices(d, names);
    if (auto both = ops::replace_region(*fresh, slices_region, sliced,
            ops::replace::every))
        fresh = std::move(both);
    else
        fresh = ops::replace_region(*fresh, inventory_region,
            listed + "\n\n" + sliced, ops::replace::first);
    out.files.add(page, std::move(*fresh));
}

void run(const ct::corpus_view& v, ct::file_output& out) {
    const auto data = cops::tables_for(codec_name, v.option(cops::data_option),
        needed);
    if (!data) {
        co::report(out.refusals, check_document, 0, unreadable, data.error(),
            "Give the tables the message names.");
        return;
    }
    const auto document = read_document(*data);
    if (!document) {
        co::report(out.refusals, data->source, 0, unreadable,
            document.error(), "Correct the row the message names.");
        return;
    }
    const auto& d = *document;
    const auto names = ops::skill_names(v, d.paths.skills_directory);
    if (!agree(v, d, data->source, names, out))
        return;
    for (const auto& name : names) {
        const auto path = d.paths.skills_directory + name + "/SKILL.org";
        const auto text = v.read_file(path).value_or("");
        const auto level = d.level_of(name);
        auto fresh = stamped(text, level);
        if (const auto marker = generated_by(text); !marker.empty()) {
            if (fresh != text)
                co::refuse(out, path, "the page holds the region \"" +
                    std::string(marker) + "\", so its generator writes it, "
                    "and its #+level: line does not say " +
                    std::string(level), "Give the page the line #+level: " +
                    std::string(level) + " by hand; its generator keeps it.");
            continue;
        }
        if (fresh)
            out.files.add(path, std::move(*fresh));
        else
            co::refuse(out, path, "the skill has no #+level: line, and no "
                "#+export_exclude_tags: noexport line to put one after",
                "Give the skill the scaffold's #+export_exclude_tags: "
                "noexport line.");
    }
    write_page(v, d, names, out);
}

constexpr std::array<ct::option, 1> options{{
    {cops::data_option, "", "The tables of the check document whose "
        "Definition names this codec: each skill's level and the reason for "
        "it, the levels in order with the System, horizon and attenuation "
        "of each, the verbs that change nothing, and the settings that "
        "place the skills and the levels page."}}};

}

const ct::descriptor skill_levels{
    .name = "skill_levels",
    .version = 5,
    .summary = "Stamps each skill's level from the project's check document, "
               "and writes what each System sees.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::file_output>{run}};

}
