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
#include "compass.docs/codecs/component_docs.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codec/types/fact.hpp"
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
    missing_overview, missing_id, missing_type, missing_description,
    missing_puml, name_collision, backtick_link
};

constexpr std::array<std::string_view, 7> codes{"MISSING_OVERVIEW",
    "MISSING_ID", "MISSING_TYPE", "MISSING_DESCRIPTION", "MISSING_PUML",
    "NAME_COLLISION", "BACKTICK_LINK"};

constexpr std::array<std::string_view, 1> component_type_sources{
    "CE17C7BB-C68C-4304-BD3D-92FA5D13272B"};

constexpr std::array<ct::rule, 7> rules{{
    {"missing_overview", "A component, or a part with include/ and src/, "
        "has modeling/component_overview.org.", component_type_sources},
    {"missing_id", "A component overview has an :ID: property.",
        component_type_sources},
    {"missing_type", "A component overview is #+type: component.",
        component_type_sources},
    {"missing_description", "A component overview has a #+description:.",
        component_type_sources},
    {"missing_puml", "A component's modeling/ directory holds a .puml "
        "diagram."},
    {"name_collision", "No top-level component's dotted name extends "
        "another's."},
    {"backtick_link", "No bracket link a site export cannot resolve sits "
        "inside a backtick span of a page under doc/."}}};

constexpr std::array<ct::option, 1> options{{
    {"exceptions", "projects/ores.codegen/docs_exceptions.txt",
        "The checkout file of CODE component lines that excuse one rule for "
        "one component or page."}}};

/**
 * @brief One violation as the script records it: its code's rule, the
 * name the exceptions file keys it by, the file and byte to report it on
 * and its message.
 */
struct violation {
    rule code;
    std::string name;
    std::string file;
    std::uint32_t offset;
    std::string message;
};

std::set<std::pair<std::string, std::string>> load_exceptions(
    const ct::corpus_view& v) {
    std::set<std::pair<std::string, std::string>> out;
    const auto bytes = v.read_file(v.option("exceptions"));
    if (!bytes)
        return out;
    const auto text = pt::universal_newlines(pt::decode_replacing(*bytes));
    for (const auto raw : co::split_lines(text)) {
        const auto line = co::strip(raw);
        if (line.empty() || line.starts_with('#'))
            continue;
        const auto gap = std::ranges::find_if(line, co::is_space);
        if (gap == line.end())
            continue;
        const auto code = line.substr(0, gap - line.begin());
        const auto name = line.substr(co::skip_spaces(line,
            gap - line.begin()));
        out.emplace(std::string(code), std::string(name));
    }
    return out;
}

void check_overview(const ct::corpus_view& v, const std::string& modeling,
    const std::string& owner, std::vector<violation>& out) {
    static const pt::regex id(R"(:ID:\s+\S+)");
    static const pt::regex type(R"(#\+type:\s*(?:ores\.codegen\.)?component)",
        {.ignorecase = true});
    static const pt::regex description(R"(#\+description:\s*(.+))");
    const auto overview = modeling + "/component_overview.org";
    const auto bytes = v.read_file(overview).value_or("");
    if (!pt::is_utf8(bytes)) {
        out.push_back({missing_description, owner, overview, 0, owner +
            ": the overview is not UTF-8, so it cannot be read"});
        return;
    }
    const auto text = pt::universal_newlines(bytes);
    if (!id.search(text))
        out.push_back({missing_id, owner, overview, 0,
            owner + ": no :ID: UUID in :PROPERTIES: block"});
    if (!type.search(text))
        out.push_back({missing_type, owner, overview, 0, owner +
            ": #+type: component (or ores.codegen.component) not found"});
    const auto m = description.search(text);
    if (!m || co::strip(m->group(1)).empty())
        out.push_back({missing_description, owner, overview, 0,
            owner + ": #+description: missing or empty"});
    const auto pumls = v.list_files(modeling + "/", ".puml");
    const bool has_puml = std::ranges::any_of(pumls, [&](auto p) {
        return co::parent_of(p) == modeling;
    });
    if (!has_puml)
        out.push_back({missing_puml, owner, overview, 0,
            owner + ": no .puml file in modeling/"});
}

/**
 * @brief The names of the directories directly under @p dir that hold a
 * file, in order.
 */
std::vector<std::string> subdirectories(
    std::span<const std::string_view> paths, const std::string& dir) {
    std::set<std::string> out;
    const auto prefix = dir + "/";
    for (auto it = std::ranges::lower_bound(paths, std::string_view(prefix));
         it != paths.end() && it->starts_with(prefix); ++it) {
        const auto rest = it->substr(prefix.size());
        const auto slash = rest.find('/');
        if (slash != std::string_view::npos)
            out.emplace(rest.substr(0, slash));
    }
    return {out.begin(), out.end()};
}

void check_components(const ct::corpus_view& v,
    std::vector<violation>& out) {
    const auto paths = v.list_files("projects/");
    const co::file_set files(paths);
    std::vector<std::string> components;
    for (const auto& c : subdirectories(paths, "projects")) {
        if (c.starts_with("ores."))
            components.push_back(c);
    }
    const auto has_overview = [&](const std::string& modeling) {
        return files.is_file(modeling + "/component_overview.org");
    };
    for (const auto& c : components) {
        const auto dir = "projects/" + c;
        std::vector<std::string> parts;
        for (const auto& p : subdirectories(paths, dir)) {
            if (p != "modeling")
                parts.push_back(p);
        }
        const bool grouped = std::ranges::any_of(parts, [&](const auto& p) {
            return has_overview(dir + "/" + p + "/modeling");
        });
        if (!grouped && files.is_dir(dir + "/modeling")) {
            if (!has_overview(dir + "/modeling"))
                out.push_back({missing_overview, c, dir + "/modeling", 0,
                    c + ": modeling/component_overview.org not found"});
            else
                check_overview(v, dir + "/modeling", c, out);
        }
        for (const auto& p : parts) {
            const auto owner = c + "." + p;
            const auto part = dir + "/" + p;
            const auto modeling = part + "/modeling";
            if (!files.is_dir(modeling)) {
                if (files.is_dir(part + "/include") &&
                    files.is_dir(part + "/src")) {
                    out.push_back({missing_overview, owner, part, 0, owner +
                        ": modeling/component_overview.org not found"});
                    out.push_back({missing_puml, owner, part, 0,
                        owner + ": no .puml diagram in modeling/"});
                }
                continue;
            }
            if (!has_overview(modeling))
                out.push_back({missing_overview, owner, modeling, 0, owner +
                    ": modeling/component_overview.org not found"});
            else
                check_overview(v, modeling, owner, out);
        }
    }
    const std::set<std::string> names(components.begin(), components.end());
    for (const auto& c : components) {
        std::vector<std::string_view> parts;
        for (std::size_t start = 0;;) {
            const auto dot = c.find('.', start);
            parts.push_back(std::string_view(c).substr(start,
                dot == std::string::npos ? std::string::npos : dot - start));
            if (dot == std::string::npos)
                break;
            start = dot + 1;
        }
        std::string prefix(parts[0]);
        for (std::size_t i = 2; i < parts.size(); ++i) {
            prefix += "." + std::string(parts[i - 1]);
            if (!names.contains(prefix))
                continue;
            out.push_back({name_collision, c, "projects/" + c, 0, c +
                ": top-level sibling of component " + prefix + "; absorb "
                "it into that component as a sub-component or rename it"});
            break;
        }
    }
}

std::vector<std::string> bare_link_targets(std::string_view text) {
    static const pt::regex link(R"(\[\[([^\]]+)\]\])");
    std::vector<std::string> out;
    for (const auto& m : link.find_all(text)) {
        auto target = m.group(1);
        target = target.substr(0, target.find("]["));
        target = co::strip(target);
        if (target.contains(':') || target.contains('.') ||
            target.starts_with('*'))
            continue;
        out.emplace_back(target);
    }
    return out;
}

/**
 * @brief The pages under doc/, which the backtick rule reads.
 */
std::vector<std::string_view> pages(const ct::corpus_view& v) {
    return v.list_files("doc/", ".org");
}

/**
 * @brief Makes the fact of a page: the byte and the message of each
 * bracket link inside a backtick span, in order. The findings are the
 * reduce step's, since the exceptions file may excuse a page.
 */
void map_page(std::string_view, std::string_view source,
    const ct::option_values&, ct::file_scan& out) {
    static const pt::regex span(R"(`([^`\n]*)`)");
    static const pt::regex inline_verbatim(R"(=[^=\n]+=|~[^~\n]+~)");
    static const pt::regex verbatim_begin(R"(^#\+(begin_(?:src|example))\b)",
        {.ignorecase = true});
    static const pt::regex verbatim_end(R"(^#\+end_(?:src|example)\b)",
        {.ignorecase = true});
    ct::fact_writer fact(out.fact);
    const auto text = pt::universal_newlines(
        pt::decode_replacing(source));
    bool in_verbatim = false;
    std::uint32_t number = 0;
    for (const auto line : co::split_lines(text)) {
        ++number;
        const auto stripped = co::strip(line);
        if (verbatim_begin.match(stripped)) {
            in_verbatim = true;
            continue;
        }
        if (verbatim_end.match(stripped)) {
            in_verbatim = false;
            continue;
        }
        if (in_verbatim)
            continue;
        for (const auto& m : span.find_all(line)) {
            std::string content(m.group(1));
            for (const auto& s : inline_verbatim.find_all(m.group(1))) {
                for (auto k = s.start(); k < s.end(); ++k)
                    content[k] = '\0';
            }
            const auto targets = bare_link_targets(content);
            if (targets.empty())
                continue;
            std::string joined;
            for (const auto& t : targets)
                joined += (joined.empty() ? "" : ",") + t;
            fact.number(co::offset_of_line(source, number));
            fact.text("org parses " + joined + " inside backticks as a "
                "link, and the site export aborts on the unresolved "
                "target; use =...= or a src block: " +
                std::string(m.group()));
        }
    }
}

/**
 * @brief Reports the gaps of the components, then the backtick links of
 * the pages in path order, each unless the exceptions file excuses it.
 */
void reduce(std::span<const ct::scanned_file> scanned,
    const ct::corpus_view& v, ct::diagnostic_table& out) {
    std::vector<violation> found;
    check_components(v, found);
    std::vector<ct::scanned_file> by_path(scanned.begin(), scanned.end());
    std::ranges::sort(by_path, co::component_less, &ct::scanned_file::path);
    for (const auto& page : by_path) {
        ct::fact_reader fact(page.fact);
        while (!fact.done()) {
            const auto offset = fact.number();
            found.push_back({backtick_link, std::string(page.path),
                std::string(page.path), offset, std::string(fact.text())});
        }
    }
    const auto exceptions = load_exceptions(v);
    for (const auto& f : found) {
        if (exceptions.contains({std::string(codes[f.code]), f.name}))
            continue;
        co::report(out, f.file, f.offset, f.code, f.message,
            "Complete the component's documentation: an overview with its "
            "ID, type and description, and a diagram, or record the "
            "exception in the exceptions file.");
    }
}

}

const ct::descriptor component_docs{
    .name = "component_docs",
    .version = 3,
    .summary = "Reports each component without a complete overview and "
        "diagram, and each link inside backticks under doc/.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::scan_function{pages, map_page, reduce}};

}
