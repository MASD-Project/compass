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
#include "compass.cpp/codecs/forward_declarable_includes.hpp"

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
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::cpp::codecs {

namespace {

namespace ct = compass::codec::types;
namespace pt = compass::platform::text;

constexpr std::array<ct::rule, 2> rules{{
    {"includes_definition_it_only_names", "A header that only names a "
        "listed type includes its declaration, not its definition."},
    {"bad_type_list", "The list of forward-declarable types is readable "
        "and not empty."}}};

constexpr std::array<ct::option, 1> options{{
    {"list", "build/scripts/forward_declarable_types.list",
        "The types a header may name without their definition: name, class "
        "key, defining header and forwarding header or -, one per line."}}};

constexpr std::array<std::string_view, 3> header_suffixes{".hpp", ".h",
    ".ipp"};

constexpr std::array<std::string_view, 5> skipped{"venv", ".venv",
    "node_modules", "build", "_deps"};

constexpr std::string_view generated_marker =
    "AUTO-GENERATED FILE - DO NOT EDIT MANUALLY";

struct declarable {
    std::string name;
    std::string keyword;
    std::string defining_header;
    std::optional<std::string> forwarding_header;

    [[nodiscard]] std::string_view short_name() const {
        return std::string_view(name).substr(name.rfind("::") + 2);
    }

    [[nodiscard]] std::string_view name_space() const {
        return std::string_view(name).substr(0, name.rfind("::"));
    }

    /**
     * @brief pq::, database::pq::, ores::database::pq::: each suffix of the
     * namespace, outermost last.
     */
    [[nodiscard]] std::vector<std::string> qualifiers() const {
        std::vector<std::string> out;
        const auto ns = name_space();
        out.push_back(std::string(ns) + "::");
        for (std::size_t at = ns.find("::"); at != std::string_view::npos;
             at = ns.find("::", at + 2))
            out.push_back(std::string(ns.substr(at + 2)) + "::");
        return out;
    }
};

const pt::regex& include_re() {
    static const pt::regex r(R"re(^[ \t]*#[ \t]*include[ \t]*[<"]([^>"]+)[>"])re",
        {.multiline = true});
    return r;
}

/**
 * @brief The text without its comments, every newline kept: the script's
 * strip_comments, which copies literals whole and drops comment bytes.
 */
std::string strip_comments(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        const char c = text[i];
        if (c == '/' && text.substr(i, 2) == "//") {
            while (i < text.size() && text[i] != '\n')
                ++i;
        } else if (c == '/' && text.substr(i, 2) == "/*") {
            i += 2;
            while (i + 1 < text.size() && text.substr(i, 2) != "*/") {
                if (text[i] == '\n')
                    out.push_back('\n');
                ++i;
            }
            i += 2;
        } else if (c == '"' || c == '\'') {
            out.push_back(c);
            ++i;
            while (i < text.size()) {
                if (text[i] == '\\' && i + 1 < text.size()) {
                    out.append(text.substr(i, 2));
                    i += 2;
                    continue;
                }
                out.push_back(text[i]);
                ++i;
                if (out.back() == c)
                    break;
            }
        } else {
            out.push_back(c);
            ++i;
        }
    }
    return out;
}

bool opens_namespace(const declarable& d, std::string_view text) {
    const auto ns = std::string(d.name_space());
    if (pt::regex(R"(\busing\s+namespace\s+)" + pt::escape(ns) + R"(\s*;)")
            .search(text))
        return true;
    if (pt::regex(R"(\bnamespace\s+)" + pt::escape(ns) + R"(\b[^;]*\{)")
            .search(text))
        return true;
    const auto last = ns.rfind("::");
    if (last == std::string::npos)
        return false;
    return pt::regex(R"(\bnamespace\s+)" + pt::escape(ns.substr(0, last)) +
               R"(\b[^;]*\{)").search(text) &&
        pt::regex(R"(\bnamespace\s+)" + pt::escape(ns.substr(last + 2)) +
            R"(\b)").search(text);
}

bool binds_short_name(const declarable& d, std::string_view text) {
    const auto s = pt::escape(d.short_name());
    return pt::regex(R"(\busing\s+)" + s + R"(\s*=\s*(?:\w+::)*)" + s +
        R"(\s*;)").search(text).has_value();
}

std::optional<std::string> enclosing_template(std::string_view text,
    std::size_t position) {
    static const pt::regex name_before(R"(([A-Za-z_]\w*)\s*$)");
    int depth = 0;
    for (auto index = static_cast<long>(position) - 1; index >= 0; --index) {
        const char c = text[static_cast<std::size_t>(index)];
        if (c == '>') {
            ++depth;
        } else if (c == '<') {
            if (depth == 0) {
                const auto m = name_before.search(
                    text.substr(0, static_cast<std::size_t>(index)));
                if (!m)
                    return std::nullopt;
                return std::string(m->group(1));
            }
            --depth;
        } else if ((c == ';' || c == '{' || c == '}') && depth == 0) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

enum class verdict { names, needs };

verdict classify(std::string_view text, std::size_t start,
    std::size_t end) {
    static const pt::regex key_before(
        R"((?:^|[^\w:])(?:class|struct|enum\s+class)\s*$)");
    static const pt::regex alias_before(R"(\b(?:using|typedef)\b[^;{}]*$)");
    const auto before = text.substr(0, start);
    auto following = text.substr(end);
    following.remove_prefix(std::min(following.size(),
        ops::skip_spaces(following, 0)));
    if (key_before.search(before))
        return verdict::names;
    if (following.starts_with('*') || following.starts_with('&'))
        return verdict::names;
    if (alias_before.search(before))
        return verdict::names;
    if (following.starts_with('>') || following.starts_with(',') ||
        following.starts_with(')')) {
        const auto t = enclosing_template(text, start);
        if (t == "shared_ptr" || t == "weak_ptr")
            return verdict::names;
    }
    return verdict::needs;
}

std::set<std::string> bound_names(const declarable& d,
    std::string_view text) {
    std::set<std::string> names;
    auto qualifiers = d.qualifiers();
    qualifiers.emplace_back();
    for (const auto& q : qualifiers) {
        const auto spelling = pt::escape(q) + pt::escape(d.short_name());
        const pt::regex parameter(R"((?<![\w:])(?:const\s+)?)" + spelling +
            R"(\s*(?:const\s*)?[&*]+\s*(?:const\s+)?([A-Za-z_]\w*))");
        for (const auto& m : parameter.find_all(text))
            names.emplace(m.group(1));
        const pt::regex member(
            R"(\b(?:shared_ptr|weak_ptr|unique_ptr)\s*<\s*)" + spelling +
            R"(\s*>\s*([A-Za-z_]\w*))");
        for (const auto& m : member.find_all(text))
            names.emplace(m.group(1));
    }
    return names;
}

bool dereferences(std::string_view text, const std::set<std::string>& names) {
    return std::ranges::any_of(names, [&](const auto& n) {
        return pt::regex(R"(\b)" + pt::escape(n) + R"(\s*(?:\.|->))")
            .search(text).has_value();
    });
}

std::optional<verdict> scan(const declarable& d, std::string_view text) {
    std::set<verdict> verdicts;
    const auto short_name = pt::escape(d.short_name());
    for (const auto& q : d.qualifiers()) {
        const pt::regex use(R"((?<![\w:]))" + pt::escape(q) + short_name +
            R"((?![\w]))");
        for (const auto& m : use.find_all(text))
            verdicts.insert(classify(text, m.start(), m.end()));
    }
    if (opens_namespace(d, text) || binds_short_name(d, text)) {
        const pt::regex use(R"((?<![\w:]))" + short_name + R"((?![\w]))");
        for (const auto& m : use.find_all(text))
            verdicts.insert(classify(text, m.start(), m.end()));
    }
    if (verdicts.contains(verdict::needs))
        return verdict::needs;
    if (dereferences(text, bound_names(d, text)))
        return verdict::needs;
    if (verdicts.empty())
        return std::nullopt;
    return verdict::names;
}

bool is_header(std::string_view path) {
    const auto parts = ops::components_of(path);
    for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
        if (std::ranges::find(skipped, parts[i]) != skipped.end())
            return false;
    }
    return std::ranges::find(header_suffixes, ops::suffix_of(path)) !=
        header_suffixes.end();
}

std::string relative_include(std::string_view path) {
    const auto parts = ops::components_of(path);
    for (auto i = parts.size(); i-- > 0;) {
        if (parts[i] != "include")
            continue;
        std::string out;
        for (auto k = i + 1; k < parts.size(); ++k) {
            if (!out.empty())
                out += '/';
            out += parts[k];
        }
        return out;
    }
    return std::string(parts.back());
}

std::uint32_t include_line(std::string_view text, std::string_view include) {
    std::uint32_t number = 0;
    for (const auto line : ops::split_lines(text)) {
        ++number;
        if (line.find(include) != std::string_view::npos &&
            line.find("#include") != std::string_view::npos)
            return number;
    }
    return 0;
}

std::vector<std::string_view> scanned(const ct::corpus_view& v) {
    std::vector<std::string_view> headers;
    for (const auto suffix : header_suffixes) {
        for (const auto p : v.list_files("projects/", suffix)) {
            if (is_header(p))
                headers.push_back(p);
        }
    }
    std::ranges::sort(headers, ops::component_less);
    return headers;
}

/**
 * @brief The fact of a header: the path of each of its #include lines.
 */
void map(std::string_view, std::string_view bytes, const ct::option_values&,
    ct::file_scan& out) {
    const auto text = pt::universal_newlines(pt::decode_ignoring(bytes));
    ct::fact_writer fact(out.fact);
    for (const auto& m : include_re().find_all(text))
        fact.text(m.group(1));
}

/**
 * @brief Reads the list, then reads again only the headers whose includes
 * name a listed type's defining header.
 */
void reduce(std::span<const ct::scanned_file> headers,
    const ct::corpus_view& v, ct::diagnostic_table& out) {
    const auto list_path = v.option("list");
    const auto list = v.read_file(list_path);
    std::vector<declarable> declarables;
    std::uint32_t number = 0;
    for (const auto line : ops::split_lines(list.value_or(""))) {
        ++number;
        const auto stripped = ops::strip(line);
        if (stripped.empty() || stripped.starts_with('#'))
            continue;
        std::vector<std::string> fields;
        for (std::size_t i = ops::skip_spaces(stripped, 0);
             i < stripped.size(); i = ops::skip_spaces(stripped, i)) {
            auto j = i;
            while (j < stripped.size() && !ops::is_space(stripped[j]))
                ++j;
            fields.emplace_back(stripped.substr(i, j - i));
            i = j;
        }
        const auto at = ops::offset_of_line(*list, number);
        if (fields.size() != 4) {
            ops::report(out, list_path, at, 1, "expected four fields, found " +
                std::to_string(fields.size()), "write the type, its class "
                "key, its defining header and its forwarding header or -");
            return;
        }
        if (fields[1] != "class" && fields[1] != "struct") {
            ops::report(out, list_path, at, 1, "the class key must be class "
                "or struct, found " + fields[1], "write class or struct");
            return;
        }
        declarables.push_back({fields[0], fields[1], fields[2],
            fields[3] == "-" ? std::nullopt
                : std::optional<std::string>(fields[3])});
    }
    if (declarables.empty()) {
        ops::report(out, list_path, 0, 1, "no type is listed, so the guard "
            "is off", "list the types a header may forward-declare");
        return;
    }
    std::map<std::string, std::vector<const declarable*>, std::less<>>
        by_include;
    for (const auto& d : declarables)
        by_include[d.defining_header].push_back(&d);

    static const pt::regex template_re(R"(^\s*\*\s*Template:\s*(\S+)\s*$)",
        {.multiline = true});
    for (const auto& [path, fact] : headers) {
        const auto own = relative_include(path);
        std::vector<const declarable*> candidates;
        ct::fact_reader includes(fact);
        while (!includes.done()) {
            const auto it = by_include.find(includes.text());
            if (it == by_include.end())
                continue;
            for (const auto* d : it->second) {
                if (d->defining_header != own &&
                    std::ranges::find(candidates, d) == candidates.end())
                    candidates.push_back(d);
            }
        }
        if (candidates.empty())
            continue;
        const auto bytes = v.read_file(path).value_or("");
        const auto text = pt::universal_newlines(pt::decode_ignoring(bytes));
        std::optional<std::string> generated_by;
        if (text.find(generated_marker) != std::string::npos) {
            const auto head = std::string_view(text).substr(0,
                std::min(text.size(), ops::byte_of_character(text, 2000)));
            const auto m = template_re.search(head);
            generated_by = m ? std::string(m->group(1)) : "unknown";
        }
        const auto body = include_re().substitute(strip_comments(text), "");
        for (const auto* d : candidates) {
            if (scan(*d, body) != verdict::names)
                continue;
            const auto line = include_line(text, d->defining_header);
            const auto key = d->name + ":" + (generated_by
                ? "template:" + *generated_by : std::string(path));
            ops::report(out, path, line == 0 ? 0
                    : ops::offset_of_line(bytes, line), 0,
                "the header includes " + d->defining_header + " but only "
                "names " + d->name + " (" + key + ")",
                d->forwarding_header
                    ? "include " + *d->forwarding_header + " instead"
                    : "declare " + d->keyword + " " + d->name + ";");
        }
    }
}

}

const ct::descriptor forward_declarable_includes{
    .name = "forward_declarable_includes",
    .version = 2,
    .summary = "Reports each header that includes the definition of a "
        "listed type it only names.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::scan_function{scanned, map, reduce}};

}
