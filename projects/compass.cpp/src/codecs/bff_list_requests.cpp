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
#include "compass.cpp/codecs/bff_list_requests.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::cpp::codecs {

namespace {

namespace ct = compass::codec::types;
namespace pt = compass::platform::text;

constexpr std::array<std::string_view, 1> filter_mismatch_sources{
    "7E3659C3-356E-4F0F-B4BA-C54050F39A98"};

constexpr std::array<ct::rule, 3> rules{{
    {"missing_member", "A hand-written list request states as_of and its "
        "filter when the protocol declares them."},
    {"filter_mismatch", "A literal filter states exactly the members the "
        "filter struct declares.", filter_mismatch_sources},
    {"wrong_rows", "readAll reads the rows a reply carries."}}};

constexpr std::string_view bff_dir = "projects/ores.web/packages/bff/src/";
constexpr std::string_view table_path =
    "projects/ores.web/packages/wire-protocol/src/generated/list_requests.ts";

const std::set<std::string, std::less<>> request_keys{"filter", "as_of",
    "offset", "limit", "order"};

using struct_map = std::map<std::string, std::string, std::less<>>;

/**
 * @brief The structs of each protocol header, kept apart, in path order;
 * a name a header repeats keeps its last body.
 */
std::vector<struct_map> cpp_headers(const ct::corpus_view& v) {
    static const pt::regex struct_re(R"(struct\s+(\w+)\s*\{(.*?)\n\};)",
        {.dotall = true});
    std::vector<std::string_view> headers;
    for (const auto p : v.list_files("projects/", "_protocol.hpp")) {
        const auto parts = ops::components_of(p);
        if (parts.size() == 7 && parts[2] == "api" && parts[3] == "include" &&
            parts[5] == "messaging")
            headers.push_back(p);
    }
    std::ranges::sort(headers, ops::component_less);
    std::vector<struct_map> out;
    for (const auto h : headers) {
        const auto text = v.read_file(h).value_or("");
        struct_map structs;
        for (const auto& m : struct_re.find_all(text))
            structs[std::string(m.group(1))] = std::string(m.group(2));
        out.push_back(std::move(structs));
    }
    return out;
}

/**
 * @brief The data members of a struct body, one per line; the generator
 * also leaves out nested struct lines.
 */
std::vector<std::string> members_of(std::string_view body,
    bool skip_structs) {
    static const pt::regex declared(
        R"(^(?:std::[^=;]*?\s+)?(\w+)\s*(?:=[^;]*)?;$)");
    std::vector<std::string> names;
    for (const auto raw : ops::split_lines(body)) {
        const auto line = ops::strip(raw);
        if (!line.ends_with(';') || line.find('(') != std::string_view::npos)
            continue;
        if (line.starts_with("using ") || line.starts_with("static ") ||
            (skip_structs && line.starts_with("struct ")))
            continue;
        if (const auto m = declared.match(line))
            names.emplace_back(m->group(1));
    }
    return names;
}

std::string_view body_of(const struct_map& structs, std::string_view name) {
    const auto it = structs.find(name);
    return it == structs.end() ? std::string_view{} : it->second;
}

struct request {
    std::optional<std::string> filter_type;
    bool as_of = false;
    std::vector<std::string> rows;
};

const pt::regex& list_request_re() {
    static const pt::regex r(R"(list_\w+_request)");
    return r;
}

const pt::regex& filter_re() {
    static const pt::regex r(R"(std::optional<(\w+)>\s+filter\s*;)");
    return r;
}

const pt::regex& as_of_re() {
    static const pt::regex r(R"(std::optional<[^>]+>\s+as_of\s*;)");
    return r;
}

std::string reply_of(std::string_view name) {
    std::string reply;
    for (std::size_t at = 0;;) {
        const auto next = name.find("_request", at);
        if (next == std::string_view::npos) {
            reply += name.substr(at);
            return reply;
        }
        reply += name.substr(at, next - at);
        reply += "_response";
        at = next + 8;
    }
}

std::map<std::string, request, std::less<>> requests(
    const std::vector<struct_map>& headers) {
    std::map<std::string, request, std::less<>> found;
    for (const auto& structs : headers) {
        for (const auto& [name, body] : structs) {
            if (!list_request_re().fullmatch(name) || found.contains(name))
                continue;
            request r;
            if (const auto f = filter_re().search(body))
                r.filter_type = std::string(f->group(1));
            r.as_of = as_of_re().search(body).has_value();
            const auto reply = body_of(structs, reply_of(name));
            for (auto& member : members_of(reply, false)) {
                const auto before = reply.substr(0, reply.find(member));
                const auto tail = before.substr(before.size() -
                    std::min<std::size_t>(before.size(), 60));
                if (tail.find("vector<") != std::string_view::npos)
                    r.rows.push_back(std::move(member));
            }
            found.emplace(name, std::move(r));
        }
    }
    return found;
}

std::optional<std::pair<std::string_view, std::size_t>> literal_at(
    std::string_view text, std::size_t start) {
    const auto opening = text.find('{', start);
    if (opening == std::string_view::npos)
        return std::nullopt;
    int depth = 0;
    for (auto i = opening; i < text.size(); ++i) {
        if (text[i] == '{') {
            ++depth;
        } else if (text[i] == '}' && --depth == 0) {
            return std::pair{text.substr(opening, i + 1 - opening), i + 1};
        }
    }
    return std::nullopt;
}

std::vector<std::string> top_level_keys(std::string_view literal) {
    static const pt::regex named(R"(^([A-Za-z_]\w*)\s*:)");
    static const pt::regex shorthand(R"(^([A-Za-z_]\w*)\s*[,}])");
    auto inner = ops::strip(literal);
    if (inner.starts_with('{') && inner.ends_with('}'))
        inner = inner.substr(1, inner.size() - 2);
    std::vector<std::string> keys;
    int depth = 0;
    bool expect_key = true;
    for (std::size_t i = 0; i < inner.size();) {
        const char c = inner[i];
        if (c == '(' || c == '[' || c == '{') {
            ++depth;
            expect_key = false;
        } else if (c == ')' || c == ']' || c == '}') {
            --depth;
        } else if (depth == 0 && c == ',') {
            expect_key = true;
        } else if (depth == 0 && !ops::is_space(c) && expect_key) {
            const auto rest = inner.substr(i);
            if (const auto m = named.search(rest)) {
                keys.emplace_back(m->group(1));
                i += m->end();
                expect_key = false;
                continue;
            }
            const auto padded = std::string(rest) + "}";
            if (const auto m = shorthand.search(padded)) {
                keys.emplace_back(m->group(1));
                i += m->end() - 1;
                expect_key = false;
                continue;
            }
            expect_key = false;
        }
        ++i;
    }
    return keys;
}

/**
 * @brief The first arguments of a call whose text starts after its '(',
 * split at top-level commas, up to the closing parenthesis.
 */
std::vector<std::string_view> split_arguments(std::string_view text) {
    std::vector<std::string_view> parts;
    int depth = 0;
    std::size_t start = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '(' || c == '[' || c == '{') {
            ++depth;
        } else if (c == ')' || c == ']' || c == '}') {
            if (depth == 0) {
                parts.push_back(text.substr(start, i - start));
                return parts;
            }
            --depth;
        }
        if (c == ',' && depth == 0) {
            parts.push_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    parts.push_back(text.substr(start));
    return parts;
}

bool is_call_argument(std::string_view text, std::size_t start,
    std::size_t end) {
    auto i = start;
    while (i > 0 && (ops::is_word(text[i - 1]) || text[i - 1] == '.'))
        --i;
    auto before = text.substr(0, i);
    while (!before.empty() && ops::is_space(before.back()))
        before.remove_suffix(1);
    if (before.empty() || (before.back() != '(' && before.back() != ','))
        return false;
    auto after = text.substr(end);
    after.remove_prefix(std::min(after.size(), ops::skip_spaces(after, 0)));
    return !after.starts_with(':');
}

std::vector<std::pair<std::size_t, std::size_t>> read_all_spans(
    std::string_view text) {
    static const pt::regex read_all(R"(\breadAll\()");
    std::vector<std::pair<std::size_t, std::size_t>> spans;
    for (const auto& m : read_all.find_all(text)) {
        int depth = 0;
        for (auto i = m.end() - 1; i < text.size(); ++i) {
            if (text[i] == '(') {
                ++depth;
            } else if (text[i] == ')' && --depth == 0) {
                spans.emplace_back(m.start(), i);
                break;
            }
        }
    }
    return spans;
}

std::string strip_quotes(std::string_view s) {
    s = ops::strip(s);
    while (!s.empty() && (s.front() == '\'' || s.front() == '"'))
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == '\'' || s.back() == '"'))
        s.remove_suffix(1);
    return std::string(s);
}

void check_file(std::string_view path, std::string_view text,
    const std::map<std::string, request, std::less<>>& by_name,
    const struct_map& structs, ct::diagnostic_table& out) {
    static const pt::regex mention(R"(\b(list_\w+_request)\b)");
    static const pt::regex filter_key(R"(\bfilter\s*:)");
    const auto spans = read_all_spans(text);
    const auto at = [](std::size_t offset) {
        return static_cast<std::uint32_t>(offset);
    };
    for (const auto& m : mention.find_all(text)) {
        const std::string name(m.group(1));
        const auto r = by_name.find(name);
        if (r == by_name.end() || !is_call_argument(text, m.start(), m.end()))
            continue;
        const auto literal = literal_at(text, m.end());
        if (!literal)
            continue;
        const auto body = literal->first;
        const auto keys = top_level_keys(body);
        const auto has = [&](std::string_view k) {
            return std::ranges::find(keys, k) != keys.end();
        };
        if (std::ranges::none_of(keys,
                [](const auto& k) { return request_keys.contains(k); }))
            continue;
        if (std::ranges::any_of(spans, [&](const auto& s) {
                return s.first <= m.start() && m.start() <= s.second; }))
            continue;
        if (r->second.as_of && !has("as_of"))
            ops::report(out, path, at(m.start()), 0, name + " leaves out "
                "as_of, which the request declares", "state as_of");
        if (!r->second.filter_type)
            continue;
        if (!has("filter")) {
            ops::report(out, path, at(m.start()), 0, name + " states no "
                "filter at all", "state the filter the request declares");
            continue;
        }
        const auto stated = filter_key.search(body);
        if (!stated)
            continue;
        auto after = body.substr(stated->end());
        after.remove_prefix(std::min(after.size(),
            ops::skip_spaces(after, 0)));
        if (!after.starts_with('{'))
            continue;
        const auto inner = literal_at(body, stated->end());
        if (!inner || ops::strip(inner->first) == "{}")
            continue;
        const auto declared = members_of(body_of(structs,
            *r->second.filter_type), false);
        const std::set<std::string> expected(declared.begin(), declared.end());
        const auto given_keys = top_level_keys(inner->first);
        const std::set<std::string> given(given_keys.begin(),
            given_keys.end());
        for (const auto& missing : expected) {
            if (!given.contains(missing))
                ops::report(out, path, at(m.start()), 1, name + " filter "
                    "leaves out " + missing + ", which the filter declares",
                    "state every member of the filter");
        }
        for (const auto& unknown : given) {
            if (!expected.contains(unknown))
                ops::report(out, path, at(m.start()), 1, name + " filter "
                    "states " + unknown + ", which the filter does not "
                    "declare", "state only the filter's members");
        }
    }

    static const pt::regex read_all(R"(readAll\()");
    for (const auto& m : read_all.find_all(text)) {
        const auto parts = split_arguments(text.substr(m.end()));
        if (parts.size() < 2)
            continue;
        auto subject = ops::strip(parts[1]);
        if (const auto dot = subject.rfind('.');
            dot != std::string_view::npos)
            subject = subject.substr(dot + 1);
        const auto rows = parts.size() > 2 ? strip_quotes(parts[2])
            : std::string();
        if (subject.empty() || rows.empty())
            continue;
        const auto r = by_name.find(subject);
        if (r == by_name.end() || r->second.rows.empty())
            continue;
        if (std::ranges::find(r->second.rows, rows) != r->second.rows.end())
            continue;
        std::string carried;
        for (const auto& row : r->second.rows) {
            if (!carried.empty())
                carried += ", ";
            carried += "'" + row + "'";
        }
        ops::report(out, path, at(m.start()), 2, "reads " +
            std::string(subject) + " rows from '" + rows + "', but the "
            "reply carries " + carried, "read the rows the reply carries");
    }
}

/**
 * @brief The width .prettierrc.yaml sets, within which prettier puts an
 * array on one line.
 */
constexpr std::size_t print_width = 100;

/**
 * @brief The table of every list request the protocol headers declare, as
 * prettier lays it out: an array of members goes on one line when the
 * line fits the print width, as prettier prints an array; the objects stay
 * expanded, as prettier keeps an object broken after its brace.
 */
std::string render_table(const std::vector<struct_map>& headers) {
    static const pt::regex subject_re(
        R"re(static constexpr std::string_view nats_subject\s*=\s*"([^"]+)";)re");
    struct shape {
        bool as_of = false;
        std::vector<std::string> filter;
        std::vector<std::string> rows;
    };
    std::map<std::string, shape> table;
    for (const auto& structs : headers) {
        for (const auto& [name, body] : structs) {
            if (!list_request_re().fullmatch(name))
                continue;
            const auto subject = subject_re.search(body);
            if (!subject)
                continue;
            std::string filter_type;
            if (const auto f = filter_re().search(body))
                filter_type = f->group(1);
            const auto reply = body_of(structs, reply_of(name));
            shape s;
            s.as_of = as_of_re().search(body).has_value();
            s.filter = members_of(body_of(structs, filter_type), true);
            for (auto& member : members_of(reply, true)) {
                if (pt::regex(R"(std::vector<[^>]*>\s+)" + member + R"(\s*;)")
                        .search(reply))
                    s.rows.push_back(std::move(member));
            }
            table[std::string(subject->group(1))] = std::move(s);
        }
    }
    std::string out =
        "/**\n"
        " * The shape of every list request, generated from the C++ protocol.\n"
        " *\n"
        " * A service decodes a list request strictly, so a payload must state every\n"
        " * member the request declares, including `as_of` where it declares one.\n"
        " * `completeListRequest` reads this table and fills what a caller left out.\n"
        " *\n"
        " * Generated by compass from the C++ protocol headers.\n"
        " * Do not edit by hand: run ./compass.sh generate instead.\n"
        " */\n"
        "\n"
        "/** What a list request declares beyond its paging and order. */\n"
        "export interface ListRequestShape {\n"
        "    readonly asOf: boolean;\n"
        "    readonly filter: readonly string[];\n"
        "    readonly rows: readonly string[];\n"
        "}\n"
        "\n"
        "export const LIST_REQUESTS: Readonly<Record<string, ListRequestShape>> = {\n";
    for (const auto& [subject, s] : table) {
        out += "    '" + subject + "': {\n";
        out += std::string("        asOf: ") + (s.as_of ? "true" : "false") +
            ",\n";
        for (const auto& [key, values] : {std::pair{"filter", &s.filter},
                 std::pair{"rows", &s.rows}}) {
            if (values->empty()) {
                out += std::string("        ") + key + ": [],\n";
                continue;
            }
            std::string flat = std::string("        ") + key + ": [";
            for (std::size_t i = 0; i < values->size(); ++i)
                flat += (i == 0 ? "'" : ", '") + (*values)[i] + "'";
            flat += "],";
            if (flat.size() <= print_width) {
                out += flat + "\n";
                continue;
            }
            out += std::string("        ") + key + ": [\n";
            for (const auto& value : *values)
                out += "            '" + value + "',\n";
            out += "        ],\n";
        }
        out += "    },\n";
    }
    out += "};\n";
    return out;
}

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    const auto headers = cpp_headers(v);
    struct_map structs;
    for (const auto& h : headers) {
        for (const auto& [name, body] : h)
            structs[name] = body;
    }
    const auto by_name = requests(headers);
    for (const auto p : v.list_files(bff_dir, ".ts")) {
        if (p.find('/', bff_dir.size()) != std::string_view::npos ||
            p.ends_with(".test.ts"))
            continue;
        check_file(p, v.read_file(p).value_or(""), by_name, structs, out);
    }
}

void write_table(const ct::corpus_view& v, ct::file_output& out) {
    out.files.add(table_path, render_table(cpp_headers(v)));
}

}

const ct::descriptor bff_list_requests{
    .name = "bff_list_requests",
    .version = 2,
    .summary = "Reports a hand-written BFF list request that disagrees with "
        "the C++ protocol.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

const ct::descriptor list_request_table{
    .name = "list_request_table",
    .version = 1,
    .summary = "Writes the wire protocol's table of list requests from the "
        "C++ protocol headers.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = ops::refusal_rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::file_output>{write_table}};

}
