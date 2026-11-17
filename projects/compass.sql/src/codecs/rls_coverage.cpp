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
#include "compass.sql/codecs/rls_coverage.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"
#include "compass.sql/ops/er_model.hpp"

namespace compass::sql::codecs {

namespace {

namespace ct = compass::codec::types;
namespace pt = compass::platform::text;

constexpr std::array<std::string_view, 1> sources{
    "46AC210B-6BDF-49D2-BA47-35404AAEBA18"};

constexpr std::array<ct::rule, 2> rules{{
    {"missing_rls", "A table with a tenant_id column enables row-level "
        "security in a create script (RLS_001).", sources},
    {"missing_party_isolation", "A table with a party_id column has a "
        "restrictive policy on party_id in a create script (RLS_002).",
        sources}}};

constexpr std::array<ct::option, 2> options{{
    {"create_dir", "projects/ores.sql/create",
        "The directory whose *_create.sql scripts define the tables and "
        "their policies."},
    {"ignore_file", "projects/ores.sql/utility/validation_ignore.txt",
        "The script's ignore file: lines of a code pattern and a table "
        "pattern; a missing file ignores nothing."}}};

/**
 * @brief Whether @p name matches @p pattern as Python's fnmatch.fnmatchcase
 * decides it: * any run, ? one character, [...] a set, [!...] its
 * complement, anything else itself.
 */
bool fnmatch(std::string_view pattern, std::string_view name) {
    if (pattern.empty())
        return name.empty();
    const auto c = pattern.front();
    if (c == '*') {
        for (std::size_t i = 0; i <= name.size(); ++i) {
            if (fnmatch(pattern.substr(1), name.substr(i)))
                return true;
        }
        return false;
    }
    if (name.empty())
        return false;
    if (c == '[') {
        std::size_t j = 1;
        if (j < pattern.size() && pattern[j] == '!')
            ++j;
        if (j < pattern.size() && pattern[j] == ']')
            ++j;
        if (const auto close = pattern.find(']', j);
            close != std::string_view::npos) {
            auto set = pattern.substr(1, close - 1);
            const bool negated = set.starts_with('!');
            if (negated)
                set.remove_prefix(1);
            bool in = false;
            for (std::size_t i = 0; i < set.size(); ++i) {
                if (i + 2 < set.size() && set[i + 1] == '-') {
                    in = in || (set[i] <= name[0] && name[0] <= set[i + 2]);
                    i += 2;
                } else {
                    in = in || set[i] == name[0];
                }
            }
            return in != negated &&
                fnmatch(pattern.substr(close + 1), name.substr(1));
        }
    }
    if (c != '?' && c != name.front())
        return false;
    return fnmatch(pattern.substr(1), name.substr(1));
}

/**
 * @brief The rules of the ignore file @p text: its lines that are neither
 * blank nor comments, split at the first run of white space.
 */
std::vector<std::pair<std::string, std::string>> ignore_rules(
    std::string_view text) {
    std::vector<std::pair<std::string, std::string>> out;
    constexpr std::string_view space = " \t\r\f\v";
    while (!text.empty()) {
        const auto end = text.find('\n');
        auto line = text.substr(0, end);
        text = end == std::string_view::npos ? std::string_view{}
                                             : text.substr(end + 1);
        const auto first = line.find_first_not_of(space);
        if (first == std::string_view::npos)
            continue;
        line = line.substr(first, line.find_last_not_of(space) + 1 - first);
        if (line.starts_with('#'))
            continue;
        const auto gap = line.find_first_of(space);
        if (gap == std::string_view::npos)
            continue;
        const auto rest = line.substr(line.find_first_not_of(space, gap));
        out.emplace_back(line.substr(0, gap), rest);
    }
    return out;
}

/**
 * @brief The byte offset of the start of the 1-based @p line of @p bytes.
 */
std::uint32_t line_start(std::string_view bytes, std::uint32_t line) {
    std::size_t at = 0;
    for (std::uint32_t l = 1; l < line && at != std::string_view::npos; ++l) {
        at = bytes.find('\n', at);
        if (at != std::string_view::npos)
            ++at;
    }
    return static_cast<std::uint32_t>(at == std::string_view::npos
        ? bytes.size() : at);
}

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    static const pt::regex enable(
        R"re(alter\s+table\s+"?(\w+)"?\s+enable\s+row\s+level\s+security)re",
        {.ignorecase = true});
    static const pt::regex restrictive(
        R"re(create\s+policy\s+\S+\s+on\s+"?(\w+)"?)re"
        R"re((?:(?!;).)*?as\s+restrictive(?:(?!;).)*?party_id)re",
        {.dotall = true, .ignorecase = true});

    auto paths = v.list_files(std::string(v.option("create_dir")) + "/",
        "_create.sql");
    // The script reads them in pathlib's order, part by part, which decides
    // which definition of a table defined twice is the one kept.
    std::ranges::sort(paths, [](std::string_view x, std::string_view y) {
        while (!x.empty() && !y.empty()) {
            const auto a = x.substr(0, x.find('/'));
            const auto b = y.substr(0, y.find('/'));
            if (a != b)
                return a < b;
            x.remove_prefix(std::min(x.size(), a.size() + 1));
            y.remove_prefix(std::min(y.size(), b.size() + 1));
        }
        return x.empty() && !y.empty();
    });
    std::vector<std::string> texts;
    texts.reserve(paths.size());
    std::vector<ops::er_script> scripts;
    std::set<std::string, std::less<>> enabled;
    std::set<std::string, std::less<>> isolated;
    for (const auto p : paths) {
        const auto bytes = v.read_file(p);
        if (!bytes)
            continue;
        texts.push_back(pt::universal_newlines(pt::decode_replacing(*bytes)));
        scripts.push_back({p, texts.back()});
        for (const auto& m : enable.find_all(texts.back()))
            enabled.emplace(m.group(1));
        for (const auto& m : restrictive.find_all(texts.back()))
            isolated.emplace(m.group(1));
    }
    const auto model = ops::er_model_of(scripts, {});
    const auto ignores = ignore_rules(
        v.read_file(v.option("ignore_file")).value_or(""));
    const auto ignored = [&](std::string_view code, std::string_view table) {
        return std::ranges::any_of(ignores, [&](const auto& r) {
            return fnmatch(r.first, code) && fnmatch(r.second, table);
        });
    };

    for (const auto& t : model.tables) {
        const auto has = [&](std::string_view column) {
            return std::ranges::contains(t.columns, column);
        };
        if (!has("tenant_id"))
            continue;
        const auto at = line_start(v.read_file(t.path).value_or(""), t.line);
        const auto report = [&](std::uint32_t rule, std::string message,
            std::string fix) {
            out.add({.document = compass::org::types::document_id::none(),
                .range = {at, at},
                .rule = ct::rule_id{rule},
                .level = ct::severity::error,
                .message = std::move(message),
                .fix = std::move(fix),
                .file = t.path});
        };
        if (!enabled.contains(t.name) && !ignored("RLS_001", t.name))
            report(0, "Table '" + t.name + "' has tenant_id but no ENABLE "
                "ROW LEVEL SECURITY found in any *_create.sql file",
                "enable row level security on the table and give it a "
                "tenant isolation policy, or list it under RLS_001 in the "
                "ignore file with its reason");
        if (has("party_id") && !isolated.contains(t.name) &&
            !ignored("RLS_002", t.name))
            report(1, "Table '" + t.name + "' has party_id but no AS "
                "RESTRICTIVE party isolation policy found in any "
                "*_create.sql file",
                "add a restrictive policy on party_id, or list the table "
                "under RLS_002 in the ignore file with its reason");
    }
}

}

const ct::descriptor rls_coverage{
    .name = "rls_coverage",
    .version = 1,
    .summary = "Reports each table with tenant_id that does not enable "
        "row-level security, and each with party_id that has no "
        "restrictive party policy, as the er-diagram gate does.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}
