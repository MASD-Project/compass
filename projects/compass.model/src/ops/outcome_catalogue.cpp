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
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include "loaders.hpp"
#include "text.hpp"

namespace compass::model::ops {

namespace {

using types::value_id;

constexpr std::string_view outcome_classes[] = {
    "ok", "invalid", "denied", "missing", "conflict", "unavailable", "failed"};

constexpr std::string_view argument_names[] = {
    "entity", "field", "value", "expected", "current", "reason", "limit"};

template <std::size_t N>
bool one_of(std::string_view s, const std::string_view (&set)[N]) {
    return std::ranges::find(set, s) != std::end(set);
}

bool is_word(char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9') || ch == '_';
}

/**
 * @brief [a-z][a-z0-9_]*, matched in full.
 */
bool is_snake_identifier(std::string_view s) {
    if (s.empty() || !(s[0] >= 'a' && s[0] <= 'z'))
        return false;
    return std::ranges::all_of(s, [](char ch) {
        return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
            ch == '_';
    });
}

/**
 * @brief The names of \{(\w+)\} in @p s.
 */
std::set<std::string> placeholders(std::string_view s) {
    std::set<std::string> out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '{')
            continue;
        auto end = i + 1;
        while (end < s.size() && is_word(s[end]))
            ++end;
        if (end > i + 1 && end < s.size() && s[end] == '}')
            out.emplace(s.substr(i + 1, end - i - 1));
    }
    return out;
}

std::string escaped(std::string_view s, std::string_view chars,
    std::string_view prefix) {
    std::string out;
    for (const char ch : s) {
        if (chars.find(ch) != std::string_view::npos)
            out += prefix;
        out += ch;
    }
    return out;
}

void names_with_commas(builder& b, value_id list,
    const std::vector<std::string>& names) {
    for (std::size_t i = 0; i < names.size(); ++i) {
        const auto item = b.append_object(list);
        b.set(item, "name", names[i]);
        b.set(item, "comma", i + 1 == names.size() ? "" : ",");
    }
}

}

void load_outcome_catalogue(load_context& c, value_id root) {
    auto& b = c.b;
    const auto& o = c.o;
    ensure_profile_binding(o);
    const auto catalogue = b.object(root, "outcome_catalogue");
    for (const auto key : {"component", "brief"}) {
        if (const auto r = frontmatter(o, key); r.found)
            b.set(catalogue, key, r.value);
    }
    const auto nodes = children(o, section(o, node_id{0}, "Outcomes"));
    if (nodes.empty())
        throw model_error("no * Outcomes section");
    const auto outcomes = b.list(catalogue, "outcomes");
    std::vector<value_id> entries;
    std::vector<std::string> codes, sqlstates;
    std::vector<std::vector<std::string>> store_args;
    std::vector<bool> stores;
    for (const auto n : nodes) {
        std::map<std::string, std::string_view> props;
        for (const auto& p : properties(o, n))
            props[to_lower(p.key)] = p.value;
        const auto get = [&](const std::string& key, std::string_view fallback)
            -> std::string_view {
            const auto it = props.find(key);
            return it == props.end() ? fallback : it->second;
        };
        const std::string code(trim(get("code", o.titles[n.value])));
        if (!is_snake_identifier(code))
            throw model_error("outcome :code: is not a snake_case identifier");
        const auto coarse = trim(get("outcome", ""));
        if (!one_of(coarse, outcome_classes))
            throw model_error("outcome declares an unknown :outcome:");
        const auto producer = trim(get("producer", ""));
        if (producer != "store" && producer != "request")
            throw model_error("outcome :producer: is not store or request");
        const std::string sqlstate(trim(get("sqlstate", "")));
        if (producer == "store" && sqlstate.empty())
            throw model_error("store outcome declares no :sqlstate:");
        if (producer == "request" && !sqlstate.empty())
            throw model_error("request outcome declares a :sqlstate:");
        std::vector<std::string> args;
        std::string_view rest = get("args", "");
        for (;;) {
            const auto comma = rest.find(',');
            const auto a = trim(rest.substr(0, comma));
            if (!a.empty())
                args.emplace_back(a);
            if (comma == std::string_view::npos)
                break;
            rest.remove_prefix(comma + 1);
        }
        for (const auto& a : args) {
            if (!one_of(a, argument_names))
                throw model_error("outcome names an unknown argument");
        }
        const std::set<std::string> unique(args.begin(), args.end());
        if (unique.size() != args.size())
            throw model_error("outcome repeats an :args: name");
        const std::string message(trim(named_block(o, n, "message").value));
        if (message.empty())
            throw model_error("outcome has no message block");
        if (placeholders(message) != unique)
            throw model_error("outcome message and :args: disagree");

        const auto entry = b.append_object(outcomes);
        entries.push_back(entry);
        b.set(entry, "code", code);
        b.set(entry, "class", coarse);
        b.set(entry, "producer", producer);
        b.set(entry, "sqlstate", sqlstate);
        b.set(entry, "sqlstate_quoted",
            sqlstate.empty() ? std::string() : "'" + sqlstate + "'");
        names_with_commas(b, b.list(entry, "args"), args);
        const auto arg_names = b.list(entry, "arg_names");
        for (const auto& a : args)
            b.append(arg_names, a);
        b.set(entry, "message", message);
        b.set(entry, "message_sql", escaped(message, "'", "'"));
        b.set(entry, "message_cpp", escaped(message, "\\\"", "\\"));
        const auto text = body(o, n);
        b.set(entry, "description", text);
        b.set(entry, "summary", collapse_spaces(text));
        b.set(entry, "is_store", producer == "store");
        b.set(entry, "is_request", producer == "request");
        codes.push_back(code);
        if (!sqlstate.empty())
            sqlstates.push_back(sqlstate);
        stores.push_back(producer == "store");
        store_args.push_back(args);
    }
    if (std::set<std::string>(codes.begin(), codes.end()).size() !=
        codes.size())
        throw model_error("duplicate outcome code");
    if (std::set<std::string>(sqlstates.begin(), sqlstates.end()).size() !=
        sqlstates.size())
        throw model_error("SQLSTATE raised by more than one outcome");
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const bool last = i + 1 == entries.size();
        b.set(entries[i], "last", last);
        b.set(entries[i], "comma", last ? "" : ",");
    }
    const auto store = b.list(catalogue, "store_outcomes");
    const auto request = b.list(catalogue, "request_outcomes");
    for (std::size_t i = 0; i < entries.size(); ++i)
        b.append_copy(stores[i] ? store : request, entries[i]);
    std::vector<std::string> raiser;
    for (const auto a : argument_names) {
        for (std::size_t i = 0; i < entries.size(); ++i) {
            if (stores[i] && std::ranges::find(store_args[i], a) !=
                store_args[i].end()) {
                raiser.emplace_back(a);
                break;
            }
        }
    }
    names_with_commas(b, b.list(catalogue, "raiser_args"), raiser);
}

}
