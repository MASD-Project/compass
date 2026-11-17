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
#include "compass.sql/codecs/populate_refs.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"
#include "compass.sql/ops/scripts.hpp"

namespace compass::sql::codecs {

namespace {

namespace ct = compass::codec::types;
namespace pt = compass::platform::text;

constexpr std::array<std::string_view, 1> unresolved_reference_sources{
    "7B477025-132E-4B20-80B4-D8C9420880B8"};

constexpr std::array<ct::rule, 2> rules{{
    {"unresolved_reference", "Every name a populate script looks up is "
        "one a populate script defines.", unresolved_reference_sources},
    {"unbalanced_parentheses", "Every call and definition the check reads "
        "closes its parentheses."}}};

constexpr std::string_view create_dir = "projects/ores.sql/create/";
constexpr std::string_view populate_dir = "projects/ores.sql/populate/";

const std::set<std::string, std::less<>> non_identifying{"tenant_id",
    "valid_from", "valid_to"};

struct unbalanced : std::runtime_error {
    std::size_t at;
    explicit unbalanced(std::size_t a)
        : std::runtime_error("unbalanced parentheses"), at(a) {}
};

/**
 * @brief The text inside the parentheses opened at @p open, and the
 * position after the closing one.
 */
std::pair<std::string_view, std::size_t> read_balanced(std::string_view text,
    std::size_t open) {
    int depth = 0;
    std::optional<char> quote;
    for (std::size_t i = open; i < text.size(); ++i) {
        const char ch = text[i];
        if (quote) {
            if (ch == *quote) {
                if (i + 1 < text.size() && text[i + 1] == *quote) {
                    ++i;
                    continue;
                }
                quote.reset();
            }
            continue;
        }
        if (ch == '\'' || ch == '"') {
            quote = ch;
        } else if (ch == '(') {
            ++depth;
        } else if (ch == ')') {
            if (--depth == 0)
                return {text.substr(open + 1, i - open - 1), i + 1};
        }
    }
    throw unbalanced(open);
}

std::string lower(std::string_view s) {
    std::string out(s);
    std::ranges::transform(out, out.begin(), [](unsigned char c) {
        return static_cast<char>(c < 0x80 ? std::tolower(c) : c);
    });
    return out;
}

std::optional<std::string> literal_value(std::string_view argument) {
    if (argument.size() < 2 || !argument.starts_with('\'') ||
        !argument.ends_with('\''))
        return std::nullopt;
    std::string out;
    const auto inner = argument.substr(1, argument.size() - 2);
    for (std::size_t i = 0; i < inner.size(); ++i) {
        out.push_back(inner[i]);
        if (inner[i] == '\'' && i + 1 < inner.size() && inner[i + 1] == '\'')
            ++i;
    }
    return out;
}

struct function {
    std::vector<std::string> params;
    std::string_view body;
};

using pairs_t = std::vector<std::pair<std::string, std::string>>;

struct guard {
    std::string table;
    pairs_t pairs;
};

using insert_map =
    std::map<std::string, std::map<std::string, std::string>, std::less<>>;

/**
 * @brief Function name to parameters and body, in the order each name is
 * first defined; a later definition replaces the earlier one.
 */
std::vector<std::pair<std::string, function>> parse_functions(
    std::string_view text) {
    static const pt::regex function_re(
        R"(create\s+or\s+replace\s+function\s+(\w+)\s*\()",
        {.ignorecase = true});
    static const pt::regex body_tag_re(R"(\bas\s+(\$[A-Za-z_]*\$))",
        {.ignorecase = true});
    std::vector<std::pair<std::string, function>> out;
    for (const auto& m : function_re.find_all(text)) {
        auto name = lower(m.group(1));
        const auto [params_text, after] = read_balanced(text, m.end() - 1);
        std::vector<std::string> params;
        for (const auto& p : ops::split_top(params_text)) {
            if (ops::strip(p).empty())
                continue;
            params.push_back(lower(ops::first_word(p)));
        }
        const auto tag = body_tag_re.search(text, after);
        if (!tag)
            continue;
        const auto end = text.find(tag->group(1), tag->end());
        if (end == std::string_view::npos)
            continue;
        function f{std::move(params),
            text.substr(tag->end(), end - tag->end())};
        const auto it = std::ranges::find_if(out,
            [&](const auto& e) { return e.first == name; });
        if (it == out.end())
            out.emplace_back(std::move(name), std::move(f));
        else
            it->second = std::move(f);
    }
    return out;
}

std::vector<guard> parse_guards(std::string_view body) {
    static const pt::regex raise_re(
        R"(raise\s+exception\s+'(?:[^']|'')*not\s+found)",
        {.ignorecase = true});
    static const pt::regex select_re(
        R"(\bselect\b.*?\bfrom\s+(\w+)\s+where\b(.*?);)",
        {.dotall = true, .ignorecase = true});
    static const pt::regex column_param_re(R"((\w+)\s*=\s*(p_\w+))",
        {.ignorecase = true});
    std::vector<guard> out;
    for (const auto& r : raise_re.find_all(body)) {
        const auto selects = select_re.find_all(body.substr(0, r.start()));
        if (selects.empty())
            continue;
        const auto& last = selects.back();
        guard g{lower(last.group(1)), {}};
        for (const auto& cp : column_param_re.find_all(last.group(2))) {
            auto column = lower(cp.group(1));
            if (!non_identifying.contains(column))
                g.pairs.emplace_back(std::move(column), lower(cp.group(2)));
        }
        if (!g.pairs.empty())
            out.push_back(std::move(g));
    }
    return out;
}

insert_map parse_inserts(std::string_view body) {
    static const pt::regex insert_re(R"(insert\s+into\s+(\w+)\s*\()",
        {.ignorecase = true});
    static const pt::regex values_re(R"(\s*values\s*\()",
        {.ignorecase = true});
    insert_map out;
    for (const auto& m : insert_re.find_all(body)) {
        const auto table = lower(m.group(1));
        const auto [columns_text, after] = read_balanced(body, m.end() - 1);
        const auto v = values_re.match(body, after);
        if (!v)
            continue;
        const auto values_text = read_balanced(body, v->end() - 1).first;
        const auto columns = ops::split_top(columns_text);
        const auto values = ops::split_top(values_text);
        if (columns.size() != values.size())
            continue;
        auto& row = out[table];
        for (std::size_t i = 0; i < columns.size(); ++i)
            row[lower(ops::strip(columns[i]))] = std::string(values[i]);
    }
    return out;
}

struct call {
    std::string_view path;
    std::string function;
    std::vector<std::string> arguments;
    std::uint32_t line;
    std::uint32_t offset;
};

using values_t = std::vector<std::string>;

std::vector<std::pair<values_t, const call*>> call_tuples(
    const std::vector<call>& calls, std::string_view name,
    const std::vector<std::size_t>& positions) {
    std::vector<std::pair<values_t, const call*>> out;
    for (const auto& c : calls) {
        if (c.function != name)
            continue;
        if (std::ranges::any_of(positions,
                [&](auto p) { return p >= c.arguments.size(); }))
            continue;
        values_t values;
        bool literal = true;
        for (const auto p : positions) {
            auto v = literal_value(c.arguments[p]);
            if (!v) {
                literal = false;
                break;
            }
            values.push_back(std::move(*v));
        }
        if (literal)
            out.emplace_back(std::move(values), &c);
    }
    return out;
}

std::string join(const std::vector<std::string>& items) {
    std::string out;
    for (const auto& i : items) {
        if (!out.empty())
            out += ", ";
        out += i;
    }
    return out;
}

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

std::vector<std::string_view> sql_files(const ct::corpus_view& v,
    std::string_view dir) {
    auto paths = v.list_files(dir, ".sql");
    std::ranges::sort(paths, ops::component_less);
    return paths;
}

void check(const ct::corpus_view& v, ct::diagnostic_table& out) {
    std::string create_text;
    std::vector<std::pair<std::string_view, std::size_t>> starts;
    for (const auto p : sql_files(v, create_dir)) {
        if (!starts.empty())
            create_text.push_back('\n');
        starts.emplace_back(p, create_text.size());
        create_text += ops::blank_comments(
            pt::universal_newlines(v.read_file(p).value_or("")));
    }
    std::vector<std::pair<std::string, function>> functions;
    try {
        functions = parse_functions(create_text);
    } catch (const unbalanced& e) {
        auto it = std::ranges::upper_bound(starts, e.at, {},
            [](const auto& s) { return s.second; });
        --it;
        add(out, it->first, static_cast<std::uint32_t>(e.at - it->second), 1,
            "a function's parameters do not close", "close the parentheses");
        return;
    }
    if (functions.empty()) {
        add(out, create_dir.substr(0, create_dir.size() - 1), 0, 1,
            "no function definitions found under the create scripts",
            "point the check at the schema's create scripts");
        return;
    }

    std::map<std::string, std::vector<guard>, std::less<>> guards;
    std::map<std::string, insert_map, std::less<>> inserts;
    std::set<std::string, std::less<>> guarded_tables;
    for (const auto& [name, f] : functions) {
        guards[name] = parse_guards(f.body);
        inserts[name] = parse_inserts(f.body);
        for (const auto& g : guards[name])
            guarded_tables.insert(g.table);
    }
    std::vector<std::string> interesting;
    for (const auto& [name, found] : guards) {
        const auto writes_guarded = std::ranges::any_of(inserts[name],
            [&](const auto& t) { return guarded_tables.contains(t.first); });
        if (!found.empty() || writes_guarded)
            interesting.push_back(name);
    }

    std::vector<call> calls;
    if (!interesting.empty()) {
        auto names = interesting;
        std::ranges::stable_sort(names, [](const auto& a, const auto& b) {
            return a.size() > b.size();
        });
        std::string alternation;
        for (const auto& n : names) {
            if (!alternation.empty())
                alternation += '|';
            alternation += pt::escape(n);
        }
        const pt::regex call_re(R"(\b()" + alternation + R"()\s*\()",
            {.ignorecase = true});
        for (const auto p : sql_files(v, populate_dir)) {
            const auto bytes = v.read_file(p).value_or("");
            const auto text = ops::blank_comments(
                pt::universal_newlines(bytes));
            for (const auto& m : call_re.find_all(text)) {
                std::string_view arguments;
                try {
                    arguments = read_balanced(text, m.end() - 1).first;
                } catch (const unbalanced&) {
                    add(out, p, static_cast<std::uint32_t>(m.start()), 1,
                        "a call does not close its parentheses",
                        "close the parentheses");
                    return;
                }
                std::vector<std::string> args;
                for (const auto a : ops::split_top(arguments))
                    args.emplace_back(ops::strip(a));
                const auto line = static_cast<std::uint32_t>(std::count(
                    text.begin(), text.begin() + static_cast<long>(m.start()),
                    '\n') + 1);
                calls.push_back({p, lower(m.group(1)), std::move(args), line,
                    ops::offset_of_line(bytes, line)});
            }
        }
    }

    std::map<std::pair<std::string, std::vector<std::string>>,
        std::vector<std::pair<std::string, std::vector<std::size_t>>>>
        defining_cache;
    auto sorted = functions;
    std::ranges::sort(sorted, {}, [](const auto& e) { return e.first; });
    for (const auto& [name, f] : sorted) {
        for (const auto& g : guards[name]) {
            std::vector<std::size_t> reference_positions;
            for (const auto& [column, param] : g.pairs) {
                const auto it = std::ranges::find(f.params, param);
                if (it != f.params.end())
                    reference_positions.push_back(
                        static_cast<std::size_t>(it - f.params.begin()));
            }
            if (reference_positions.size() != g.pairs.size())
                continue;
            std::vector<std::string> columns;
            for (const auto& pair : g.pairs)
                columns.push_back(pair.first);

            const auto key = std::make_pair(g.table, columns);
            if (!defining_cache.contains(key)) {
                auto& found = defining_cache[key];
                for (const auto& [candidate, cf] : functions) {
                    const auto& by_table = inserts[candidate];
                    const auto row = by_table.find(g.table);
                    if (row == by_table.end() || row->second.empty())
                        continue;
                    std::vector<std::size_t> positions;
                    bool all = true;
                    for (const auto& column : columns) {
                        const auto e = row->second.find(column);
                        if (e == row->second.end()) {
                            all = false;
                            break;
                        }
                        const auto expression = lower(e->second);
                        const auto it = std::ranges::find(cf.params,
                            expression);
                        if (it == cf.params.end()) {
                            all = false;
                            break;
                        }
                        positions.push_back(
                            static_cast<std::size_t>(it - cf.params.begin()));
                    }
                    if (all && !positions.empty())
                        found.emplace_back(candidate, std::move(positions));
                }
            }

            std::set<values_t> definitions;
            for (const auto& [candidate, positions] : defining_cache[key]) {
                for (auto& [values, c] :
                    call_tuples(calls, candidate, positions))
                    definitions.insert(std::move(values));
            }

            std::vector<std::string> params;
            for (const auto& pair : g.pairs)
                params.push_back(pair.second);
            for (const auto& [values, c] :
                call_tuples(calls, name, reference_positions)) {
                if (definitions.contains(values))
                    continue;
                add(out, c->path, c->offset, 0, name + "(" + join(params) +
                    ") looks up " + g.table + " (" + join(columns) +
                    ") but no populate script defines (" + join(values) +
                    ")", "define the row in a populate script that runs "
                    "before this one, or correct the name");
            }
        }
    }
}

}

const ct::descriptor populate_refs{
    .name = "populate_refs",
    .version = 1,
    .summary = "Reports each populate call that looks up a name no "
        "populate script defines.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{check}};

}
