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
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codec/types/fact.hpp"
#include "compass.sql/ops/includes.hpp"
#include "compass.sql/ops/scripts.hpp"
#include "compass.sql/ops/sql_model.hpp"

namespace compass::sql::codecs {

namespace {

namespace ct = compass::codec::types;

constexpr std::array<std::string_view, 1> unresolved_reference_sources{
    "7B477025-132E-4B20-80B4-D8C9420880B8"};

constexpr std::uint32_t unresolved_reference = 0;
constexpr std::uint32_t unbalanced_parentheses = 1;
constexpr std::uint32_t unread_function = 2;

constexpr std::array<ct::rule, 3> rules{{
    {"unresolved_reference", "Every name a populate script looks up is "
        "one a populate script that runs before it defines.",
        unresolved_reference_sources, ct::blame_method::line},
    {"unbalanced_parentheses", "Every call and definition the check reads "
        "closes its parentheses.", {}, ct::blame_method::line},
    {"unread_function", "Every function a populate lookup involves is "
        "named *_fn, as the check reads only those calls.",
        {}, ct::blame_method::line}}};

constexpr std::string_view create_dir = "projects/ores.sql/create/";
constexpr std::string_view populate_dir = "projects/ores.sql/populate/";

const std::set<std::string, std::less<>> non_identifying{"tenant_id",
    "valid_from", "valid_to"};

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

std::vector<std::string_view> scanned(const ct::corpus_view& v) {
    auto paths = v.list_files(std::string(ops::sql_root) + "/", ".sql");
    std::ranges::sort(paths, ops::component_less);
    return paths;
}

/**
 * @brief The fact of a file: its includes; for a create script, each
 * function with its parameters, guards and inserts; for a populate
 * script, each call with a literal argument.
 */
void map(std::string_view path, std::string_view bytes,
    const ct::option_values&, ct::file_scan& out) {
    const bool create = path.starts_with(create_dir);
    const bool populate = path.starts_with(populate_dir);
    const auto tokens = ops::lex(bytes);
    ct::fact_writer fact(out.fact);
    if (!create && !populate) {
        ops::write_includes(fact, ops::includes_of(bytes, tokens));
        fact.number(0).number(0);
        return;
    }
    const auto model = ops::model_of(bytes, tokens);
    ops::write_includes(fact, model.includes);
    for (const auto& u : model.unbalanced) {
        if (u.call ? populate : create)
            add(out.findings, path, u.offset, unbalanced_parentheses,
                u.call ? "a call does not close its parentheses"
                       : "a function's parameters do not close",
                "close the parentheses");
    }
    static const std::vector<ops::sql_function> no_functions;
    static const std::vector<ops::sql_call> no_calls;
    const auto& functions = create ? model.functions : no_functions;
    const auto& calls = populate ? model.calls : no_calls;
    fact.number(static_cast<std::uint32_t>(functions.size()));
    for (const auto& f : functions) {
        fact.text(f.name).number(f.offset)
            .number(static_cast<std::uint32_t>(f.parameters.size()));
        for (const auto& p : f.parameters)
            fact.text(p);
        fact.number(static_cast<std::uint32_t>(f.guards.size()));
        for (const auto& g : f.guards) {
            fact.text(g.table).number(
                static_cast<std::uint32_t>(g.columns.size()));
            for (const auto& [column, param] : g.columns)
                fact.text(column).text(param);
        }
        fact.number(static_cast<std::uint32_t>(f.inserts.size()));
        for (const auto& in : f.inserts) {
            fact.text(in.table).number(
                static_cast<std::uint32_t>(in.values.size()));
            for (const auto& [column, value] : in.values)
                fact.text(column).text(value);
        }
    }
    fact.number(static_cast<std::uint32_t>(calls.size()));
    for (const auto& c : calls) {
        fact.text(c.function).number(c.offset)
            .number(static_cast<std::uint32_t>(c.arguments.size()));
        for (const auto& a : c.arguments) {
            fact.number(a ? 1 : 0);
            if (a)
                fact.text(*a);
        }
    }
}

using values_t = std::vector<std::string>;

struct function {
    std::string_view path;
    std::uint32_t offset = 0;
    std::vector<std::string> params;
    std::vector<ops::sql_guard> guards;
    std::map<std::string, std::map<std::string, std::string>, std::less<>>
        inserts;
};

struct call {
    std::string_view path;
    std::uint32_t offset = 0;
    std::vector<std::optional<std::string>> arguments;
};

/**
 * @brief The literal values of @p c at @p positions, or nothing when one
 * is missing or not a literal.
 */
std::optional<values_t> values_at(const call& c,
    const std::vector<std::size_t>& positions) {
    values_t out;
    for (const auto p : positions) {
        if (p >= c.arguments.size() || !c.arguments[p])
            return std::nullopt;
        out.push_back(*c.arguments[p]);
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

std::string lower(std::string_view s) {
    return ops::lowered(s);
}

void reduce(std::span<const ct::scanned_file> files,
    const ct::corpus_view& v, ct::diagnostic_table& out) {
    std::unordered_map<std::string_view, std::vector<ops::sql_include>>
        includes;
    std::vector<std::pair<std::string, function>> functions;
    std::map<std::string, std::vector<call>, std::less<>> calls;
    std::vector<std::string_view> roots;
    for (const auto& f : files) {
        ct::fact_reader fact(f.fact);
        includes.emplace(f.path, ops::read_includes(fact));
        if (f.path.find('/', ops::sql_root.size() + 1) ==
            std::string_view::npos)
            roots.push_back(f.path);
        for (auto n = fact.number(); n > 0; --n) {
            std::string name(fact.text());
            function fn{f.path, fact.number(), {}, {}, {}};
            fn.params.resize(fact.number());
            for (auto& p : fn.params)
                p = fact.text();
            fn.guards.resize(fact.number());
            for (auto& g : fn.guards) {
                g.table = fact.text();
                g.columns.resize(fact.number());
                for (auto& [column, param] : g.columns) {
                    column = fact.text();
                    param = fact.text();
                }
            }
            for (auto inserts = fact.number(); inserts > 0; --inserts) {
                auto& row = fn.inserts[std::string(fact.text())];
                for (auto values = fact.number(); values > 0; --values) {
                    std::string column(fact.text());
                    row[column] = fact.text();
                }
            }
            const auto it = std::ranges::find_if(functions,
                [&](const auto& e) { return e.first == name; });
            if (it == functions.end())
                functions.emplace_back(std::move(name), std::move(fn));
            else
                it->second = std::move(fn);
        }
        for (auto n = fact.number(); n > 0; --n) {
            std::string name(fact.text());
            call c{f.path, fact.number(), {}};
            c.arguments.resize(fact.number());
            for (auto& a : c.arguments) {
                if (fact.number() == 1)
                    a = std::string(fact.text());
            }
            calls[name].push_back(std::move(c));
        }
    }
    if (functions.empty()) {
        add(out, create_dir.substr(0, create_dir.size() - 1), 0,
            unbalanced_parentheses,
            "no function definitions found under the create scripts",
            "point the check at the schema's create scripts");
        return;
    }

    const auto walk = ops::walk_includes(roots,
        [&](std::string_view p) {
            const auto it = includes.find(p);
            return it == includes.end() ? std::vector<ops::sql_include>{}
                                        : it->second;
        },
        [&](std::string_view p) { return v.file_exists(p); });

    std::map<std::string, std::vector<ops::sql_guard>, std::less<>> guards;
    std::set<std::string, std::less<>> guarded_tables;
    for (const auto& [name, f] : functions) {
        auto& kept = guards[name];
        for (const auto& g : f.guards) {
            ops::sql_guard k{g.table, {}};
            for (const auto& pair : g.columns) {
                if (!non_identifying.contains(pair.first))
                    k.columns.push_back(pair);
            }
            if (!k.columns.empty())
                kept.push_back(std::move(k));
        }
        for (const auto& g : kept)
            guarded_tables.insert(g.table);
    }
    for (const auto& [name, f] : functions) {
        const auto writes_guarded = std::ranges::any_of(f.inserts,
            [&](const auto& t) { return guarded_tables.contains(t.first); });
        if ((guards[name].empty() && !writes_guarded) || name.ends_with("_fn"))
            continue;
        add(out, f.path, f.offset, unread_function, "function " + name +
            " takes part in a populate lookup, but the check reads only "
            "the calls of functions named *_fn",
            "rename the function to end in _fn");
    }

    const std::vector<call> no_calls;
    const auto calls_of = [&](std::string_view name) -> const auto& {
        const auto it = calls.find(name);
        return it == calls.end() ? no_calls : it->second;
    };

    std::map<std::pair<std::string, std::vector<std::string>>,
        std::vector<std::pair<std::string, std::vector<std::size_t>>>>
        defining_cache;
    auto sorted = functions;
    std::ranges::sort(sorted, {}, [](const auto& e) { return e.first; });
    for (const auto& [name, f] : sorted) {
        for (const auto& g : guards[name]) {
            std::vector<std::size_t> reference_positions;
            for (const auto& [column, param] : g.columns) {
                const auto it = std::ranges::find(f.params, param);
                if (it != f.params.end())
                    reference_positions.push_back(
                        static_cast<std::size_t>(it - f.params.begin()));
            }
            if (reference_positions.size() != g.columns.size())
                continue;
            std::vector<std::string> columns;
            for (const auto& pair : g.columns)
                columns.push_back(pair.first);

            const auto key = std::make_pair(g.table, columns);
            if (!defining_cache.contains(key)) {
                auto& found = defining_cache[key];
                for (const auto& [candidate, cf] : functions) {
                    const auto row = cf.inserts.find(g.table);
                    if (row == cf.inserts.end() || row->second.empty())
                        continue;
                    std::vector<std::size_t> positions;
                    bool all = true;
                    for (const auto& column : columns) {
                        const auto e = row->second.find(column);
                        if (e == row->second.end()) {
                            all = false;
                            break;
                        }
                        const auto it = std::ranges::find(cf.params,
                            lower(e->second));
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

            // Each value set defined, with the earliest point psql runs a
            // definition of it, when an entry point reaches one.
            std::map<values_t, std::optional<ops::run_point>> definitions;
            for (const auto& [candidate, positions] : defining_cache[key]) {
                for (const auto& c : calls_of(candidate)) {
                    auto values = values_at(c, positions);
                    if (!values)
                        continue;
                    const auto at = walk.when(c.path, c.offset);
                    auto [it, added] = definitions.try_emplace(
                        std::move(*values), at);
                    if (!added && at && (!it->second || *at < *it->second))
                        it->second = at;
                }
            }

            std::vector<std::string> params;
            for (const auto& pair : g.columns)
                params.push_back(pair.second);
            const auto head = name + "(" + join(params) + ") looks up " +
                g.table + " (" + join(columns) + ")";
            const std::string_view fix = "define the row in a populate "
                "script that runs before this one, or correct the name";
            for (const auto& c : calls_of(name)) {
                const auto values = values_at(c, reference_positions);
                if (!values)
                    continue;
                const auto d = definitions.find(*values);
                if (d == definitions.end()) {
                    add(out, c.path, c.offset, unresolved_reference, head +
                        " but no populate script defines (" + join(*values) +
                        ")", fix);
                    continue;
                }
                const auto used = walk.when(c.path, c.offset);
                if (!used || (d->second && *d->second < *used))
                    continue;
                add(out, c.path, c.offset, unresolved_reference, head +
                    " but (" + join(*values) + ") is defined only by a "
                    "populate call that psql runs after this one, or that "
                    "no entry point reaches", fix);
            }
        }
    }
}

}

const ct::descriptor populate_refs{
    .name = "populate_refs",
    .version = 2,
    .summary = "Reports each populate call that looks up a name no "
        "populate script that runs before it defines.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::scan_function{scanned, map, reduce}};

}
