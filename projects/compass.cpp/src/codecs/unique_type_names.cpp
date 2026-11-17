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
#include "compass.cpp/codecs/unique_type_names.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codec/types/fact.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::cpp::codecs {

namespace {

namespace ct = compass::codec::types;
namespace pt = compass::platform::text;

constexpr std::array<ct::rule, 1> rules{{
    {"type_defined_twice", "No two headers that can meet in one binary "
        "define a class or struct of the same qualified name."}}};

/**
 * @brief The binary that may include every public header.
 */
constexpr std::string_view public_binary = "public";

/**
 * @brief True when a directory below projects/ores.<name>/ on the way to the
 * file is @p dir.
 */
bool under(const std::vector<std::string_view>& parts, std::string_view dir) {
    for (std::size_t i = 2; i + 1 < parts.size(); ++i) {
        if (parts[i] == dir)
            return true;
    }
    return false;
}

bool is_scanned(std::string_view path) {
    const auto parts = ops::components_of(path);
    if (parts.size() < 3 || !parts[1].starts_with("ores.") ||
        !parts.back().ends_with(".hpp"))
        return false;
    if (std::ranges::find(parts, "build") != parts.end())
        return false;
    return under(parts, "include") || under(parts, "tests");
}

std::string binary_of(std::string_view path) {
    const auto parts = ops::components_of(path);
    if (std::ranges::find(parts, "include") != parts.end())
        return std::string(public_binary);
    std::string out;
    for (const auto part : parts) {
        if (!out.empty())
            out += '/';
        out += part;
        if (part == "tests")
            break;
    }
    return out;
}

/**
 * @brief Each qualified class or struct name the header defines directly
 * in named namespaces, with the offset of its first definition.
 */
std::map<std::string, std::uint32_t> defined_types(std::string_view text) {
    static const pt::regex lexeme(
        R"re(R"(?P<delim>[^(\s]*)\(.*?\)(?P=delim)"|//[^\n]*|/\*.*?\*/|^\s*#[^\n]*)re"
        R"re(|"(?:[^"\\\n]|\\.)*"|'(?:[^'\\\n]|\\.)*')re",
        {.dotall = true, .multiline = true});
    static const pt::regex token(
        R"(\bnamespace\s+(?P<namespace>[A-Za-z_][\w:]*)?\s*\{)"
        R"(|\b(?:struct|class)\s+(?:[A-Z][A-Z0-9_]+\s+)?(?P<type>[A-Za-z_]\w*))"
        R"((?:\s+final)?\s*(?::[^{;()]*)?\{)"
        R"(|\{|\})");
    // A lexeme blanked byte for byte keeps the offsets; the tokens split the
    // same as around the one space the script puts in its place.
    const auto blanked = lexeme.substitute(text, [](const auto& m) {
        std::string out(m.group().size(), ' ');
        for (std::size_t i = 0; i < out.size(); ++i) {
            if (m.group()[i] == '\n')
                out[i] = '\n';
        }
        return out;
    });
    // Each open brace: a namespace name, empty for an anonymous namespace,
    // or nothing for any other scope.
    std::vector<std::optional<std::string>> scopes;
    std::map<std::string, std::uint32_t> found;
    for (const auto& m : token.find_all(blanked)) {
        const auto t = m.group();
        if (t == "}") {
            if (!scopes.empty())
                scopes.pop_back();
            continue;
        }
        const bool in_namespaces = std::ranges::all_of(scopes,
            [](const auto& s) { return s && !s->empty(); });
        if (m.matched("type")) {
            if (!scopes.empty() && in_namespaces) {
                std::string name;
                for (const auto& s : scopes)
                    name += *s + "::";
                name += m.group("type");
                found.try_emplace(std::move(name),
                    static_cast<std::uint32_t>(m.start("type")));
            }
            scopes.emplace_back(std::nullopt);
        } else if (t.starts_with("namespace")) {
            scopes.emplace_back(std::string(m.group("namespace")));
        } else {
            scopes.emplace_back(std::nullopt);
        }
    }
    return found;
}

std::vector<std::string_view> scanned(const ct::corpus_view& v) {
    std::vector<std::string_view> headers;
    for (const auto p : v.list_files("projects/ores.", ".hpp")) {
        if (is_scanned(p))
            headers.push_back(p);
    }
    std::ranges::sort(headers, ops::component_less);
    return headers;
}

/**
 * @brief The fact of a header: each type it defines and its offset, zero
 * when the text was decoded, as defined_types orders them.
 */
void map(std::string_view, std::string_view bytes, const ct::option_values&,
    ct::file_scan& out) {
    const bool exact = pt::is_utf8(bytes) &&
        bytes.find('\r') == std::string_view::npos;
    const auto text = exact ? std::string(bytes)
        : pt::universal_newlines(pt::decode_replacing(bytes));
    ct::fact_writer fact(out.fact);
    for (const auto& [name, offset] : defined_types(text))
        fact.text(name).number(exact ? offset : 0);
}

void reduce(std::span<const ct::scanned_file> headers,
    const ct::corpus_view&, ct::diagnostic_table& out) {

    struct definition {
        std::string_view header;
        std::uint32_t offset;
    };
    // Name, then binary in the order first seen, then its definitions.
    std::map<std::string,
        std::vector<std::pair<std::string, std::vector<definition>>>> defined;
    for (const auto& [h, f] : headers) {
        const auto binary = binary_of(h);
        ct::fact_reader fact(f);
        while (!fact.done()) {
            const std::string name(fact.text());
            const auto offset = fact.number();
            auto& binaries = defined[name];
            auto it = std::ranges::find_if(binaries,
                [&](const auto& b) { return b.first == binary; });
            if (it == binaries.end())
                it = binaries.insert(binaries.end(), {binary, {}});
            it->second.push_back({h, offset});
        }
    }

    for (const auto& [name, binaries] : defined) {
        std::vector<definition> pub;
        for (const auto& [binary, defs] : binaries) {
            if (binary == public_binary)
                pub = defs;
        }
        for (const auto& [binary, defs] : binaries) {
            auto meeting = defs;
            for (const auto& d : pub) {
                if (std::ranges::none_of(meeting,
                        [&](const auto& m) { return m.header == d.header; }))
                    meeting.push_back(d);
            }
            if (meeting.size() < 2)
                continue;
            std::ranges::sort(meeting, {}, &definition::header);
            std::string where;
            for (const auto& d : meeting) {
                if (!where.empty())
                    where += " and ";
                where += d.header;
            }
            ops::report(out, meeting.front().header, meeting.front().offset, 0,
                name + " is defined in " + where + ", which can meet in one "
                "binary", "rename one of the types, or move it into a "
                "namespace of its own");
            break;
        }
    }
}

}

const ct::descriptor unique_type_names{
    .name = "unique_type_names",
    .version = 2,
    .summary = "Reports each qualified type name two headers define where "
        "both can meet in one binary.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::scan_function{scanned, map, reduce}};

}
