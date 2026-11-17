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
#include "compass.cpp/codecs/shell.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/lexer.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::cpp::codecs {

namespace {

namespace ct = compass::codec::types;
namespace pt = compass::platform::text;

constexpr std::string_view shell_root = "projects/ores.shell/";

std::vector<std::string_view> sorted_sources(const ct::corpus_view& v,
    std::string_view root) {
    auto paths = v.list_files(root, ".cpp");
    std::ranges::sort(paths, ops::component_less);
    return paths;
}

/**
 * @brief The code of @p bytes with its comments blanked and its literals
 * kept, offset for offset; decoded, and so without offsets, when it is not
 * UTF-8.
 */
std::pair<std::string, bool> code_of(std::string_view bytes) {
    auto text = ops::without_comments(bytes);
    if (pt::is_utf8(bytes))
        return {std::move(text), true};
    return {pt::universal_newlines(pt::decode_replacing(text)), false};
}

constexpr std::array<ct::rule, 3> unit_rules{{
    {"menu_built_twice", "One command unit builds each of its menus once."},
    {"menu_owned_twice", "One menu name has one owning file."},
    {"unit_registered_twice", "A file registers each command unit once."}}};

void run_units(const ct::corpus_view& v, ct::diagnostic_table& out) {
    static const pt::regex menu(R"re(make_unique<cli::Menu>\(\s*"([^"]+)"\s*\))re");
    static const pt::regex registration(R"(^\s*(\w+)::register_commands\()");
    std::map<std::string, std::string_view> owner;
    for (const auto path : sorted_sources(v, shell_root)) {
        const auto parts = ops::components_of(path);
        if (std::ranges::find(parts, "tests") != parts.end())
            continue;
        const auto [text, exact] = code_of(v.read_file(path).value_or(""));
        const auto at = [&](std::size_t offset) {
            return static_cast<std::uint32_t>(exact ? offset : 0);
        };
        std::map<std::string, std::vector<std::size_t>> menus;
        std::vector<std::pair<std::string, std::size_t>> in_order;
        for (const auto& m : menu.find_all(text)) {
            menus[std::string(m.group(1))].push_back(m.start());
            in_order.emplace_back(m.group(1), m.start());
        }
        for (const auto& [name, sites] : menus) {
            if (sites.size() > 1)
                ops::report(out, path, at(sites[1]), 0, "builds the menu '" +
                    name + "' " + std::to_string(sites.size()) + " times",
                    "one unit has one menu");
        }
        for (const auto& [name, offset] : in_order) {
            const auto o = owner.find(name);
            if (o != owner.end() && o->second != path)
                ops::report(out, path, at(offset), 1, "builds the menu '" +
                    name + "', which " + std::string(o->second) +
                    " already builds", "one name has one owner");
            owner.try_emplace(name, path);
        }
        std::map<std::string, std::vector<std::size_t>> units;
        for (const auto line : ops::split_lines(text)) {
            if (const auto m = registration.match(line))
                units[std::string(m->group(1))].push_back(
                    static_cast<std::size_t>(line.data() - text.data()));
        }
        for (const auto& [unit, sites] : units) {
            if (sites.size() > 1)
                ops::report(out, path, at(sites[1]), 2, "registers " + unit +
                    " " + std::to_string(sites.size()) + " times, so the "
                    "second registration finds the name taken and the shell "
                    "stops", "register the unit once");
        }
    }
}

constexpr std::array<ct::rule, 2> menu_rules{{
    {"menu_with_prompt", "A menu is constructed from its name alone, which "
        "Menu::Prompt() reads back."},
    {"root_name_collides", "No two units own one root name."}}};

void run_menus(const ct::corpus_view& v, ct::diagnostic_table& out) {
    static const pt::regex construction(
        R"re(cli::Menu\s*>?\s*\(\s*"([^"]*)"\s*([,)]))re");
    static const pt::regex root_command(R"re(root_menu\.Insert\(\s*"([^"]+)")re");
    struct site {
        std::string_view path;
        std::uint32_t offset;
    };
    std::map<std::string, std::vector<site>> owners;
    for (const auto path : sorted_sources(v, shell_root)) {
        const auto [text, exact] = code_of(v.read_file(path).value_or(""));
        const auto at = [&](std::size_t offset) {
            return static_cast<std::uint32_t>(exact ? offset : 0);
        };
        for (const auto& m : construction.find_all(text)) {
            const std::string name(m.group(1));
            owners[name].push_back({path, at(m.start())});
            if (m.group(2) == ",")
                ops::report(out, path, at(m.start()), 0, "cli::Menu(\"" +
                    name + "\", ...) passes a second argument, so the shell "
                    "reads back a prompt that is not the name",
                    "construct the menu from its name alone");
        }
        for (const auto& m : root_command.find_all(text))
            owners[std::string(m.group(1))].push_back({path, at(m.start())});
    }
    for (const auto& [name, sites] : owners) {
        if (sites.size() < 2)
            continue;
        std::string where;
        for (const auto& s : sites) {
            if (!where.empty())
                where += ", ";
            where += s.path;
        }
        ops::report(out, sites[1].path, sites[1].offset, 1, name +
            " is owned by " + std::to_string(sites.size()) + " units: " +
            where, "give each menu and root command its own name");
    }
}

constexpr std::array<ct::rule, 1> composition_rules{{
    {"unregistered_unit", "Every generated shell command unit of a checked "
        "component is registered by an application source."}}};

constexpr std::array<std::string_view, 2> composition_components{"refdata",
    "reporting"};

void run_composition(const ct::corpus_view& v, ct::diagnostic_table& out) {
    static const pt::regex registration(R"(\b(\w+)_commands::register_commands\s*\()");
    std::set<std::string> composed;
    for (const auto p : v.list_files("projects/ores.shell/application/src/",
             ".cpp")) {
        for (const auto& m : registration.find_all(
                 ops::without_comments(v.read_file(p).value_or(""))))
            composed.insert(std::string(m.group(1)) + "_commands");
    }
    for (const auto c : composition_components) {
        const auto dir = std::string(shell_root) + std::string(c) +
            "/include/ores.shell/app/commands/" + std::string(c) + "/";
        for (const auto h : v.list_files(dir, "_commands.hpp")) {
            if (h.find('/', dir.size()) != std::string_view::npos)
                continue;
            const auto text = v.read_file(h).value_or("");
            if (text.find("AUTO-GENERATED FILE") == std::string::npos)
                continue;
            const std::string unit(ops::stem_of(h));
            if (composed.contains(unit))
                continue;
            ops::report(out, h, 0, 0, std::string(c) + ": the generated "
                "command unit " + unit + " is not registered",
                "call its register_commands() from a composite or from the "
                "REPL");
        }
    }
}

}

const ct::descriptor shell_command_units{
    .name = "shell_command_units",
    .version = 1,
    .summary = "Reports a shell menu built twice or by two files, and a "
        "command unit registered twice.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = unit_rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run_units}};

const ct::descriptor shell_menus{
    .name = "shell_menus",
    .version = 1,
    .summary = "Reports a shell menu built with a prompt, and a root name "
        "two units own.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = menu_rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run_menus}};

const ct::descriptor shell_command_composition{
    .name = "shell_command_composition",
    .version = 1,
    .summary = "Reports each generated shell command unit no application "
        "source registers.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = composition_rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run_composition}};

}
