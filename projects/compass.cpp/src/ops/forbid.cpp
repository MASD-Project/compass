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
#include "compass.cpp/ops/forbid.hpp"

#include <algorithm>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include "compass.cpp/ops/glob.hpp"
#include "compass.cpp/ops/lexer.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/source_model.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::cpp::ops {

namespace {

namespace pt = compass::platform::text;

std::vector<std::string> words_of(std::string_view value) {
    std::vector<std::string> out;
    std::size_t at = 0;
    while (at < value.size()) {
        const auto start = value.find_first_not_of(' ', at);
        if (start == std::string_view::npos)
            break;
        const auto end = std::min(value.find(' ', start), value.size());
        out.emplace_back(value.substr(start, end - start));
        at = end;
    }
    return out;
}

/**
 * @brief @p glob with each {a,b} replaced by each alternative in turn, or
 * why it cannot be.
 */
std::expected<std::vector<std::string>, std::string> expand_braces(
    std::string_view glob) {
    const auto open = glob.find('{');
    if (open == std::string_view::npos) {
        if (glob.find('}') != std::string_view::npos)
            return std::unexpected("unopened } in " + std::string(glob));
        return std::vector<std::string>{std::string(glob)};
    }
    const auto close = glob.find('}', open);
    if (close == std::string_view::npos)
        return std::unexpected("unclosed { in " + std::string(glob));
    const auto inner = glob.substr(open + 1, close - open - 1);
    if (inner.find('{') != std::string_view::npos)
        return std::unexpected("nested { in " + std::string(glob));
    auto rest = expand_braces(glob.substr(close + 1));
    if (!rest)
        return rest;
    std::vector<std::string> out;
    std::size_t at = 0;
    while (true) {
        const auto comma = std::min(inner.find(',', at), inner.size());
        for (const auto& r : *rest)
            out.push_back(std::string(glob.substr(0, open)) +
                std::string(inner.substr(at, comma - at)) + r);
        if (comma == inner.size())
            break;
        at = comma + 1;
    }
    return out;
}

std::string_view trimmed(std::string_view s) {
    while (!s.empty() && s.front() == ' ')
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\r'))
        s.remove_suffix(1);
    return s;
}

const pt::regex& compiled(const std::string& pattern) {
    thread_local std::unordered_map<std::string, pt::regex> cache;
    const auto it = cache.find(pattern);
    if (it != cache.end())
        return it->second;
    return cache.emplace(pattern, pt::regex(pattern)).first->second;
}

std::string with_match(std::string_view text, std::string_view match) {
    std::string out;
    std::size_t at = 0;
    for (auto p = text.find("{match}"); p != std::string_view::npos;
         p = text.find("{match}", at)) {
        out.append(text.substr(at, p - at)).append(match);
        at = p + 7;
    }
    return out.append(text.substr(at));
}

/**
 * @brief True when the name at @p offset of @p code follows . or ->, so it
 * names a member, not the facility.
 */
bool is_member(std::string_view code, std::uint32_t offset) {
    auto at = static_cast<std::size_t>(offset);
    while (at > 0 && (code[at - 1] == ' ' || code[at - 1] == '\t' ||
        code[at - 1] == '\n'))
        --at;
    return (at >= 1 && code[at - 1] == '.') ||
        (at >= 2 && code[at - 2] == '-' && code[at - 1] == '>');
}

bool starts_with_components(std::span<const std::string_view> name,
    std::span<const std::string_view> pattern) {
    return pattern.size() <= name.size() &&
        std::ranges::equal(pattern, name.first(pattern.size()));
}

std::string_view include_target(std::string_view listed, bool& angled) {
    angled = listed.size() >= 2 && listed.front() == '<';
    if (listed.size() >= 2 && (listed.front() == '<' || listed.front() == '"'))
        return listed.substr(1, listed.size() - 2);
    return listed;
}

struct hit {
    std::uint32_t line;
    std::uint32_t rule;
    std::string match;
};

}

std::expected<forbid_definition, std::string>
parse_forbid_definition(std::string_view text) {
    forbid_definition d;
    std::set<std::string, std::less<>> seen;
    std::set<std::string, std::less<>> rule_names;
    std::uint32_t line = 0;
    std::uint32_t rule_line = 0;
    const auto fail = [&](std::uint32_t at, const std::string& what) {
        return std::unexpected("line " + std::to_string(at) + ": " + what);
    };
    const auto finish_rule = [&]() -> std::optional<std::string> {
        if (d.rules.empty())
            return std::nullopt;
        const auto& r = d.rules.back();
        if (r.cpp_names.empty() && r.cpp_includes.empty() &&
            r.cpp_text.empty() && r.text.empty())
            return "rule " + r.name + " forbids nothing; give it CPP_NAME, "
                "CPP_INCLUDE, CPP_TEXT or TEXT";
        if (r.message.empty())
            return "rule " + r.name + " has no MESSAGE";
        return std::nullopt;
    };

    for (const auto raw : split_lines_keeping_ends(text)) {
        ++line;
        const auto l = trimmed(raw.substr(0, raw.find('\n')));
        if (l.empty())
            continue;
        const auto space = l.find(' ');
        const auto key = l.substr(0, space);
        const auto value = space == std::string_view::npos ? std::string_view{}
            : trimmed(l.substr(space + 1));
        if (key == "RULE") {
            if (auto problem = finish_rule())
                return fail(rule_line, *problem);
            if (value.empty() || value.find(' ') != std::string_view::npos)
                return fail(line, "RULE takes one name");
            if (!rule_names.emplace(value).second)
                return fail(line, "rule " + std::string(value) +
                    " is defined twice");
            d.rules.push_back({.name = std::string(value)});
            rule_line = line;
            seen.clear();
            continue;
        }
        if (!seen.emplace(key).second)
            return fail(line, std::string(key) + " is given twice");
        if (value.empty())
            return fail(line, std::string(key) + " has no value");
        if (key == "FILES" || key == "OUTSIDE") {
            if (!d.rules.empty())
                return fail(line, std::string(key) +
                    " comes before the first RULE");
            auto& globs = key == "FILES" ? d.files : d.outside;
            for (const auto& w : words_of(value)) {
                auto expanded = expand_braces(w);
                if (!expanded)
                    return fail(line, expanded.error());
                for (auto& g : *expanded) {
                    if (g == "**" || g.ends_with("/**"))
                        g += "/*";
                    globs.push_back(std::move(g));
                }
            }
            continue;
        }
        if (d.rules.empty())
            return fail(line, std::string(key) + " is not a setting of the "
                "definition; FILES and OUTSIDE are, and the rest follow RULE");
        auto& r = d.rules.back();
        if (key == "CPP_NAME") {
            r.cpp_names = words_of(value);
        } else if (key == "CPP_INCLUDE") {
            r.cpp_includes = words_of(value);
        } else if (key == "CPP_TEXT" || key == "TEXT") {
            try {
                static_cast<void>(compiled(std::string(value)));
            } catch (const std::invalid_argument& e) {
                return fail(line, e.what());
            }
            (key == "TEXT" ? r.text : r.cpp_text) = std::string(value);
        } else if (key == "MESSAGE") {
            r.message = std::string(value);
        } else if (key == "FIX") {
            r.fix = std::string(value);
        } else {
            return fail(line, "unknown key " + std::string(key));
        }
    }
    if (auto problem = finish_rule())
        return fail(rule_line, *problem);
    if (d.files.empty())
        return std::unexpected("the definition has no FILES");
    if (d.rules.empty())
        return std::unexpected("the definition has no RULE");
    return d;
}

bool covers(const forbid_definition& d, std::string_view path) {
    const auto matches = [&](const std::string& g) {
        return glob_matches(g, path);
    };
    return std::ranges::any_of(d.files, matches) &&
        std::ranges::none_of(d.outside, matches);
}

std::vector<std::string_view> covered_files(
    const compass::codec::types::corpus_view& v, const forbid_definition& d) {
    std::vector<std::string_view> out;
    for (const auto& g : d.files) {
        for (const auto p : glob(v, g)) {
            if (std::ranges::none_of(d.outside, [&](const std::string& o) {
                    return glob_matches(o, p); }))
                out.push_back(p);
        }
    }
    std::ranges::sort(out);
    const auto [first, last] = std::ranges::unique(out);
    out.erase(first, last);
    return out;
}

void forbid_scan(const forbid_definition& d, std::string_view path,
    std::string_view bytes, compass::codec::types::diagnostic_table& out) {
    std::vector<hit> hits;
    const auto add = [&](std::size_t offset, std::size_t rule,
        std::string_view match) {
        hits.push_back({line_of(bytes, offset),
            static_cast<std::uint32_t>(rule), std::string(match)});
    };

    bool needs_code = false;
    for (std::size_t i = 0; i < d.rules.size(); ++i) {
        const auto& r = d.rules[i];
        if (!r.text.empty()) {
            for (const auto& m : compiled(r.text).find_all(bytes))
                add(m.start(), i, m.group());
        }
        needs_code = needs_code || !r.cpp_text.empty() ||
            std::ranges::any_of(r.cpp_names, [&](const std::string& n) {
                const auto parts = components_of_name(n);
                return !parts.empty() &&
                    bytes.find(parts.back()) != std::string_view::npos; }) ||
            std::ranges::any_of(r.cpp_includes, [&](const std::string& n) {
                bool angled = false;
                return bytes.find(include_target(n, angled)) !=
                    std::string_view::npos; });
    }

    if (needs_code) {
        const auto lexed = lex(bytes);
        const auto model = model_of(bytes, lexed, model_parts::names);
        for (std::size_t i = 0; i < d.rules.size(); ++i) {
            const auto& r = d.rules[i];
            for (const auto& listed : r.cpp_names) {
                const auto pattern = components_of_name(listed);
                for (const auto& n : model.names) {
                    if (!is_member(lexed.code, n.offset) &&
                        starts_with_components(components_of_name(n.text),
                            pattern))
                        add(n.offset, i, listed);
                }
            }
            for (const auto& listed : r.cpp_includes) {
                bool angled = false;
                const auto target = include_target(listed, angled);
                for (const auto& inc : model.includes) {
                    if (inc.target == target && inc.angled == angled)
                        add(inc.offset, i, listed);
                }
            }
            if (!r.cpp_text.empty()) {
                for (const auto& m : compiled(r.cpp_text).find_all(
                        lexed.code)) {
                    const auto at = static_cast<std::uint32_t>(m.start());
                    if (!lexed.is_dead(at))
                        add(at, i, m.group());
                }
            }
        }
    }

    std::ranges::stable_sort(hits, {}, [](const hit& h) {
        return std::tie(h.line, h.rule); });
    const hit* last = nullptr;
    for (const auto& h : hits) {
        if (last != nullptr && last->line == h.line && last->rule == h.rule)
            continue;
        last = &h;
        const auto& r = d.rules[h.rule];
        report(out, path, offset_of_line(bytes, h.line), h.rule,
            with_match(r.message, h.match), with_match(r.fix, h.match));
    }
}

}
