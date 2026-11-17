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
#include "compass.docs/codecs/prototypes.hpp"

#include <array>
#include <map>
#include <optional>
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

enum rule : std::uint32_t {
    unlisted, missing, no_group_page, unlinked_group, missing_group,
    review_without_index, review_cell, copy_differs
};

constexpr std::array<std::string_view, 2> record_sources{
    "708C2462-C1F8-455B-B091-9D9690FEEAAB",
    "D913FE21-6CF6-439A-89CF-6A1928AFA094"};

constexpr std::array<std::string_view, 1> inventory_sources{
    "D913FE21-6CF6-439A-89CF-6A1928AFA094"};

constexpr std::array<ct::rule, 8> rules{{
    {"unlisted", "Every prototype is listed on its group's page.",
        record_sources},
    {"missing", "Every prototype a group page lists exists.", inventory_sources},
    {"no_group_page", "A group that holds prototypes has a page.",
        record_sources},
    {"unlinked_group", "The top page links every group page.",
        inventory_sources},
    {"missing_group", "Every group the top page links has a page.",
        inventory_sources},
    {"review_without_index", "A review directory lists its screens on an "
        "index.html."},
    {"review_cell", "A prototype's row links its review, or says none in "
        "its review cell."},
    {"copy_differs", "The copies of a file a group shares are identical, "
        "save one that may add rules after the shared text."}}};

constexpr std::string_view base = "doc/prototypes/";

using pair = std::pair<std::string, std::string>;

/**
 * @brief The files a group shares by copy, each mapped to the one
 * prototype whose copy may add rules after the shared text, or none.
 */
const std::map<std::string, std::vector<std::pair<std::string,
    std::optional<std::string>>>>& shared_copies() {
    static const std::map<std::string, std::vector<std::pair<std::string,
        std::optional<std::string>>>> out{
        {"trades", {{"common.js", std::nullopt},
            {"prototype.css", "blotter"}}},
        {"conventions", {{"management.js", std::nullopt},
            {"management.css", std::nullopt}}}};
    return out;
}

std::string read(const ct::corpus_view& v, std::string_view path) {
    return pt::universal_newlines(pt::decode_replacing(
        v.read_file(path).value_or("")));
}

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    static const pt::regex prototype_link(R"(\[\[file:([a-z0-9][a-z0-9-]*)/)");
    static const pt::regex group_link(R"(\[\[file:([a-z0-9][a-z0-9_]*)/)");
    const auto all = v.list_files(base);
    const auto report = [&](rule r, std::string_view path,
        const std::string& message) {
        co::report(out, path, 0, r, message, "Keep the prototype inventory, "
            "its group pages and the tree in step.");
    };

    std::set<pair> present;
    std::set<pair> reviewed;
    std::set<std::string> pages;
    for (const auto p : all) {
        const auto parts = co::components_of(p);
        if (parts.size() == 5 && parts[4] == "index.html")
            present.emplace(parts[2], parts[3]);
        if (parts.size() >= 5 && parts[4] == "review")
            reviewed.emplace(parts[2], parts[3]);
        if (parts.size() == 4 && parts[3] == "index.org")
            pages.emplace(parts[2]);
    }
    std::set<pair> listed;
    std::map<pair, std::string> rows;
    for (const auto& g : pages) {
        const auto text = read(v, std::string(base) + g + "/index.org");
        for (const auto& m : prototype_link.find_all(text))
            listed.emplace(g, m.group(1));
        for (const auto line : co::split_lines(text)) {
            for (const auto& m : prototype_link.find_all(line))
                rows[{g, std::string(m.group(1))}] = std::string(line);
        }
    }
    std::set<std::string> linked;
    const auto top = read(v, "doc/prototypes/index.org");
    for (const auto& m : group_link.find_all(top))
        linked.emplace(m.group(1));

    for (const auto& [g, n] : present) {
        if (!listed.contains({g, n}))
            report(unlisted, std::string(base) + g + "/" + n + "/index.html",
                "doc/prototypes/" + g + "/" + n + "/index.html is not listed "
                "on doc/prototypes/" + g + "/index.org");
    }
    for (const auto& [g, n] : listed) {
        if (!present.contains({g, n}))
            report(missing, std::string(base) + g + "/index.org",
                "doc/prototypes/" + g + "/index.org lists " + n + ", but "
                "doc/prototypes/" + g + "/" + n + "/index.html does not "
                "exist");
    }
    std::set<std::string> groups;
    for (const auto& [g, n] : present)
        groups.insert(g);
    for (const auto& g : groups) {
        if (!pages.contains(g))
            report(no_group_page, std::string(base) + g, "doc/prototypes/" +
                g + "/ holds prototypes but has no index.org: the group "
                "needs a page");
    }
    for (const auto& g : pages) {
        if (!linked.contains(g))
            report(unlinked_group, std::string(base) + g + "/index.org",
                "doc/prototypes/" + g + "/index.org is not linked from "
                "doc/prototypes/index.org: the top page must name every "
                "group");
    }
    for (const auto& g : linked) {
        if (!pages.contains(g))
            report(missing_group, "doc/prototypes/index.org",
                "doc/prototypes/index.org links the group " + g + ", but "
                "doc/prototypes/" + g + "/index.org does not exist");
    }
    for (const auto& [g, n] : reviewed) {
        const auto index = std::string(base) + g + "/" + n +
            "/review/index.html";
        if (!v.file_exists(index))
            report(review_without_index, std::string(base) + g + "/" + n +
                "/review", "doc/prototypes/" + g + "/" + n + "/review/ has "
                "no index.html: a review lists its screens on one page");
    }
    for (const auto& [g, n] : present) {
        const auto row = rows.find({g, n});
        if (row == rows.end())
            continue;
        const auto page = std::string(base) + g + "/index.org";
        if (reviewed.contains({g, n})) {
            if (!row->second.contains("[[file:" + n + "/review/"))
                report(review_cell, page, "doc/prototypes/" + g + "/index.org "
                    "lists " + n + " without its review, though the review "
                    "directory exists");
        } else {
            auto trimmed = std::string_view(row->second);
            while (!trimmed.empty() && co::is_space(trimmed.back()))
                trimmed.remove_suffix(1);
            if (!trimmed.ends_with("| none |"))
                report(review_cell, page, "doc/prototypes/" + g + "/index.org "
                    "lists " + n + " without a review, so its review cell "
                    "must say none: an unreviewed prototype is found by that "
                    "row");
        }
    }
    for (const auto& [group, files] : shared_copies()) {
        std::vector<std::string> folders;
        for (const auto& [g, n] : present) {
            if (g == group)
                folders.push_back(n);
        }
        for (const auto& [file, extender] : files) {
            std::vector<std::pair<std::string, std::string>> plain;
            std::optional<std::string> extended;
            for (const auto& f : folders) {
                const auto path = std::string(base) + group + "/" + f + "/" +
                    file;
                if (!v.file_exists(path))
                    continue;
                auto text = read(v, path);
                if (extender && f == *extender)
                    extended = std::move(text);
                else
                    plain.emplace_back(f, std::move(text));
            }
            if (plain.empty())
                continue;
            const auto& [reference_name, reference] = plain.front();
            for (const auto& [name, text] : plain) {
                if (text != reference)
                    report(copy_differs, std::string(base) + group + "/" +
                        name + "/" + file, "doc/prototypes/" + group + "/" +
                        name + "/" + file + " differs from doc/prototypes/" +
                        group + "/" + reference_name + "/" + file + ": the "
                        "group shares this file by copy, so the copies must "
                        "be identical");
            }
            if (extended && !extended->starts_with(reference))
                report(copy_differs, std::string(base) + group + "/" +
                    *extender + "/" + file, "doc/prototypes/" + group + "/" +
                    *extender + "/" + file + " does not begin with the shared "
                    + file + ": it may add rules after the shared text, and "
                    "may not change it");
        }
    }
}

}

const ct::descriptor prototypes{
    .name = "prototypes",
    .version = 2,
    .summary = "Reports each way the prototype inventory and its pages "
        "disagree with the tree.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}
