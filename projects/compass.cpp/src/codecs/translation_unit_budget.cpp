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
#include "compass.cpp/codecs/translation_unit_budget.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <map>
#include <set>
#include <string>
#include <system_error>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/lexer.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/source_model.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::cpp::codecs {

namespace {

namespace ct = compass::codec::types;
namespace pt = compass::platform::text;

constexpr std::array<ct::rule, 3> rules{{
    {"over_budget", "No reflecting unit names more convention families than "
        "its budget: one, or the ceiling recorded for it."},
    {"below_ceiling", "A recorded ceiling is what its unit reflects, so it "
        "only comes down."},
    {"bad_ceiling", "Every recorded ceiling reads path=count.",
        {}, ct::blame_method::line}}};

constexpr std::array<ct::option, 3> options{{
    {"registry", "projects/ores.refdata/api/include/ores.refdata.api/domain/"
        "convention_family_registry.hpp",
        "The header that names every convention family's value type."},
    {"scopes", "projects/ores.refdata/core/src,projects/ores.refdata/core/"
        "include,projects/ores.ore/core/src,projects/ores.ore/core/include,"
        "projects/ores.ore/service/src",
        "The directories whose units are measured, comma-separated."},
    {"ceilings",
        "projects/ores.ore/core/src/domain/"
        "conventions_family_tenor_basis_swap.cpp=3,"
        "projects/ores.ore/core/src/domain/"
        "conventions_family_commodity_future.cpp=6,"
        "projects/ores.ore/core/src/domain/"
        "conventions_family_cross_currency_basis.cpp=4,"
        "projects/ores.ore/core/src/domain/"
        "conventions_family_cross_currency_fix_float.cpp=4",
        "The units allowed more than one family, as path=count, "
        "comma-separated; each family of one is one document element."}}};

constexpr std::size_t budget = 1;
constexpr std::array<std::string_view, 3> suffixes{".cpp", ".hpp", ".inc"};
constexpr std::string_view reflecting_header = "conventions_family_pack.hpp";

std::vector<std::string_view> split(std::string_view list) {
    std::vector<std::string_view> out;
    for (std::size_t start = 0; start <= list.size();) {
        auto end = list.find(',', start);
        if (end == std::string_view::npos)
            end = list.size();
        if (end > start)
            out.push_back(list.substr(start, end - start));
        start = end + 1;
    }
    return out;
}

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    static const pt::regex domain_type(
        R"re("ores::refdata::domain::([a-z_0-9]+)")re");
    std::map<std::string, std::size_t, std::less<>> ceilings;
    for (const auto entry : split(v.option("ceilings"))) {
        const auto eq = entry.rfind('=');
        std::size_t n = 0;
        if (eq == std::string_view::npos || std::from_chars(
                entry.data() + eq + 1, entry.data() + entry.size(), n).ec !=
                std::errc{}) {
            ops::report(out, ".compass.org", 0, 2, "the ceiling " +
                std::string(entry) + " is not path=count",
                "write each ceiling as path=count");
            return;
        }
        ceilings[std::string(entry.substr(0, eq))] = n;
    }

    const auto registry = v.read_file(v.option("registry"));
    std::set<std::string, std::less<>> names;
    if (registry) {
        for (const auto& m : domain_type.find_all(*registry))
            names.emplace(m.group(1));
    }
    std::map<std::string, std::size_t, std::less<>> counts;
    for (const auto scope : split(v.option("scopes"))) {
        const auto base = std::string(scope) + "/";
        for (const auto suffix : suffixes) {
            for (const auto p : v.list_files(base, suffix)) {
                const auto bytes = v.read_file(p).value_or("");
                if (bytes.find(reflecting_header) == std::string::npos)
                    continue;
                const auto lexed = ops::lex(bytes);
                const auto model = ops::model_of(bytes, lexed,
                    ops::model_parts::names);
                if (std::ranges::none_of(model.includes, [](const auto& i) {
                        return !i.angled &&
                            i.target.ends_with(reflecting_header); }))
                    continue;
                std::set<std::string_view> named;
                for (const auto& n : model.names) {
                    const auto parts = ops::components_of_name(n.text);
                    for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
                        if (parts[i] == "domain" &&
                            names.contains(parts[i + 1]))
                            named.insert(parts[i + 1]);
                    }
                }
                if (!named.empty())
                    counts[std::string(p)] = named.size();
            }
        }
    }

    for (const auto& [path, count] : counts) {
        const auto known = ceilings.find(path);
        const auto ceiling = known == ceilings.end() ? budget : known->second;
        if (count > ceiling)
            ops::report(out, path, 0, 0, "reflects " + std::to_string(count) +
                " convention families; its budget is " +
                std::to_string(ceiling), "move each family's serialisation "
                "into a unit of its own");
    }
    for (const auto& [path, ceiling] : ceilings) {
        const auto found = counts.find(path);
        const auto count = found == counts.end() ? 0 : found->second;
        if (count < ceiling)
            ops::report(out, path, 0, 1, "now reflects " +
                std::to_string(count) + " convention families, below its "
                "recorded ceiling of " + std::to_string(ceiling),
                "lower the ceiling or remove the entry");
    }
}

}

const ct::descriptor translation_unit_budget{
    .name = "translation_unit_budget",
    .version = 1,
    .summary = "Reports a unit that reflects more convention families than "
        "its budget, and a ceiling above what its unit reflects.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}
