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
#include "compass.sql/codecs/curve_parameter_paths.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
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

constexpr std::array<ct::rule, 3> rules{{
    {"undeclared_path", "Every curve parameter path the artefact names is "
        "one the ORE curve schema declares, or an exemption explains."},
    {"stale_exemption", "Every exemption explains some path of the "
        "artefact."},
    {"unreadable_input", "The schema declares the root element, and the "
        "exemptions read."}}};

constexpr std::array<ct::option, 3> options{{
    {"artefact", "projects/ores.sql/populate/reporting/"
        "reporting_ore_parameters_artefact_populate.sql",
        "The populate script whose quoted /CurveConfiguration/ paths are "
        "checked."},
    {"schema", "external/ore/xsd/curveconfig.xsd",
        "The ORE curve configuration schema, with the schemas it includes."},
    {"exemptions", "projects/ores.sql/utility/curve_schema_exemptions.json",
        "The paths ORE reads though a schema type omits them: a parent "
        "path, with * for any segment, and the elements read there."}}};

constexpr std::string_view root_element = "CurveConfiguration";

struct exemption {
    std::vector<std::string> path;
    std::vector<std::string> elements;
    std::uint32_t offset = 0;
};

void add(ct::diagnostic_table& out, std::string_view file,
    std::uint32_t offset, std::uint32_t rule, const std::string& message,
    std::string_view fix) {
    out.add({.document = compass::org::types::document_id::none(),
        .range = {offset, offset},
        .rule = ct::rule_id{rule},
        .level = ct::severity::error,
        .message = message,
        .fix = fix,
        .file = file});
}

/**
 * @brief The strings of the JSON array that follows "@p key": in @p object.
 */
std::optional<std::vector<std::string>> strings_of(std::string_view object,
    std::string_view key) {
    const pt::regex array("\"" + pt::escape(key) + R"("\s*:\s*\[(.*?)\])",
        {.dotall = true});
    static const pt::regex item(R"re("((?:[^"\\]|\\.)*)")re");
    const auto m = array.search(object);
    if (!m)
        return std::nullopt;
    std::vector<std::string> out;
    for (const auto& s : item.find_all(m->group(1)))
        out.emplace_back(s.group(1));
    return out;
}

/**
 * @brief The rules of the exemptions file: each object of its "rules"
 * array, with the offset of its opening brace.
 */
std::vector<exemption> read_exemptions(std::string_view json) {
    std::vector<exemption> out;
    const auto start = json.find("\"rules\"");
    if (start == std::string_view::npos)
        return out;
    int depth = 0;
    std::size_t open = 0;
    for (auto i = json.find('[', start); i < json.size(); ++i) {
        const char c = json[i];
        if (c == '"') {
            for (++i; i < json.size() && json[i] != '"'; ++i) {
                if (json[i] == '\\')
                    ++i;
            }
        } else if (c == '{') {
            if (depth++ == 0)
                open = i;
        } else if (c == '}') {
            if (--depth == 0) {
                const auto object = json.substr(open, i + 1 - open);
                exemption e;
                e.path = strings_of(object, "path").value_or(
                    std::vector<std::string>{});
                e.elements = strings_of(object, "elements").value_or(
                    std::vector<std::string>{});
                e.offset = static_cast<std::uint32_t>(open);
                out.push_back(std::move(e));
            }
        } else if (c == ']' && depth == 0) {
            break;
        }
    }
    return out;
}

bool exempt(const exemption& rule, const std::vector<std::string>& segments) {
    if (segments.size() != rule.path.size() + 1)
        return false;
    for (std::size_t i = 0; i < rule.path.size(); ++i) {
        if (rule.path[i] != "*" && rule.path[i] != segments[i])
            return false;
    }
    return std::ranges::find(rule.elements, segments.back()) !=
        rule.elements.end();
}

std::string normalise(std::string_view dir, std::string_view name) {
    std::vector<std::string> parts;
    for (const auto path : {dir, name}) {
        for (std::size_t s = 0; s <= path.size();) {
            auto e = path.find('/', s);
            if (e == std::string_view::npos)
                e = path.size();
            const auto part = path.substr(s, e - s);
            s = e + 1;
            if (part.empty() || part == ".")
                continue;
            if (part == ".." && !parts.empty())
                parts.pop_back();
            else
                parts.emplace_back(part);
        }
    }
    std::string out;
    for (const auto& p : parts)
        out += (out.empty() ? "" : "/") + p;
    return out;
}

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    static const pt::regex complex_type(
        R"re(<xs:complexType\s+name="(\w+)">(.*?)\n  </xs:complexType>)re",
        {.dotall = true});
    static const pt::regex include(R"re(<xs:include\s+schemaLocation="([^"]+)")re");
    static const pt::regex declared(R"(<xs:element\b([^>]*?)(/?)>)");
    static const pt::regex name_attr(R"re(\bname="(\w+)")re");
    static const pt::regex type_attr(R"re(\btype="([\w.-]+)")re");
    static const pt::regex xpath_re(R"('(/CurveConfiguration/[A-Za-z0-9_/]+)')");

    const std::string schema(v.option("schema"));
    std::map<std::string, std::string> types;
    std::set<std::string> seen;
    std::vector<std::string> pending{schema};
    while (!pending.empty()) {
        auto current = std::move(pending.back());
        pending.pop_back();
        if (seen.contains(current))
            continue;
        const auto body = v.read_file(current);
        if (!body)
            continue;
        seen.insert(current);
        for (const auto& m : complex_type.find_all(*body))
            types[std::string(m.group(1))] = std::string(m.group(2));
        const auto dir = current.substr(0, current.rfind('/'));
        for (const auto& m : include.find_all(*body))
            pending.push_back(normalise(dir, m.group(1)));
    }
    const pt::regex root_re(R"re(<xs:element\s+type="(\w+)"\s+name=")re" +
        std::string(root_element) + "\"");
    const auto schema_text = v.read_file(schema).value_or("");
    const auto root = root_re.search(schema_text);
    if (!root) {
        add(out, schema, 0, 2, schema + " declares no " +
            std::string(root_element) + " element",
            "point the schema option at the curve configuration schema");
        return;
    }
    const std::string start(root->group(1));

    const auto artefact = v.option("artefact");
    const auto artefact_text = v.read_file(artefact).value_or("");
    std::map<std::string, std::size_t> found;
    for (const auto& m : xpath_re.find_all(artefact_text))
        found.try_emplace(std::string(m.group(1)), m.start());

    const auto exemptions_path = v.option("exemptions");
    const auto exemptions = read_exemptions(
        v.read_file(exemptions_path).value_or(""));

    const auto segments_of = [](std::string_view xpath) {
        std::vector<std::string> segments;
        const auto rest = xpath.substr(root_element.size() + 1);
        for (std::size_t s = 0; s <= rest.size();) {
            auto e = rest.find('/', s);
            if (e == std::string_view::npos)
                e = rest.size();
            if (e > s)
                segments.emplace_back(rest.substr(s, e - s));
            s = e + 1;
        }
        return segments;
    };

    std::vector<bool> used(exemptions.size(), false);
    for (const auto& [xpath, offset] : found) {
        const auto segments = segments_of(xpath);
        for (std::size_t r = 0; r < exemptions.size(); ++r) {
            if (exempt(exemptions[r], segments))
                used[r] = true;
        }
        std::optional<std::string> failure;
        auto current = start;
        for (std::size_t i = 0; i < segments.size(); ++i) {
            const auto body = types.find(current);
            std::string parent;
            for (std::size_t k = 0; k < i; ++k)
                parent += (k == 0 ? "" : "/") + segments[k];
            const auto fail = [&](std::string_view p, std::string_view kind,
                std::string_view child) {
                failure = (p.empty() ? std::string(root_element)
                    : std::string(p)) + " of type " + std::string(kind) +
                    " does not declare " + std::string(child);
            };
            if (body == types.end()) {
                fail(parent, current, segments[i]);
                break;
            }
            std::map<std::string, std::optional<std::string>> children;
            for (const auto& m : declared.find_all(body->second)) {
                const auto attributes = m.group(1);
                const auto name = name_attr.search(attributes);
                if (!name)
                    continue;
                const auto kind = type_attr.search(attributes);
                children[std::string(name->group(1))] = kind
                    ? std::optional<std::string>(kind->group(1))
                    : std::nullopt;
            }
            const auto child = children.find(segments[i]);
            if (child == children.end()) {
                fail(parent, current, segments[i]);
                break;
            }
            if (i + 1 == segments.size())
                break;
            const auto& kind = child->second;
            if (!kind || !types.contains(*kind)) {
                fail(parent.empty() ? segments[i] : parent + "/" + segments[i],
                    kind ? *kind : segments[i], segments[i + 1]);
                break;
            }
            current = *kind;
        }
        if (!failure)
            continue;
        if (std::ranges::any_of(exemptions,
                [&](const auto& r) { return exempt(r, segments); }))
            continue;
        add(out, artefact, static_cast<std::uint32_t>(offset), 0, xpath +
            ": " + *failure, "correct the path, or add an exemption with "
            "the ORE source line that reads it");
    }
    for (std::size_t r = 0; r < exemptions.size(); ++r) {
        if (used[r])
            continue;
        std::string path;
        for (const auto& p : exemptions[r].path)
            path += (path.empty() ? "" : "/") + p;
        add(out, exemptions_path, exemptions[r].offset, 1, "the exemption "
            "for " + path + " names no path of the artefact",
            "retire the exemption");
    }
}

}

const ct::descriptor curve_parameter_paths{
    .name = "curve_parameter_paths",
    .version = 1,
    .summary = "Reports each curve parameter path the ORE curve schema does "
        "not declare, and each exemption no path needs.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}
