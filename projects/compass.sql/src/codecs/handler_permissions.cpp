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
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.platform/text/regex.hpp"
#include "compass.sql/ops/scripts.hpp"

namespace compass::sql::codecs {

namespace {

namespace ct = compass::codec::types;
namespace pt = compass::platform::text;

constexpr std::array<std::string_view, 1> unseeded_permission_sources{
    "7B477025-132E-4B20-80B4-D8C9420880B8"};

constexpr std::array<ct::rule, 1> rules{{
    {"unseeded_permission", "Every permission code a handler of a checked "
        "component checks is seeded.", unseeded_permission_sources}}};

constexpr std::array<ct::option, 2> options{{
    {"components", "refdata,analytics,compute,assets,storage,reporting,dq,"
        "trading,marketdata,inbox",
        "The components, by bare name, whose handler permission codes are "
        "all seeded, comma-separated."},
    {"seeds", "projects/ores.sql/populate/iam/iam_permissions_populate.sql",
        "The populate script that seeds the permission codes."}}};

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    static const pt::regex seed(
        R"('(?P<code>[a-z_]+::[a-z_]+:[a-z_-]+)')");
    static const pt::regex permission(
        R"re(has_permission\(\s*[^,]+,\s*"([^"]+)"(\s*\+)?)re");
    std::set<std::string, std::less<>> seeded;
    for (const auto& m : seed.find_all(
             v.read_file(v.option("seeds")).value_or("")))
        seeded.emplace(m.group("code"));

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
        for (const auto p : paths) {
            const auto text = v.read_file(p).value_or("");
            // A code built at run time, "prefix" + name, is its prefix and
            // a *; a seed that starts with the prefix satisfies it.
            std::map<std::string, std::size_t> codes;
            for (const auto& m : permission.find_all(text))
                codes.try_emplace(std::string(m.group(1)) +
                    (m.matched(2) ? "*" : ""), m.start());
            for (const auto& [code, offset] : codes) {
                if (seeded.contains(code))
                    continue;
                if (code.ends_with('*')) {
                    const auto prefix = std::string_view(code).substr(0,
                        code.size() - 1);
                    if (std::ranges::any_of(seeded, [&](const auto& s) {
                            return s.starts_with(prefix); }))
                        continue;
                }
                out.add({.document = compass::org::types::document_id::none(),
                    .range = {static_cast<std::uint32_t>(offset),
                        static_cast<std::uint32_t>(offset)},
                    .rule = ct::rule_id{0},
                    .level = ct::severity::error,
                    .message = code + " is checked but never seeded",
                    .fix = "seed the code in " + std::string(
                        v.option("seeds")),
                    .file = p});
            }
        }
    }
}

}

const ct::descriptor handler_permissions{
    .name = "handler_permissions",
    .version = 1,
    .summary = "Reports each permission code a handler checks that no "
        "populate script seeds.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}
