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
#include "compass.sql/codecs/handler_permissions.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <span>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codec/types/fact.hpp"
#include "compass.platform/text/regex.hpp"
#include "compass.sql/ops/lexer.hpp"

namespace compass::sql::codecs {

namespace {

namespace ct = compass::codec::types;
namespace pt = compass::platform::text;

constexpr std::array<std::string_view, 1> unseeded_permission_sources{
    "7B477025-132E-4B20-80B4-D8C9420880B8"};

constexpr std::array<ct::rule, 1> rules{{
    {"unseeded_permission", "Every permission code a handler of a checked "
        "component checks is seeded.", unseeded_permission_sources,
        ct::blame_method::line}}};

constexpr std::array<ct::option, 2> declared_options{{
    {"components", "refdata,analytics,compute,assets,storage,reporting,dq,"
        "trading,marketdata,inbox",
        "The components, by bare name, whose handler permission codes are "
        "all seeded, comma-separated."},
    {"seeds", "projects/ores.sql/populate/iam/iam_permissions_populate.sql",
        "The populate script that seeds the permission codes."}}};

/**
 * @brief Whether @p s has the shape of a permission code,
 * [a-z_]+::[a-z_]+:[a-z_-]+.
 */
bool is_code(std::string_view s) {
    const auto part = [](std::string_view p, bool dash) {
        return !p.empty() && std::ranges::all_of(p, [&](char c) {
            return (c >= 'a' && c <= 'z') || c == '_' || (dash && c == '-');
        });
    };
    const auto scope = s.find("::");
    if (scope == std::string_view::npos)
        return false;
    const auto rest = s.substr(scope + 2);
    const auto colon = rest.find(':');
    return colon != std::string_view::npos &&
        part(s.substr(0, scope), false) &&
        part(rest.substr(0, colon), false) &&
        part(rest.substr(colon + 1), true);
}

/**
 * @brief Each string literal of the code of @p text, in dollar bodies too,
 * whose value has the shape of a permission code.
 */
void seeded_codes(std::string_view text,
    const std::vector<ops::token>& tokens, ct::fact_writer& fact) {
    for (const auto& t : tokens) {
        if (t.kind == ops::token_kind::dollar_body) {
            const auto content = ops::dollar_content(ops::text_of(text, t));
            const auto base = static_cast<std::uint32_t>(
                content.data() - text.data());
            seeded_codes(text, ops::lex(content, base, false).tokens, fact);
        } else if (t.kind == ops::token_kind::string) {
            const auto value = ops::string_value(ops::text_of(text, t));
            if (is_code(value))
                fact.text(value);
        }
    }
}

std::vector<std::string_view> scanned(const ct::corpus_view& v) {
    std::vector<std::string_view> out;
    const auto seeds = v.option("seeds");
    for (const auto p : v.list_files(seeds)) {
        if (p == seeds)
            out.push_back(p);
    }
    const auto list = v.option("components");
    for (std::size_t start = 0; start < list.size();) {
        auto end = list.find(',', start);
        if (end == std::string_view::npos)
            end = list.size();
        const auto root = "projects/ores." +
            std::string(list.substr(start, end - start)) + "/";
        start = end + 1;
        std::vector<std::string_view> paths;
        for (const auto suffix : {"_handler.hpp", "_handler.cpp"}) {
            for (const auto p : v.list_files(root, suffix)) {
                const auto slash = p.rfind('/');
                if (p.substr(0, slash).ends_with("/messaging"))
                    paths.push_back(p);
            }
        }
        std::ranges::sort(paths);
        out.insert(out.end(), paths.begin(), paths.end());
    }
    return out;
}

constexpr std::uint32_t seeds_fact = 0;
constexpr std::uint32_t handler_fact = 1;

/**
 * @brief The fact of the seeds script: each code it seeds. Of a handler:
 * each code it checks, with the offset of its first check. A code built
 * at run time, "prefix" + name, is its prefix and a *.
 */
void map(std::string_view path, std::string_view bytes,
    const ct::option_values& options, ct::file_scan& out) {
    static const pt::regex permission(
        R"re(has_permission\(\s*[^,]+,\s*"([^"]+)"(\s*\+)?)re");
    ct::fact_writer fact(out.fact);
    if (path == options["seeds"]) {
        fact.number(seeds_fact);
        seeded_codes(bytes, ops::lex(bytes).tokens, fact);
        return;
    }
    fact.number(handler_fact);
    std::map<std::string, std::size_t> codes;
    for (const auto& m : permission.find_all(bytes))
        codes.try_emplace(std::string(m.group(1)) +
            (m.matched(2) ? "*" : ""), m.start());
    for (const auto& [code, offset] : codes)
        fact.text(code).number(static_cast<std::uint32_t>(offset));
}

void reduce(std::span<const ct::scanned_file> files,
    const ct::corpus_view& v, ct::diagnostic_table& out) {
    std::set<std::string, std::less<>> seeded;
    for (const auto& f : files) {
        ct::fact_reader fact(f.fact);
        if (fact.number() != seeds_fact)
            continue;
        while (!fact.done())
            seeded.emplace(fact.text());
    }
    for (const auto& f : files) {
        ct::fact_reader fact(f.fact);
        if (fact.number() != handler_fact)
            continue;
        while (!fact.done()) {
            const auto code = fact.text();
            const auto offset = fact.number();
            if (seeded.contains(code))
                continue;
            if (code.ends_with('*')) {
                const auto prefix = code.substr(0, code.size() - 1);
                const auto it = seeded.lower_bound(prefix);
                if (it != seeded.end() && it->starts_with(prefix))
                    continue;
            }
            out.add({.document = compass::org::types::document_id::none(),
                .range = {offset, offset},
                .rule = ct::rule_id{0},
                .level = ct::severity::error,
                .message = std::string(code) + " is checked but never seeded",
                .fix = "seed the code in " + std::string(v.option("seeds")),
                .file = f.path});
        }
    }
}

}

const ct::descriptor handler_permissions{
    .name = "handler_permissions",
    .version = 2,
    .summary = "Reports each permission code a handler checks that no "
        "populate script seeds.",
    .reads = ct::inputs::of_types({}),
    .options = declared_options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::scan_function{scanned, map, reduce}};

}
