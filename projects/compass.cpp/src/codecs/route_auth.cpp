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
#include "compass.cpp/codecs/route_auth.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::cpp::codecs {

namespace {

namespace ct = compass::codec::types;
namespace pt = compass::platform::text;

constexpr std::array<ct::rule, 2> rules{{
    {"route_without_auth", "Every add_route call states its authentication "
        "position: auth_required, auth_optional or roles."},
    {"too_few_routes", "The scan finds at least 20 add_route calls, so a "
        "pattern that stops matching fails rather than passes."}}};

constexpr std::string_view http_root = "projects/ores.http/";
constexpr std::size_t minimum_routes = 20;

const pt::regex& add_route_re() {
    static const pt::regex r(R"((?<=[.>])\s*add_route\s*\()");
    return r;
}

/**
 * @brief True where a byte is code, false inside a comment or a literal;
 * a raw string runs to its )delimiter".
 */
std::vector<bool> mask_code(std::string_view text) {
    const auto n = text.size();
    std::vector<bool> mask(n, true);
    const auto clear = [&](std::size_t from, std::size_t to) {
        for (auto k = from; k < to && k < n; ++k)
            mask[k] = false;
    };
    std::size_t i = 0;
    while (i < n) {
        const char c = text[i];
        if (c == '/' && i + 1 < n && text[i + 1] == '/') {
            while (i < n && text[i] != '\n')
                mask[i++] = false;
        } else if (c == '/' && i + 1 < n && text[i + 1] == '*') {
            auto end = text.find("*/", i + 2);
            end = end == std::string_view::npos ? n : end + 2;
            clear(i, end);
            i = end;
        } else if (c == '"' && i >= 1 && text[i - 1] == 'R') {
            const auto paren = text.find('(', i + 1);
            if (paren == std::string_view::npos) {
                mask[i++] = false;
                continue;
            }
            const auto close = ")" + std::string(text.substr(i + 1,
                paren - i - 1)) + "\"";
            auto end = text.find(close, paren + 1);
            end = end == std::string_view::npos ? n : end + close.size();
            clear(i, end);
            i = end;
        } else if (c == '"' || c == '\'') {
            auto j = i + 1;
            while (j < n) {
                if (text[j] == '\\') {
                    j += 2;
                    continue;
                }
                if (text[j] == c)
                    break;
                ++j;
            }
            const auto end = std::min(j + 1, n);
            clear(i, end);
            i = end;
        } else {
            ++i;
        }
    }
    return mask;
}

std::string masked_text(std::string_view text) {
    const auto mask = mask_code(text);
    std::string out(text);
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (!mask[i])
            out[i] = ' ';
    }
    return out;
}

std::optional<std::size_t> close_paren(std::string_view text,
    const std::vector<bool>& mask, std::size_t open) {
    int depth = 0;
    for (auto i = open; i < text.size(); ++i) {
        if (!mask[i])
            continue;
        if (text[i] == '(') {
            ++depth;
        } else if (text[i] == ')' && --depth == 0) {
            return i;
        }
    }
    return std::nullopt;
}

std::size_t statement_end(std::string_view text,
    const std::vector<bool>& mask, std::size_t start) {
    int paren = 0;
    int brace = 0;
    int bracket = 0;
    for (auto i = start; i < text.size(); ++i) {
        if (!mask[i])
            continue;
        switch (text[i]) {
        case '(': ++paren; break;
        case ')': --paren; break;
        case '{': ++brace; break;
        case '}': --brace; break;
        case '[': ++bracket; break;
        case ']': --bracket; break;
        case ';':
            if (paren == 0 && brace == 0 && bracket == 0)
                return i + 1;
            break;
        default: break;
        }
    }
    return text.size();
}

bool declared(std::string_view chain) {
    static const pt::regex declaration(
        R"(\.\s*(?:auth_required|auth_optional|roles)\s*\()");
    return declaration.search(masked_text(chain)).has_value();
}

std::optional<std::string_view> variable_chain(std::string_view text,
    const std::vector<bool>& mask, std::string_view name) {
    const pt::regex assignment(R"((?<![\w.]))" + pt::escape(name) +
        R"(\s*=)");
    for (const auto& m : assignment.find_all(text)) {
        if (!mask[m.start()])
            continue;
        const auto newline = text.rfind('\n', m.start() == 0 ? 0
            : m.start() - 1);
        const auto head = newline == std::string_view::npos || m.start() == 0
            ? 0 : newline + 1;
        if (text.substr(head, m.start() - head).find("auto") ==
            std::string_view::npos)
            continue;
        return text.substr(m.start(),
            statement_end(text, mask, m.start()) - m.start());
    }
    return std::nullopt;
}

std::optional<std::string_view> builder_chain(std::string_view text,
    const std::vector<bool>& mask, std::string_view name,
    std::set<std::string>& seen) {
    if (!seen.insert(std::string(name)).second)
        return std::nullopt;
    const auto chain = variable_chain(text, mask, name);
    if (!chain)
        return std::nullopt;
    static const pt::regex build_alias(
        R"(^\s*[A-Za-z_]\w*\s*=\s*([A-Za-z_]\w*)\s*\.\s*build\s*\(\s*\)\s*;)");
    const auto alias = build_alias.match(*chain);
    if (!alias)
        return chain;
    return builder_chain(text, mask, alias->group(1), seen);
}

bool is_source(std::string_view path) {
    const auto parts = ops::components_of(path);
    return std::ranges::find(parts, "tests") == parts.end() &&
        std::ranges::find(parts, "build") == parts.end();
}

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    static const pt::regex variable_build(
        R"(^\s*([A-Za-z_]\w*)\s*\.\s*build\s*\(\s*\)\s*$)");
    static const pt::regex bare_name(R"(^[A-Za-z_]\w*$)");
    static const pt::regex route_re(
        R"re((?:->|\.)\s*(get|post|put|patch|delete_|head|options)\s*\(\s*"([^"]*)")re");
    std::vector<std::string_view> paths;
    for (const auto suffix : {".cpp", ".hpp"}) {
        for (const auto p : v.list_files(http_root, suffix)) {
            if (is_source(p))
                paths.push_back(p);
        }
    }
    std::ranges::sort(paths, ops::component_less);

    std::size_t found = 0;
    for (const auto path : paths) {
        const auto text = v.read_file(path).value_or("");
        const auto mask = mask_code(text);
        found += add_route_re().find_all(masked_text(text)).size();
        for (const auto& m : add_route_re().find_all(text)) {
            if (!mask[m.start()])
                continue;
            const auto open = text.find('(', m.start());
            const auto close = close_paren(text, mask, open);
            if (!close)
                continue;
            const auto argument = std::string_view(text).substr(open + 1,
                *close - open - 1);
            std::set<std::string> seen;
            std::string_view chain;
            if (const auto variable = variable_build.match(argument)) {
                const auto c = builder_chain(text, mask, variable->group(1),
                    seen);
                if (!c)
                    continue;
                chain = *c;
            } else {
                const auto first = argument.find_first_not_of(" \t\n\r\f\v");
                const auto last = argument.find_last_not_of(" \t\n\r\f\v");
                const auto name = first == std::string::npos
                    ? std::string_view{}
                    : argument.substr(first, last - first + 1);
                std::optional<std::string_view> c;
                if (bare_name.match(name))
                    c = builder_chain(text, mask, name, seen);
                chain = c ? *c : argument;
            }
            if (declared(chain))
                continue;
            const auto route = route_re.search(chain);
            std::string where = "an unnamed route";
            if (route) {
                std::string method(route->group(1));
                std::ranges::transform(method, method.begin(),
                    [](unsigned char ch) {
                        return static_cast<char>(std::toupper(ch));
                    });
                where = method + " " + std::string(route->group(2));
            }
            ops::report(out, path, static_cast<std::uint32_t>(m.start()), 0,
                where + " states no authentication position",
                "call auth_required() or auth_optional() on its builder");
        }
    }
    if (found < minimum_routes)
        ops::report(out, http_root.substr(0, http_root.size() - 1), 0, 1,
            "only " + std::to_string(found) + " add_route call(s) found, so "
            "the scan read almost nothing", "teach the check how routes are "
            "registered now");
}

}

const ct::descriptor route_auth{
    .name = "route_auth",
    .version = 1,
    .summary = "Reports each HTTP route registered with no authentication "
        "position.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}
