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
#include "compass.codegen/ops/entity_steps.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include "compass.codegen/ops/values.hpp"
#include "entity_surfaces.hpp"
#include "entity_text.hpp"
#include "entity_wire.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::model_tables;
using compass::model::types::value_id;

namespace {

constexpr std::string_view list_type = "std::vector<std::string>";

/*
 * _SHELL_TOKEN_TYPES: the types a command fills from one token.
 */
bool is_token_type(std::string_view cpp) {
    constexpr std::string_view types[] = {"std::string", "bool", "int",
        "std::int32_t", "std::int64_t", "std::uint16_t", "std::uint32_t",
        "std::uint64_t", "double", "boost::uuids::uuid",
        "std::chrono::system_clock::time_point", "boost::asio::ip::address"};
    return std::ranges::find(types, cpp) != std::end(types);
}

using surfaces::sentinel_commentary;
using surfaces::sentinel_reason;
using surfaces::sentinel_value;

}

namespace surfaces {

std::string sentinel_value(std::string_view cpp_type) {
    constexpr std::pair<std::string_view, std::string_view> sentinels[] = {
        {"boost::uuids::uuid", "00000000-0000-0000-0000-000000000000"},
        {"utility::uuid::tenant_id", "00000000-0000-0000-0000-000000000000"},
        {"std::string", "__none__"}, {"bool", "false"}, {"int", "0"},
        {"std::int32_t", "0"}, {"std::int64_t", "0"}, {"std::uint16_t", "0"},
        {"std::uint32_t", "0"}, {"std::uint64_t", "0"}, {"std::size_t", "0"},
        {"double", "0"}, {"ores::utility::decimal::decimal", "1"},
        {"std::chrono::system_clock::time_point", "1970-01-01T00:00:00Z"},
        {"std::chrono::year_month_day", "1970-01-01"},
        {"boost::asio::ip::address", "0.0.0.0"},
        {"std::vector<std::string>", "__none__"}};
    const auto kind = values::strip(cpp_type);
    if (kind.starts_with("std::optional<"))
        return "-";
    for (const auto& [type, value] : sentinels) {
        if (type == kind)
            return std::string(value);
    }
    return "__none__";
}

std::string sentinel_for_field(std::string_view name,
    std::string_view cpp_type) {
    if (name == "reason_code")
        return std::string(sentinel_reason);
    if (name == "commentary")
        return std::string(sentinel_commentary);
    return sentinel_value(cpp_type);
}

}

namespace {

enum class supply { user, minted, session_party };

/*
 * One input a shell command reads, as entity_shell_plan's _input shapes
 * it; index is the offset a batch verb reads it at, on keys alone.
 */
struct shell_input {
    std::string name;
    std::string cpp_type;
    supply from = supply::user;
    std::optional<std::size_t> index{};

    bool is_list() const { return cpp_type == list_type; }
    bool is_bool() const { return cpp_type == "bool"; }
    bool fillable() const { return is_list() || is_token_type(cpp_type); }
};

struct shell_command {
    std::string command;
    std::string verb;
    std::string kind;
    std::string request;
    value_id response_type;
    std::string subject;
    std::vector<shell_input> keys;
    std::vector<shell_input> writes;
    std::optional<shell_input> relation;
    bool has_order = false;
    bool requires_session = true;
    std::string precondition;
    std::size_t positional_count = 0;
    bool exact_count = true;

    bool has_intent() const {
        return verb == "put" || verb == "put_many" || verb == "delete" ||
            verb == "delete_many";
    }
    bool allows_version() const {
        return (kind == "put" || kind == "delete") &&
            precondition != "must_not_exist";
    }
    std::vector<const shell_input*> put_writes() const {
        std::vector<const shell_input*> out;
        for (const auto& w : writes) {
            if (w.from == supply::user)
                out.push_back(&w);
        }
        return out;
    }
    /*
     * Only what the handler asks for: no key for a paged read, the write
     * record for a write, the relation for a scoped read.
     */
    std::vector<std::string> unsupported() const {
        std::vector<std::string> out;
        const auto add = [&](const shell_input& i) {
            if (!i.fillable())
                out.push_back(i.name);
        };
        if (kind == "list_by") {
            add(*relation);
        } else if (kind == "put" || kind == "put_many") {
            std::ranges::for_each(writes, add);
        } else if (kind != "paged") {
            std::ranges::for_each(keys, add);
        }
        std::ranges::sort(out);
        return out;
    }
};

struct shell_plan {
    std::vector<shell_command> commands;
    std::vector<std::string> uncovered_verbs;
};

/*
 * _entity_shell_invocation: the command line a generated script sends.
 */
std::string invocation(const shell_command& c) {
    std::vector<std::string> tokens{c.command};
    if (c.kind == "put_many") {
        tokens.insert(tokens.end(), {"--count", "1"});
        for (const auto& w : c.writes)
            tokens.push_back(sentinel_value(w.cpp_type));
    } else if (c.kind == "put") {
        for (const auto* w : c.put_writes())
            tokens.push_back(sentinel_value(w->cpp_type));
    } else if (c.kind == "list_by") {
        tokens.push_back(sentinel_value(c.relation->cpp_type));
    } else if (c.kind != "paged") {
        for (const auto& k : c.keys)
            tokens.push_back(sentinel_value(k.cpp_type));
    }
    if (c.has_intent()) {
        tokens.emplace_back(sentinel_reason);
        tokens.emplace_back(sentinel_commentary);
    }
    if (c.kind == "version_read")
        tokens.insert(tokens.end(), {"--version", "1"});
    if (c.has_order)
        tokens.insert(tokens.end(), {"--limit", "1"});
    std::string out;
    for (const auto& token : tokens)
        out += (out.empty() ? "" : " ") + token;
    return out;
}

/*
 * _entity_shell_usage: the one-line help a generated command registers.
 */
std::string usage(const shell_command& c) {
    std::vector<std::string> parts{c.command};
    const auto named = [&](const std::string& name) {
        parts.push_back("<" + name + ">");
    };
    if (c.kind == "put_many") {
        parts.emplace_back("--count <n>");
        for (const auto& w : c.writes)
            named(w.name);
    } else if (c.kind == "put") {
        for (const auto* w : c.put_writes())
            named(w->name);
    } else if (c.kind == "list_by") {
        named(c.relation->name);
    } else if (c.kind != "paged") {
        for (const auto& k : c.keys)
            named(k.name);
    }
    if (c.has_intent())
        parts.emplace_back("<reason> <commentary>");
    if (c.kind == "version_read")
        parts.emplace_back("--version <n>");
    else if (c.allows_version())
        parts.emplace_back("[--version <n>]");
    if (c.has_order) {
        parts.insert(parts.end(), {"[--offset <n>]", "[--limit <n>]",
            "[--order <field>]", "[--desc]"});
    }
    std::string out;
    for (const auto& part : parts)
        out += (out.empty() ? "" : " ") + part;
    return out;
}

/*
 * entity_shell_plan: one command per derived operation, a put as add and
 * set.
 */
shell_plan plan_shell(const model_tables& t, value_id entity) {
    std::vector<std::pair<std::string, supply>> supplies;
    const auto supplied = [&](value_id column) {
        const auto name = values::truthy(t, column, "name") ?
            values::py_str(t, values::member(t, column, "name")) :
            values::truthy(t, column, "column") ?
            values::py_str(t, values::member(t, column, "column")) :
            std::string();
        if (name.empty())
            return;
        const auto from = values::truthy(t, column, "is_minted") ?
            supply::minted : values::truthy(t, column, "is_session_party") ?
            supply::session_party : supply::user;
        const auto it = std::ranges::find(supplies, name,
            &std::pair<std::string, supply>::first);
        if (it == supplies.end())
            supplies.emplace_back(name, from);
        else
            it->second = from;
    };
    for (const auto c : values::items(t, entity, "columns"))
        supplied(c);
    for (const auto c : values::items(t,
             values::member(t, entity, "primary_key"), "columns"))
        supplied(c);
    const auto input = [&](std::string name, std::string_view cpp_type) {
        shell_input i{.name = std::move(name),
            .cpp_type = std::string(values::strip(
                cpp_type.empty() ? std::string_view("std::string") :
                cpp_type))};
        const auto it = std::ranges::find(supplies, i.name,
            &std::pair<std::string, supply>::first);
        if (it != supplies.end())
            i.from = it->second;
        return i;
    };

    std::vector<shell_input> keys;
    const auto declared = wire::declared_key_column(t, entity);
    if (!declared.is_none()) {
        keys.push_back(input(wire::column_name(t, declared),
            wire::key_cpp_type(t, declared)));
    } else {
        for (const auto c : values::items(t,
                 values::member(t, entity, "primary_key"), "columns"))
            keys.push_back(input(values::py_str(t,
                values::member(t, c, "column"), ""),
                values::str(t, c, "cpp_type")));
    }
    for (std::size_t i = 0; i < keys.size(); ++i) {
        keys[i].from = supply::user;
        keys[i].index = i;
    }
    const auto write_fields = values::items(t, entity, "write_fields");

    const auto command = [&](value_id operation, std::string kind,
        std::string name, std::string precondition = {},
        std::optional<shell_input> relation = {}, bool id_from_caller = false) {
        shell_command c{.command = std::move(name),
            .verb = values::py_str(t, values::member(t, operation, "verb"), ""),
            .kind = std::move(kind),
            .request = values::py_str(t, values::member(t, operation,
                "request")),
            .response_type = values::member(t, operation, "response"),
            .subject = values::py_str(t, values::member(t, operation,
                "subject")),
            .keys = keys, .writes = {}, .relation = std::move(relation),
            .has_order = values::truthy(t, operation, "has_order"),
            .requires_session = !values::member(t, operation,
                "requires_session").is_none() ?
                values::truthy(t, operation, "requires_session") : true,
            .precondition = std::move(precondition)};
        for (const auto f : write_fields) {
            auto i = input(values::py_str(t, values::member(t, f, "name"), ""),
                values::str(t, f, "cpp_type"));
            if (id_from_caller && i.from == supply::minted)
                i.from = supply::user;
            c.writes.push_back(std::move(i));
        }
        const auto& k = c.kind;
        if (k == "paged" || k == "list_by") {
            c.positional_count = k == "list_by" ? 1 : 0;
            c.exact_count = true;
        } else if (k == "put" || k == "put_many") {
            c.positional_count = write_fields.size();
            c.exact_count = k == "put";
        } else {
            c.positional_count = keys.size();
            c.exact_count = k != "delete_many" && k != "get_many";
        }
        return c;
    };

    constexpr std::array<std::array<std::string_view, 3>, 8> shapes{{
        {"list", "paged", "list"}, {"get", "key_read", "get"},
        {"get_many", "get_many", "get-many"},
        {"put_many", "put_many", "put-many"}, {"delete", "delete", "delete"},
        {"delete_many", "delete_many", "delete-many"},
        {"list_versions", "versions", "versions"},
        {"get_version", "version_read", "version"}}};

    shell_plan plan;
    std::set<std::string> uncovered;
    for (const auto o : values::items(t, entity, "operations")) {
        const auto verb = values::py_str(t, values::member(t, o, "verb"), "");
        const auto shape = std::ranges::find_if(shapes,
            [&](const auto& s) { return s[0] == verb; });
        if (shape == shapes.end() && verb != "put" && verb != "list_scoped")
            uncovered.insert(verb);
        if (verb == "put") {
            plan.commands.push_back(command(o, "put", "add", "must_not_exist"));
            plan.commands.push_back(command(o, "put", "set", "any", {}, true));
            continue;
        }
        std::string relation;
        if (verb == "list_scoped")
            relation = values::py_str(t, values::member(t, o, "leading"), "");
        else if (verb.starts_with("list_by_"))
            relation = verb.substr(8);
        if (!relation.empty()) {
            std::string dashed = relation;
            std::ranges::replace(dashed, '_', '-');
            plan.commands.push_back(command(o, "list_by", "by-" + dashed, {},
                input(relation, wire::column_cpp_type(t, entity, relation))));
            continue;
        }
        if (shape != shapes.end())
            plan.commands.push_back(command(o, std::string((*shape)[1]),
                std::string((*shape)[2])));
    }
    plan.uncovered_verbs.assign(uncovered.begin(), uncovered.end());
    return plan;
}

void write_input(builder& b, value_id o, const shell_input& i) {
    b.set(o, "name", i.name);
    b.set(o, "cpp_type", i.cpp_type);
    b.set(o, "is_user", i.from == supply::user);
    b.set(o, "is_minted", i.from == supply::minted);
    b.set(o, "is_session_party", i.from == supply::session_party);
    b.set(o, "is_string", i.cpp_type == "std::string");
    b.set(o, "is_bool", i.is_bool());
    b.set(o, "is_list", i.is_list());
    b.set(o, "needs_from_token",
        i.cpp_type != "std::string" && i.cpp_type != "bool");
    b.set(o, "fillable", i.fillable());
    if (i.index)
        b.set_integer(o, "index", static_cast<std::int64_t>(*i.index));
}

void write_inputs(builder& b, value_id parent, std::string_view key,
    const std::vector<shell_input>& inputs) {
    const auto list = b.list(parent, key);
    for (const auto& i : inputs)
        write_input(b, b.append_object(list), i);
}

void write_strings(builder& b, value_id parent, std::string_view key,
    const std::vector<std::string>& strings) {
    const auto list = b.list(parent, key);
    for (const auto& s : strings)
        b.append(list, s);
}

void write_relation(builder& b, value_id o, const shell_command& c) {
    if (c.relation)
        write_input(b, b.object(o, "relation"), *c.relation);
}

void write_command(builder& b, value_id o, const shell_command& c) {
    std::string identifier = c.command;
    std::ranges::replace(identifier, '-', '_');
    b.set(o, "command", c.command);
    b.set(o, "identifier", identifier);
    b.set(o, "verb", c.verb);
    b.set(o, "kind", c.kind);
    b.set(o, "request", c.request);
    b.set_copy(o, "response_type", c.response_type);
    b.set(o, "subject", c.subject);
    write_inputs(b, o, "keys", c.keys);
    write_inputs(b, o, "writes", c.writes);
    const auto put_writes = c.put_writes();
    const auto put_list = b.list(o, "put_writes");
    for (const auto* w : put_writes)
        write_input(b, b.append_object(put_list), *w);
    b.set_integer(o, "key_arity", static_cast<std::int64_t>(c.keys.size()));
    b.set_integer(o, "write_arity",
        static_cast<std::int64_t>(c.writes.size()));
    b.set_integer(o, "put_arity", static_cast<std::int64_t>(put_writes.size()));
    if (c.relation)
        write_relation(b, o, c);
    else
        b.set_null(o, "relation");
    b.set(o, "has_order", c.has_order);
    b.set(o, "has_intent", c.has_intent());
    b.set(o, "precondition", c.precondition);
    b.set(o, "allows_version", c.allows_version());
    b.set(o, "requires_session", c.requires_session);
    write_strings(b, o, "unsupported", c.unsupported());
    std::vector<std::string> flags;
    std::vector<std::string> switches;
    if (c.has_order) {
        flags = {"offset", "limit", "order"};
        switches = {"desc"};
    }
    if (c.kind == "list_by")
        flags.emplace_back("scope");
    if (c.kind == "version_read" || c.allows_version())
        flags.emplace_back("version");
    if (c.kind == "put_many")
        flags.emplace_back("count");
    write_strings(b, o, "flags", flags);
    write_strings(b, o, "switches", switches);
    for (const auto shape : {"paged", "list_by", "versions", "key_read",
             "version_read", "put", "put_many", "delete", "get_many",
             "delete_many"})
        b.set(o, std::string("is_") + shape, c.kind == shape);
    b.set_integer(o, "positional_count",
        static_cast<std::int64_t>(c.positional_count));
    b.set(o, "exact_count", c.exact_count);
    b.set(o, "usage", usage(c));
    b.set(o, "invocation", invocation(c));
}

void write_shell(builder& b, value_id entity, const shell_plan& plan) {
    const auto o = b.object(entity, "shell");
    const auto list = b.list(o, "commands");
    std::set<std::string_view> kinds;
    bool has_order = false;
    bool any_versioned = false;
    bool has_list = false;
    bool has_bool = false;
    bool has_minted = false;
    bool has_session_party = false;
    for (const auto& c : plan.commands) {
        write_command(b, b.append_object(list), c);
        kinds.insert(c.kind);
        has_order = has_order || c.has_order;
        any_versioned = any_versioned || c.allows_version();
        for (const auto* inputs : {&c.keys, &c.writes}) {
            for (const auto& i : *inputs) {
                has_list = has_list || i.is_list();
                has_bool = has_bool || i.is_bool();
                has_minted = has_minted || i.from == supply::minted;
                has_session_party = has_session_party ||
                    i.from == supply::session_party;
            }
        }
    }
    const auto any_kind = [&](std::initializer_list<std::string_view> ks) {
        return std::ranges::any_of(ks,
            [&](std::string_view k) { return kinds.contains(k); });
    };
    write_strings(b, o, "uncovered_verbs", plan.uncovered_verbs);
    b.set_integer(o, "command_count",
        static_cast<std::int64_t>(plan.commands.size()));
    b.set(o, "has_order", has_order);
    b.set(o, "has_list", has_list);
    b.set(o, "has_bool", has_bool);
    b.set(o, "has_minted", has_minted);
    b.set(o, "has_session_party", has_session_party);
    b.set(o, "has_batch", any_kind({"get_many", "put_many", "delete_many"}));
    b.set(o, "has_version", any_kind({"version_read", "put", "delete"}));
    b.set(o, "any_versioned", any_versioned);
    b.set(o, "has_helpers", !plan.commands.empty());
}

std::string words(std::string s) {
    std::ranges::replace(s, '_', ' ');
    return s;
}

std::string replace_all(std::string s, std::string_view from,
    std::string_view to) {
    for (auto at = s.find(from); at != std::string::npos;
         at = s.find(from, at + to.size()))
        s.replace(at, from.size(), to);
    return s;
}

/*
 * entity_http_route_plan: one route per shell command.
 */
void write_http_route(builder& b, value_id entity, const shell_plan& plan) {
    const auto& t = b.tables();
    const auto component = values::py_str(t,
        values::member(t, entity, "component"), "");
    const auto singular = values::py_str(t,
        values::member(t, entity, "entity_singular"), "");
    const auto plural = values::py_str(t,
        values::member(t, entity, "entity_plural"), "");
    const auto area_stated = values::strip(values::str(t, entity, "http_area"));
    const auto area = area_stated.empty() ? component : std::string(area_stated);
    const auto base = "/api/v1/" + area + "/" + plural;

    const auto o = b.object(entity, "http_route");
    const auto routes = b.list(o, "routes");
    bool any_page = false;
    for (const auto& c : plan.commands) {
        const auto& kind = c.kind;
        std::string key_path;
        for (const auto& k : c.keys)
            key_path += (key_path.empty() ? "{" : "/{") + k.name + "}";
        const auto relation_name = c.relation ? c.relation->name : std::string();
        std::string pattern = base;
        if (kind == "version_read") {
            pattern = base + "/" + key_path + "/versions/{version}";
        } else if (kind == "versions") {
            pattern = base + "/" + key_path + "/versions";
        } else if (kind == "list_by") {
            std::string dashed = relation_name;
            std::ranges::replace(dashed, '_', '-');
            pattern = base + "/by-" + dashed + "/{" + relation_name + "}";
        } else if (kind == "key_read" || kind == "delete") {
            pattern = base + "/" + key_path;
        } else if (kind == "get_many" || kind == "put_many" ||
            kind == "delete_many") {
            std::string dashed = kind;
            std::ranges::replace(dashed, '_', '-');
            pattern = base + "/" + dashed;
        }
        std::string method;
        std::string summary;
        if (kind == "put") {
            const auto create = c.precondition == "must_not_exist";
            method = create ? "post" : "put";
            summary = create ? "Create one {singular}" : "Replace one {singular}";
        } else {
            constexpr std::array<std::array<std::string_view, 3>, 9> shapes{{
                {"paged", "get", "List {plural}"},
                {"key_read", "get", "Get one {singular}"},
                {"get_many", "post", "Get many {plural}"},
                {"put_many", "post", "Write many {plural}"},
                {"delete", "delete_", "Delete one {singular}"},
                {"delete_many", "post", "Delete many {plural}"},
                {"versions", "get",
                 "List the recorded versions of one {singular}"},
                {"version_read", "get",
                 "Get one recorded version of one {singular}"},
                {"list_by", "get",
                 "List the {plural} that reference one {relation}"}}};
            const auto shape = std::ranges::find_if(shapes,
                [&](const auto& s) { return s[0] == kind; });
            method = std::string((*shape)[1]);
            summary = std::string((*shape)[2]);
        }
        summary = replace_all(summary, "{plural}", words(plural));
        summary = replace_all(summary, "{singular}", words(singular));
        summary = replace_all(summary, "{relation}", words(relation_name));
        const auto method_upper = method == "delete_" ? std::string("DELETE") :
            text::upper(method);
        const auto has_page = kind == "paged" || kind == "versions" ||
            kind == "list_by";
        any_page = any_page || has_page;

        std::string identifier = c.command;
        std::ranges::replace(identifier, '-', '_');
        const auto r = b.append_object(routes);
        b.set(r, "command", c.command);
        b.set(r, "identifier", identifier);
        b.set(r, "kind", kind);
        b.set(r, "method", method);
        b.set(r, "method_upper", method_upper);
        b.set(r, "pattern", pattern);
        b.set(r, "summary", summary);
        b.set(r, "description", "Forwards to the " + c.subject + " operation.");
        b.set(r, "subject", c.subject);
        b.set(r, "request", c.request);
        b.set_copy(r, "response_type", c.response_type);
        b.set(r, "requires_session", c.requires_session);
        write_inputs(b, r, "keys", c.keys);
        if (c.relation)
            write_relation(b, r, c);
        else
            b.object(r, "relation");
        b.set(r, "precondition", c.precondition);
        b.set(r, "has_order", c.has_order);
        b.set(r, "has_search",
            values::truthy(t, entity, "has_search") && kind == "paged");
        b.set(r, "has_scope", kind == "list_by");
        b.set(r, "allows_version", c.allows_version());
        b.set(r, "has_body", kind == "get_many" || kind == "put" ||
            kind == "put_many" || kind == "delete_many");
        b.set(r, "has_page", has_page);
        b.set(r, "has_intent", kind == "delete");
        for (const auto shape : {"paged", "key_read", "get_many", "put",
                 "put_many", "delete", "delete_many", "versions",
                 "version_read", "list_by"})
            b.set(r, std::string("is_") + shape, kind == shape);
    }
    b.set_integer(o, "route_count",
        static_cast<std::int64_t>(plan.commands.size()));
    write_strings(b, o, "uncovered_verbs", plan.uncovered_verbs);
    b.set(o, "has_page", any_page);
}

/*
 * json.dumps of a text: quoted, escaped, non-ASCII as \u escapes.
 */
std::string json_quote(std::string_view s) {
    std::string out = "\"";
    const auto escape = [&](unsigned code) {
        char buffer[8];
        std::snprintf(buffer, sizeof buffer, "\\u%04x", code);
        out += buffer;
    };
    for (std::size_t i = 0; i < s.size(); ++i) {
        const auto c = static_cast<unsigned char>(s[i]);
        if (c == '"' || c == '\\') {
            out += '\\';
            out += static_cast<char>(c);
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else if (c == '\t') {
            out += "\\t";
        } else if (c == '\b') {
            out += "\\b";
        } else if (c == '\f') {
            out += "\\f";
        } else if (c < 0x20) {
            escape(c);
        } else if (c < 0x80) {
            out += static_cast<char>(c);
        } else {
            const auto extra = c >= 0xf0 ? 3U : c >= 0xe0 ? 2U : 1U;
            unsigned code = c & (0x3fU >> extra);
            for (unsigned k = 0; k < extra && i + 1 < s.size(); ++k)
                code = (code << 6) | (static_cast<unsigned char>(s[++i]) & 0x3fU);
            if (code >= 0x10000) {
                code -= 0x10000;
                escape(0xd800 + (code >> 10));
                escape(0xdc00 + (code & 0x3ff));
            } else {
                escape(code);
            }
        }
    }
    return out + "\"";
}

/*
 * Whether json.loads reads the whole of @p text: one JSON value between
 * optional whitespace, NaN and the infinities included.
 */
class json_check final {
public:
    explicit json_check(std::string_view text) : s_(text) {}

    bool whole() {
        space();
        if (!value())
            return false;
        space();
        return i_ == s_.size();
    }

private:
    char peek() const { return i_ < s_.size() ? s_[i_] : '\0'; }

    void space() {
        while (peek() == ' ' || peek() == '\t' || peek() == '\n' ||
            peek() == '\r')
            ++i_;
    }

    bool word(std::string_view w) {
        if (s_.substr(i_, w.size()) != w)
            return false;
        i_ += w.size();
        return true;
    }

    bool digits() {
        const auto start = i_;
        while (peek() >= '0' && peek() <= '9')
            ++i_;
        return i_ > start;
    }

    bool number() {
        if (peek() == '-') {
            ++i_;
            if (word("Infinity"))
                return true;
        }
        if (peek() == '0')
            ++i_;
        else if (!digits())
            return false;
        if (peek() == '.') {
            ++i_;
            if (!digits())
                return false;
        }
        if (peek() == 'e' || peek() == 'E') {
            ++i_;
            if (peek() == '+' || peek() == '-')
                ++i_;
            if (!digits())
                return false;
        }
        return true;
    }

    bool string() {
        ++i_;
        while (i_ < s_.size() && s_[i_] != '"') {
            const auto c = static_cast<unsigned char>(s_[i_++]);
            if (c < 0x20)
                return false;
            if (c != '\\')
                continue;
            const auto e = peek();
            ++i_;
            if (e == 'u') {
                for (int k = 0; k < 4; ++k, ++i_) {
                    if (!std::isxdigit(static_cast<unsigned char>(peek())))
                        return false;
                }
            } else if (std::string_view("\"\\/bfnrt").find(e) ==
                std::string_view::npos) {
                return false;
            }
        }
        return i_++ < s_.size();
    }

    bool sequence(char close, bool keyed) {
        ++i_;
        space();
        if (peek() == close) {
            ++i_;
            return true;
        }
        while (true) {
            space();
            if (keyed) {
                if (peek() != '"' || !string())
                    return false;
                space();
                if (peek() != ':')
                    return false;
                ++i_;
                space();
            }
            if (!value())
                return false;
            space();
            if (peek() == close) {
                ++i_;
                return true;
            }
            if (peek() != ',')
                return false;
            ++i_;
        }
    }

    bool value() {
        switch (peek()) {
        case '{': return sequence('}', true);
        case '[': return sequence(']', false);
        case '"': return string();
        case 't': return word("true");
        case 'f': return word("false");
        case 'n': return word("null");
        case 'N': return word("NaN");
        case 'I': return word("Infinity");
        default: return number();
        }
    }

    std::string_view s_;
    std::size_t i_ = 0;
};

/*
 * _json_default: the JSON literal a write member the browser may leave
 * out takes, from the column's default_value.
 */
std::optional<std::string> json_default(const model_tables& t,
    value_id column) {
    const auto raw = values::member(t, column, "default_value");
    const auto cpp_type = values::str(t, column, "cpp_type");
    if (raw.is_none() || values::is(t, raw, compass::model::types::value_kind::null))
        return std::nullopt;
    const auto stated = values::py_str(t, raw);
    if (stated.find("::") != std::string::npos ||
        cpp_type.starts_with("std::optional"))
        return std::nullopt;
    auto text = std::string(values::strip(stated));
    if (text.size() >= 2 && text.front() == text.back() &&
        (text.front() == '\'' || text.front() == '"'))
        text = text.substr(1, text.size() - 2);
    if (cpp_type == "std::string")
        return json_quote(text);
    if (!json_check(text).whole())
        return std::nullopt;
    return text;
}

const shell_command* find_command(const shell_plan& plan,
    std::string_view kind, std::string_view precondition = {}) {
    for (const auto& c : plan.commands) {
        if (c.kind == kind &&
            (precondition.empty() || c.precondition == precondition))
            return &c;
    }
    return nullptr;
}

std::string segment(const std::string& subject, std::size_t n) {
    const auto parts = text::split(subject, '.');
    return n < parts.size() ? std::string(parts[n]) : std::string();
}

void write_quoted(builder& b, value_id parent, std::string_view key,
    const std::vector<std::string>& names) {
    const auto list = b.list(parent, key);
    for (std::size_t i = 0; i < names.size(); ++i) {
        const auto o = b.append_object(list);
        b.set(o, "name", names[i]);
        b.set(o, "last", i + 1 == names.size());
    }
}

std::string stripped(const model_tables& t, value_id entity,
    std::string_view key, std::string_view otherwise = {}) {
    const auto v = values::strip(values::truthy(t, entity, key) ?
        values::str(t, entity, key) : otherwise);
    return std::string(v);
}

/*
 * entity_web_records_plan: the record routes a screen reads the entity
 * through, when it states #+web_resource:.
 */
void write_web_records(builder& b, value_id entity, const shell_plan& plan) {
    const auto& t = b.tables();
    const auto key = stripped(t, entity, "web_resource");
    if (key.empty()) {
        b.set_null(entity, "web_records");
        return;
    }
    const auto component = values::py_str(t,
        values::member(t, entity, "component"), "");
    const auto singular = values::py_str(t,
        values::member(t, entity, "entity_singular"), "");
    const auto plural = values::py_str(t,
        values::member(t, entity, "entity_plural"), singular + "s");
    const auto* paged = find_command(plan, "paged");
    const auto* put = find_command(plan, "put");
    const auto* remove = find_command(plan, "delete");
    if (paged == nullptr || put == nullptr || remove == nullptr)
        throw std::runtime_error(singular +
            ": #+web_resource: needs a paged list, a put and a delete");
    const auto writes = stripped(t, entity, "web_writes", "one");
    const auto* put_many = find_command(plan, "put_many");
    const auto* delete_many = find_command(plan, "delete_many");

    std::vector<const shell_command*> list_bys;
    for (const auto& c : plan.commands) {
        if (c.kind == "list_by")
            list_bys.push_back(&c);
    }
    const auto wanted = stripped(t, entity, "web_list_by");
    const shell_command* list_by = nullptr;
    if (!wanted.empty()) {
        const auto it = std::ranges::find_if(list_bys,
            [&](const shell_command* c) { return c->relation->name == wanted; });
        if (it == list_bys.end())
            throw std::runtime_error(singular + ": #+web_list_by: names '" +
                wanted + "', which is not a relation the entity is listed by");
        list_by = *it;
    } else if (list_bys.size() == 1) {
        list_by = list_bys.front();
    } else if (list_bys.size() > 1) {
        throw std::runtime_error(singular +
            ": the entity is listed by more than one relation");
    }

    std::vector<std::string> key_fields;
    for (const auto c : values::items(t,
             values::member(t, entity, "primary_key"), "columns"))
        key_fields.push_back(wire::column_name(t, c));
    value_id single_key = value_id::none();
    std::vector<std::pair<std::string, std::string>> filters;
    for (const auto m : values::items(t, entity, "filter_members")) {
        if (key_fields.size() == 1 && single_key.is_none() &&
            values::truthy(t, m, "is_one_of") && values::truthy(t, m, "is_key"))
            single_key = m;
        if (values::truthy(t, m, "is_equals"))
            filters.emplace_back(values::py_str(t, values::member(t, m, "member")),
                values::py_str(t, values::member(t, m, "read_type")));
    }
    const auto parent_name = stripped(t, entity, "web_parent_filter");
    const auto parent_filter = std::ranges::find(filters, parent_name,
        &std::pair<std::string, std::string>::first);
    const auto has_parent_filter = !parent_name.empty() &&
        parent_filter != filters.end();
    const auto key_read = stripped(t, entity, "web_key_read", "generated");
    const auto because = stripped(t, entity, "web_read_only_because");
    const auto area_stated = stripped(t, entity, "web_area");
    const auto area = area_stated.empty() ? component : area_stated;
    std::vector<std::string> sortable;
    for (const auto f : values::items(t, entity, "sortable_fields"))
        sortable.push_back(values::py_str(t, values::member(t, f, "name")));
    const auto resource_name = segment(put->subject, 2);

    const auto o = b.object(entity, "web_records");
    b.set(o, "key", key);
    b.set(o, "unit", singular + "_web");
    b.set(o, "base", "/api/" + area + "/" + key);
    b.set(o, "area", area);
    b.set(o, "filter_name", plural + "_filter");
    b.set(o, "entity_type", "ores." + component + "." + singular);
    b.set(o, "rows_member", values::truthy(t, entity, "entity_plural_short") ?
        values::py_str(t, values::member(t, entity, "entity_plural_short")) :
        plural);
    b.set(o, "row_type", "ores::" + component + "::domain::" + singular);
    b.set(o, "versioned", !(values::truthy(t,
        values::member(t, entity, "sql"), "current_state") ||
        values::truthy(t, entity, "current_state")));
    b.set(o, "writable",
        !values::is_true(t, entity, "web_read_only") && because.empty());
    b.set(o, "read_only_because", because);
    b.set(o, "read_only_because_json", json_quote(because));
    b.set(o, "system_owned", values::is_true(t, entity, "web_system_owned"));
    b.set(o, "writes_many", writes == "many");
    b.set(o, "put_many_request", put_many ? put_many->request : "");
    b.set(o, "delete_many_request", delete_many ? delete_many->request : "");
    if (has_parent_filter) {
        const auto p = b.object(o, "parent_filter");
        b.set(p, "member", parent_filter->first);
        b.set(p, "read_type", parent_filter->second);
    } else {
        b.set_null(o, "parent_filter");
    }
    b.set(o, "has_by", list_by != nullptr || has_parent_filter);
    b.set(o, "has_key_route", !single_key.is_none() && key_read == "generated");
    b.set(o, "key_field", key_fields.size() == 1 ? key_fields[0] : "");
    b.set(o, "key_read_type", single_key.is_none() ? std::string() :
        values::py_str(t, values::member(t, single_key, "read_type")));
    const auto removal = wire::key_record_fields(t, entity);
    b.set(o, "removal_key_field", removal.empty() ? "" : removal[0].name);
    b.set(o, "removal_key_type", wire::strip_optional(removal.empty() ?
        std::string_view("std::string") : removal[0].cpp_type));
    const auto defaults = b.list(o, "write_defaults");
    bool any_default = false;
    for (const auto c : wire::write_record_columns(t, entity)) {
        const auto literal = json_default(t, c);
        if (!literal)
            continue;
        any_default = true;
        const auto d = b.append_object(defaults);
        b.set(d, "name", wire::column_name(t, c));
        b.set(d, "json", *literal);
    }
    b.set(o, "has_write_defaults", any_default);
    b.set(o, "search", values::truthy(t, entity, "has_search"));
    write_quoted(b, o, "key_fields", key_fields);
    write_quoted(b, o, "sortable", sortable);
    const auto filter_list = b.list(o, "filters");
    for (const auto& [member, read_type] : filters) {
        const auto f = b.append_object(filter_list);
        b.set(f, "member", member);
        b.set(f, "read_type", read_type);
    }
    b.set(o, "has_filters", !filters.empty());
    if (single_key.is_none())
        b.set_null(o, "single_key");
    else
        b.set_copy(o, "single_key", single_key);
    b.set(o, "list_request", paged->request);
    b.set(o, "put_request", put->request);
    b.set(o, "delete_request", remove->request);
    if (list_by == nullptr) {
        b.set_null(o, "list_by");
    } else {
        const auto l = b.object(o, "list_by");
        b.set(l, "member", list_by->relation->name);
        b.set(l, "request", list_by->request);
        b.set(l, "read_type", wire::strip_optional(list_by->relation->cpp_type));
    }
    b.set(o, "events_component", segment(put->subject, 0));
    b.set(o, "resource_name", resource_name);
    b.set(o, "read_permission", component + "::" + resource_name + ":read");
    b.set(o, "write_permission", component + "::" + resource_name + ":write");
    b.set(o, "delete_permission", component + "::" + resource_name + ":delete");
}

/*
 * entity_web_classification_plan: the routes the classification screen
 * maintains the entity through, when it states #+web_classification:.
 */
void write_web_classification(builder& b, value_id entity,
    const shell_plan& plan) {
    const auto& t = b.tables();
    const auto key = stripped(t, entity, "web_classification");
    if (key.empty()) {
        b.set_null(entity, "web_classification");
        return;
    }
    const auto singular = values::py_str(t,
        values::member(t, entity, "entity_singular"), "");
    const auto topic = stripped(t, entity, "web_classification_topic");
    const auto needed = [&](std::string_view kind) {
        const auto* c = find_command(plan, kind);
        if (c == nullptr)
            throw std::runtime_error(singular +
                ": #+web_classification: needs a " + std::string(kind) +
                " verb");
        return c;
    };
    const auto* paged = needed("paged");
    const auto* put = needed("put");
    const auto* put_many = needed("put_many");
    const auto* remove = needed("delete");
    bool has_name = false;
    bool has_order = false;
    for (const auto& f : wire::write_record_for(t, entity)) {
        has_name = has_name || f.name == "name";
        has_order = has_order || f.name == "display_order";
    }
    const auto component = values::py_str(t,
        values::member(t, entity, "component"), "");
    const auto plural = values::py_str(t,
        values::member(t, entity, "entity_plural"), singular + "s");
    const auto resource_name = segment(put->subject, 2);

    const auto o = b.object(entity, "web_classification");
    b.set(o, "key", key);
    b.set(o, "unit", singular + "_classification");
    b.set(o, "base", "/api/classifications/" + key);
    b.set(o, "topic", topic);
    b.set(o, "entity_type", "ores." + component + "." + singular);
    b.set(o, "shape", has_name ? "named" : has_order ? "ordered" : "plain");
    b.set(o, "has_name", has_name);
    b.set(o, "has_order", has_order);
    b.set(o, "editable",
        !values::is_true(t, entity, "web_classification_read_only"));
    b.set(o, "code_domain", singular);
    b.set(o, "rows_member", values::truthy(t, entity, "entity_plural_short") ?
        values::py_str(t, values::member(t, entity, "entity_plural_short")) :
        plural);
    b.set(o, "list_request", paged->request);
    b.set(o, "put_request", put->request);
    b.set(o, "put_many_request", put_many->request);
    b.set(o, "delete_request", remove->request);
    b.set(o, "write_permission", component + "::" + resource_name + ":write");
    b.set(o, "delete_permission", component + "::" + resource_name + ":delete");
}

}

void derive_surface_plans(builder& b, value_id entity) {
    const auto plan = plan_shell(b.tables(), entity);
    write_shell(b, entity, plan);
    write_http_route(b, entity, plan);
    write_web_records(b, entity, plan);
    write_web_classification(b, entity, plan);
}

}
