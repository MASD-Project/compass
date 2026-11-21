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
#include "compass.cli/app/check_inventory.hpp"

#include <algorithm>
#include <format>
#include <map>
#include <vector>
#include "compass.codec/ops/data_tables.hpp"
#include "compass.codec/ops/exceptions.hpp"

namespace compass::cli::app {

namespace {

namespace ct = compass::codec::types;
namespace cops = compass::codec::ops;

std::string kind_of(const ct::descriptor& d) {
    if (d.formatting_check)
        return "formatting check";
    return ct::output_of(d) == ct::output_kind::files ? "generator" : "check";
}

bool enabled_in(const ct::configuration& settings, const ct::descriptor& d) {
    bool on = d.enabled_by_default;
    for (std::size_t i = 0; i < settings.size(); ++i) {
        if (settings.codecs[i] == d.name && settings.keys[i] == ct::enabled_key)
            on = settings.values[i] == "true";
    }
    return on;
}

/**
 * @brief @p text fit for a cell: a bar would end it, so it is \vert.
 */
std::string cell(std::string_view text) {
    std::string out;
    for (const char c : text) {
        if (c == '|')
            out += "\\vert{}";
        else if (c != '\n')
            out.push_back(c);
    }
    return out;
}

/**
 * @brief Where a codec is defined, as the page shows it.
 */
std::string source_of(std::string_view path, std::string_view id) {
    if (path.empty())
        return "built into compass";
    if (id.empty())
        return std::format("={}=", path);
    return std::format("[[id:{}][{}]]", id, path);
}

}

std::string inventory_of(std::span<const ct::descriptor> built_in,
    const config::check_set& checks, const ct::configuration& settings) {
    std::map<std::string, const config::codec_data*, std::less<>> data_of;
    for (const auto& d : checks.data)
        for (const auto& c : d.codecs)
            data_of.emplace(c, &d);

    cops::data_tables out{.source = "compass"};
    cops::data_table codecs{.name = "Codecs", .columns = {"name", "enabled",
        "kind", "severity", "summary", "path", "id"}};
    cops::data_table rules{.name = "Rules", .columns = {"codec", "rule",
        "statement"}};
    const auto add = [&](const ct::descriptor& d, std::string path,
                         std::string id) {
        codecs.rows.push_back({std::string(d.name),
            enabled_in(settings, d) ? "yes" : "no", kind_of(d),
            d.level == ct::severity::warning ? "warning" : "error",
            std::string(d.summary), std::move(path), std::move(id)});
        codecs.row_lines.push_back(0);
        for (const auto& r : d.rules) {
            rules.rows.push_back({std::string(d.name), std::string(r.name),
                std::string(r.summary)});
            rules.row_lines.push_back(0);
        }
        if (ct::output_of(d) != ct::output_kind::files || d.formatting_check)
            return;
        for (const auto* r : {&cops::drift_rule,
                 &cops::names_missing_file_rule}) {
            rules.rows.push_back({std::string(d.name), std::string(r->name),
                std::string(r->summary)});
            rules.row_lines.push_back(0);
        }
    };
    for (const auto& d : built_in) {
        const auto it = data_of.find(d.name);
        add(d, it == data_of.end() ? "" : it->second->path,
            it == data_of.end() ? "" : it->second->id);
    }
    for (const auto& c : checks.checks)
        add(c->codec, c->path, c->id);
    out.tables = {std::move(codecs), std::move(rules)};
    return cops::encode_data_tables(out);
}

std::string inventory_page(const ct::corpus_view& v) {
    const auto tables = cops::decode_data_tables(v.option(inventory_option));
    struct entry {
        std::vector<std::string> row;
        std::vector<std::pair<std::string, std::string>> rules;
    };
    std::vector<entry> entries;
    std::map<std::string, std::size_t, std::less<>> at;
    if (tables) {
        if (const auto* c = tables->find("Codecs")) {
            for (const auto& row : c->rows) {
                at.emplace(row[0], entries.size());
                entries.push_back({row, {}});
            }
        }
        if (const auto* r = tables->find("Rules")) {
            for (const auto& row : r->rows) {
                if (const auto it = at.find(row[0]); it != at.end())
                    entries[it->second].rules.emplace_back(row[1], row[2]);
            }
        }
    }
    std::ranges::stable_sort(entries, {}, [](const entry& e) {
        return std::tuple(e.row[1] != "yes", e.row[0]);
    });

    std::string out;
    if (const auto id = v.option("id"); !id.empty())
        out += std::format(":PROPERTIES:\n:ID: {}\n:END:\n", id);
    out += std::format("#+title: {}\n", v.option("title"));
    out += "#+description: Every check and generator compass runs here: "
           "whether it is on, what it is for, its rules, and where it is "
           "defined.\n";
    out += std::format("#+type: {}\n#+level: {}\n#+filetags: {}\n",
        v.option("type"), v.option("level"), v.option("filetags"));
    for (const auto key : {"created", "updated"}) {
        if (const auto date = v.option(key); !date.empty())
            out += std::format("#+{}: {}\n", key, date);
    }
    out += "\nThis page is generated by compass's check_inventory codec "
           "from the codecs and the check documents this repository's "
           "configuration gives it. Edit those and run compass generate, "
           "never the page; compass check reports the page when it no "
           "longer says what runs.\n";

    const auto on = std::ranges::count_if(entries, [](const entry& e) {
        return e.row[1] == "yes";
    });
    out += std::format("\n* Summary\n\n{} of the {} codecs and checks "
        "compass knows here run; the ones that do not are listed after "
        "them, as what could be turned on.\n\n", on, entries.size());
    out += "| Name | Enabled | Kind | Severity | Purpose | Defined in |\n"
           "|------+---------+------+----------+---------+------------|\n";
    for (const auto& e : entries) {
        const auto& r = e.row;
        out += std::format("| ={}= | {} | {} | {} | {} | {} |\n", r[0], r[1],
            r[2], r[3], cell(r[4]), source_of(r[5], r[6]));
    }

    out += "\n* Detail\n";
    for (const auto section : {"yes", "no"}) {
        out += std::format("\n** {}\n",
            std::string_view(section) == "yes" ? "Enabled" : "Disabled");
        for (const auto& e : entries) {
            const auto& r = e.row;
            if (r[1] != section)
                continue;
            out += std::format("\n*** {}\n\n{}\n\n- Kind: {}.\n- Severity: "
                "{}.\n- Defined in: {}.\n", r[0], r[4], r[2], r[3],
                source_of(r[5], r[6]));
            if (e.rules.empty())
                continue;
            out += "\n| Rule | Statement |\n|------+-----------|\n";
            for (const auto& [name, statement] : e.rules)
                out += std::format("| ={}= | {} |\n", name, cell(statement));
        }
    }
    return out;
}

}
