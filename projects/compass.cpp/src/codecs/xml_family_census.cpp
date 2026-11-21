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
#include "compass.cpp/codecs/xml_family_census.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::cpp::codecs {

namespace {

namespace ct = compass::codec::types;
namespace pt = compass::platform::text;

constexpr std::array<ct::rule, 5> rules{{
    {"unclassified_family", "Every root element of the ORE example corpus "
        "is a classified family."},
    {"family_not_in_corpus", "Every classified family is the root of some "
        "corpus document."},
    {"state_disagrees", "A supported family has a generated table, and a "
        "migrating or unused one has none."},
    {"suite_names_unused", "No XML family suite names an unused family."},
    {"bad_family", "Every family reads name=state, the state supported, "
        "migrating or unused."}}};

constexpr std::array<ct::option, 3> options{{
    {"families",
        "CalendarAdjustments=supported,Conventions=supported,"
        "CounterpartyInformation=supported,CurrencyConfig=supported,"
        "IborFallbackConfig=supported,NettingSetDefinitions=supported,"
        "TodaysMarket=supported,CreditSimulation=supported,"
        "PricingEngines=supported,ORE=supported,Portfolio=supported,"
        "CurveConfiguration=supported,StressTesting=supported,"
        "Simulation=unused,SensitivityAnalysis=unused,ReferenceData=unused,"
        "ScriptLibrary=unused,CollateralBalances=unused,"
        "SIMMCalibrationData=unused,CrossAssetModel=unused",
        "Each ORE document family and its state, as name=state, "
        "comma-separated: which families the codebase reads."},
    {"corpus", "external/ore/examples",
        "The ORE example corpus whose root elements are the families."},
    {"generated", "projects/ores.ore/core/src/xml/generated",
        "The generated codec tables, which bind the supported families."}}};

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    std::map<std::string, std::string, std::less<>> families;
    const auto list = v.option("families");
    for (std::size_t start = 0; start < list.size();) {
        auto end = list.find(',', start);
        if (end == std::string_view::npos)
            end = list.size();
        const auto entry = list.substr(start, end - start);
        start = end + 1;
        const auto eq = entry.find('=');
        const auto state = eq == std::string_view::npos ? std::string_view{}
            : entry.substr(eq + 1);
        if (state != "supported" && state != "migrating" &&
            state != "unused") {
            ops::report(out, ".compass.org", 0, 4, "the family " +
                std::string(entry) + " is not name=state",
                "write supported, migrating or unused after the name");
            return;
        }
        families[std::string(entry.substr(0, eq))] = std::string(state);
    }

    static const pt::regex comment(R"(<!--.*?-->)", {.dotall = true});
    static const pt::regex instruction(R"(<\?.*?\?>)", {.dotall = true});
    static const pt::regex element(R"(<([A-Za-z_][\w.:-]*))");
    const auto corpus = std::string(v.option("corpus"));
    std::map<std::string, std::string_view> roots;
    for (const auto p : v.list_files(corpus + "/", ".xml")) {
        auto text = pt::decode_replacing(v.read_file(p).value_or(""));
        text = instruction.substitute(comment.substitute(text, ""), "");
        if (const auto m = element.search(text))
            roots.try_emplace(std::string(m->group(1)), p);
    }

    static const pt::regex binding(R"re(document_type doc\{"([A-Za-z_][\w.:-]*)")re");
    const auto generated = std::string(v.option("generated")) + "/";
    std::set<std::string> tabled;
    for (const auto p : v.list_files(generated, ".cpp")) {
        if (p.find('/', generated.size()) != std::string_view::npos)
            continue;
        // Named, because a range-for keeps alive only the outermost
        // temporary before C++23's P2718, which MSVC lacks, and the matches
        // view this text.
        const auto text = v.read_file(p).value_or("");
        for (const auto& m : binding.find_all(text))
            tabled.emplace(m.group(1));
    }

    for (const auto& [name, first] : roots) {
        if (!families.contains(name))
            ops::report(out, first, 0, 0, name + " is a corpus root element "
                "that nobody classified", "add it to the families option "
                "with its state");
    }
    for (const auto& [name, state] : families) {
        if (!roots.contains(name))
            ops::report(out, corpus, 0, 1, name + " is classified but the "
                "corpus carries no document with that root",
                "remove the family, or restore its documents");
        const bool bound = tabled.contains(name);
        if (state == "supported" && !bound)
            ops::report(out, generated.substr(0, generated.size() - 1), 0, 2,
                name + " is called supported but no generated table binds it",
                "generate its table, or change its state");
        if (state != "supported" && bound)
            ops::report(out, generated.substr(0, generated.size() - 1), 0, 2,
                name + " is called " + state + " but a table binds it",
                "call it supported");
    }

    static const pt::regex suite(R"((^|/)xml_\w+_tests\.cpp$)");
    std::vector<std::pair<std::string, pt::regex>> unused;
    for (const auto& [name, state] : families) {
        if (state == "unused")
            unused.emplace_back(name, pt::regex(R"(\b)" + pt::escape(name) +
                R"(\b)"));
    }
    for (const auto p : v.list_files("projects/", "_tests.cpp")) {
        const auto parts = ops::components_of(p);
        if (parts.size() < 3 ||
            std::find(parts.begin() + 2, parts.end() - 1, "tests") ==
                parts.end() - 1 || !suite.search(p))
            continue;
        const auto text = v.read_file(p).value_or("");
        for (const auto& [name, pattern] : unused) {
            if (const auto m = pattern.search(text))
                ops::report(out, p, static_cast<std::uint32_t>(m->start()), 3,
                    "names " + name + ", which is called unused: a test would "
                    "prove a capability no feature has", "remove the case, or "
                    "change the family's state");
        }
    }
}

}

const ct::descriptor xml_family_census{
    .name = "xml_family_census",
    .version = 1,
    .summary = "Reports where the ORE document family census and the tree "
        "disagree.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}
