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
using types::value_kind;

constexpr std::string_view derived_subject_hole = "{component}";

constexpr std::string_view shell_token_types[] = {
    "std::string", "bool", "int", "std::int32_t", "std::int64_t",
    "std::uint16_t", "std::uint32_t", "std::uint64_t", "double",
    "boost::uuids::uuid", "std::chrono::system_clock::time_point",
    "boost::asio::ip::address"};

constexpr std::pair<std::string_view, std::string_view> shell_literals[] = {
    {"std::string", "sample"}, {"bool", "true"}, {"int", "1"},
    {"std::int32_t", "1"}, {"std::int64_t", "1"}, {"std::uint16_t", "1"},
    {"std::uint32_t", "1"}, {"std::uint64_t", "1"},
    {"boost::uuids::uuid", "00000000-0000-0000-0000-000000000001"},
    {"double", "1.0"},
    {"std::chrono::system_clock::time_point", "2026-01-01T00:00:00Z"},
    {"boost::asio::ip::address", "127.0.0.1"}};

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

template <std::size_t N>
std::string_view lookup_in(
    const std::pair<std::string_view, std::string_view> (&table)[N],
    std::string_view key, std::string_view fallback) {
    for (const auto& [k, v] : table) {
        if (k == key)
            return v;
    }
    return fallback;
}

std::vector<value_id> items(const builder& b, value_id list) {
    std::vector<value_id> out;
    if (list.is_none())
        return out;
    const auto& v = b.tables().values;
    for (auto i = v.firsts[list]; !i.is_none(); i = v.nexts[i])
        out.push_back(i);
    return out;
}


void derived_subject(load_context& c, value_id op,
    const std::vector<value_id>& messages, std::string_view singular) {
    auto& b = c.b;
    std::set<std::string> patterns;
    for (const auto m : messages) {
        const auto subject = string_of(b, b.find(m, "subject"));
        if (subject.find(derived_subject_hole) != std::string_view::npos)
            patterns.emplace(subject);
    }
    if (patterns.empty())
        return;
    if (patterns.size() > 1)
        throw model_error("more than one derived subject");
    const auto& pattern = *patterns.begin();
    const auto hole = pattern.find(derived_subject_hole);
    b.set(op, "derived_subject", true);
    b.set(op, "derived_subject_prefix", pattern.substr(0, hole));
    b.set(op, "derived_subject_suffix",
        pattern.substr(hole + derived_subject_hole.size()));
    const std::string s(singular);
    b.set(op, "subject_function", s + "_subject_for");
    b.set(op, "subject_function_by_component", s + "_subject_by_component");
    std::string ts;
    std::size_t start = 0;
    bool first = true;
    for (;;) {
        const auto end = s.find('_', start);
        const auto word = s.substr(start, end == std::string::npos
            ? std::string::npos : end - start);
        if (first)
            ts += word;
        else if (!word.empty())
            ts += pascal_case(word);
        first = false;
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    b.set(op, "subject_function_ts", ts + "SubjectFor");
}

void domain_imports(load_context& c, value_id op,
    const std::vector<value_id>& messages, value_id component) {
    auto& b = c.b;
    std::vector<std::string_view> cpp_types;
    for (const auto m : messages) {
        for (const auto f : items(b, b.find(m, "fields")))
            cpp_types.push_back(string_of(b, b.find(f, "cpp_type")));
    }
    const auto list = b.list(op, "domain_imports");
    const auto component_text = component.is_none() ? std::string() :
        text_value(b, component);
    const auto owner_of = component.is_none() ? std::nullopt :
        std::optional<std::string_view>(component_text);
    for (const auto& i : ts_domain_imports(cpp_types, owner_of,
             custom_type_headers(c.find, c.cache))) {
        const auto item = b.append_object(list);
        b.set(item, "entity", i.entity);
        b.set(item, "entity_pascal", i.entity_pascal);
        b.set(item, "module_dir", i.module_dir);
    }
}

void utility_imports(load_context& c, value_id op,
    const std::vector<value_id>& messages) {
    auto& b = c.b;
    std::vector<std::string_view> cpp_types;
    for (const auto m : messages) {
        for (const auto f : items(b, b.find(m, "fields")))
            cpp_types.push_back(string_of(b, b.find(f, "cpp_type")));
    }
    const auto found = ts_utility_imports(cpp_types);
    const auto list = b.list(op, "utility_imports");
    for (const auto& i : found) {
        const auto item = b.append_object(list);
        b.set(item, "name_pascal", i.pascal);
        b.set(item, "module", i.module);
    }
}

std::string command_name(std::string_view message) {
    std::vector<std::string_view> words;
    std::size_t start = 0;
    for (;;) {
        const auto end = message.find('_', start);
        words.push_back(message.substr(start, end == std::string_view::npos
            ? std::string_view::npos : end - start));
        if (end == std::string_view::npos)
            break;
        start = end + 1;
    }
    while (words.size() > 1 && (words.back() == "request" ||
        words.back() == "command" || words.back() == "typed"))
        words.pop_back();
    std::string out;
    for (std::size_t i = 0; i < words.size(); ++i) {
        if (i != 0)
            out += '-';
        out += words[i];
    }
    return out;
}

std::string_view optional_inner(std::string_view cpp) {
    if (cpp.starts_with("std::optional<") && cpp.ends_with(">"))
        return trim(cpp.substr(14, cpp.size() - 15));
    return {};
}

bool fillable(std::string_view cpp, bool is_list) {
    if (is_list)
        return true;
    const auto t = cpp.starts_with("std::optional<") && cpp.ends_with(">")
        ? optional_inner(cpp) : cpp;
    return std::ranges::find(shell_token_types, t) !=
        std::end(shell_token_types);
}

std::string_view token_literal(std::string_view cpp, bool is_list) {
    if (is_list)
        return "sample";
    if (cpp.starts_with("std::optional<") && cpp.ends_with(">"))
        return lookup_in(shell_literals, optional_inner(cpp), "sample");
    return lookup_in(shell_literals, cpp, "sample");
}

std::string_view sentinel(std::string_view name, std::string_view cpp) {
    if (name == "reason_code")
        return "system.new_record";
    if (name == "commentary")
        return "generated_script";
    const auto kind = trim(cpp);
    if (kind.starts_with("std::optional<"))
        return "-";
    return lookup_in(sentinels, kind, "__none__");
}

struct shell_field_facts {
    bool optional = false;
    bool is_list = false;
    bool is_bool = false;
    bool is_fillable = true;
    std::string name;
    std::string cpp;
};

shell_field_facts shell_field(builder& b, value_id out, value_id field) {
    shell_field_facts f;
    const auto declared = b.find(field, "cpp_type");
    const auto raw = truthy(b, declared) ? text_value(b, declared)
        : std::string("std::string");
    f.cpp = std::string(trim(raw));
    f.name = text_value(b, b.find(field, "name"));
    f.is_list = f.cpp == "std::vector<std::string>";
    const auto def = b.find(field, "default");
    f.optional = !def.is_none() &&
        b.tables().values.kinds[def] != value_kind::null;
    f.is_bool = f.cpp == "bool";
    f.is_fillable = fillable(f.cpp, f.is_list);
    b.set_copy(out, "name", b.find(field, "name"));
    b.set(out, "cpp_type", f.cpp);
    if (def.is_none())
        b.set_null(out, "default");
    else
        b.set_copy(out, "default", def);
    b.set(out, "is_optional", f.optional);
    b.set(out, "is_list", f.is_list);
    b.set(out, "is_string", f.cpp == "std::string");
    b.set(out, "is_bool", f.is_bool);
    b.set(out, "is_number", f.cpp == "int" || f.cpp == "std::uint32_t" ||
        f.cpp == "std::uint64_t");
    b.set(out, "needs_from_token", !f.is_list && f.cpp != "std::string" &&
        f.cpp != "bool");
    b.set(out, "fillable", f.is_fillable);
    b.set(out, "token_literal", token_literal(f.cpp, f.is_list));
    return f;
}

void copy_or_null(builder& b, value_id out, std::string_view key,
    value_id from, std::string_view from_key) {
    const auto v = b.find(from, from_key);
    if (v.is_none())
        b.set_null(out, key);
    else
        b.set_copy(out, key, v);
}

/**
 * @brief shell_command_projection, with the flags load_org_operation_model
 * derives from it. Returns the unsupported fields as command.field.
 */
std::set<std::string> shell_commands(builder& b, value_id op,
    const std::vector<value_id>& messages) {
    const auto list = b.list(op, "shell_commands");
    bool any_bool = false, any_list = false, any_timeout = false;
    std::int64_t count = 0;
    std::set<std::string> unsupported_all;
    for (const auto m : messages) {
        const auto subject = b.find(m, "subject");
        const auto response = b.find(m, "response_type");
        if (!truthy(b, subject) || !truthy(b, response))
            continue;
        if (truthy(b, b.find(m, "subject_is_derived")))
            continue;
        const auto cmd = b.append_object(list);
        ++count;
        const auto name = text_value(b, b.find(m, "name"));
        const auto derived = command_name(name);
        const auto declared = b.find(m, "shell_command");
        const auto command = truthy(b, declared) ? text_value(b, declared)
            : derived;
        const auto positionals = b.list(cmd, "positionals");
        const auto flags = b.list(cmd, "flags");
        std::string usage = command;
        std::string invocation = command;
        std::vector<std::string> unsupported;
        std::vector<shell_field_facts> positional_facts, flag_facts;
        for (const auto f : items(b, b.find(m, "fields"))) {
            const auto scratch = b.detached_object();
            const auto fact = shell_field(b, scratch, f);
            if (!fact.is_fillable)
                unsupported.push_back(fact.name);
            b.append_copy(fact.optional ? flags : positionals, scratch);
            (fact.optional ? flag_facts : positional_facts).push_back(fact);
            any_bool = any_bool || fact.is_bool;
            any_list = any_list || fact.is_list;
        }
        for (const auto& f : positional_facts) {
            usage += " <" + f.name + ">";
            invocation += " " + std::string(sentinel(f.name, f.cpp));
        }
        for (const auto& f : flag_facts)
            usage += " [--" + f.name + " <v>]";
        auto identifier = command;
        std::ranges::replace(identifier, '-', '_');
        auto operation_identifier = derived;
        std::ranges::replace(operation_identifier, '-', '_');
        b.set(cmd, "command", command);
        b.set(cmd, "identifier", identifier);
        b.set(cmd, "operation", derived);
        b.set(cmd, "operation_identifier", operation_identifier);
        b.set_copy(cmd, "request", b.find(m, "name"));
        b.set_copy(cmd, "response_type", response);
        b.set_copy(cmd, "subject", subject);
        b.set(cmd, "public",
            string_of(b, b.find(m, "requires_session")) == "false");
        b.set(cmd, "http_route", truthy(b, b.find(m, "http_route")));
        if (const auto r = b.find(m, "rate_policy"); r.is_none())
            b.set(cmd, "rate_policy", "");
        else
            b.set_copy(cmd, "rate_policy", r);
        b.set(cmd, "is_destructive", truthy(b, b.find(m, "destructive")));
        copy_or_null(b, cmd, "request_timeout_seconds", m,
            "request_timeout_seconds");
        any_timeout = any_timeout ||
            truthy(b, b.find(m, "request_timeout_seconds"));
        copy_or_null(b, cmd, "adopts_token_field", m, "shell_adopts_token_for");
        b.set_integer(cmd, "positional_count",
            static_cast<std::int64_t>(positional_facts.size()));
        b.set(cmd, "has_positionals", !positional_facts.empty());
        b.set(cmd, "has_flags", !flag_facts.empty());
        b.set(cmd, "usage", usage);
        b.set(cmd, "invocation", invocation);
        const auto unsupported_list = b.list(cmd, "unsupported");
        for (const auto& u : unsupported) {
            b.append(unsupported_list, u);
            unsupported_all.insert(command + "." + u);
        }
    }
    b.set(op, "shell_has_bool", any_bool);
    b.set(op, "shell_has_list", any_list);
    b.set(op, "shell_has_helpers", any_bool || any_list);
    b.set(op, "shell_has_request_timeout", any_timeout);
    b.set_integer(op, "shell_command_count", count);
    return unsupported_all;
}

}

void load_operation(load_context& c, value_id root) {
    auto& b = c.b;
    const auto& o = c.o;
    const node_id document{0};
    ensure_profile_binding(o);
    reject_junction_only_flags(o, "operation");
    const auto op = b.object(root, "operation");
    for (const auto key : {"product", "component", "subcomponent",
        "entity_singular", "namespace", "brief"}) {
        if (const auto r = frontmatter(o, key); r.found)
            b.set(op, key, r.value);
    }
    if (const auto text = body(o, document); !text.empty())
        b.set(op, "description", text);
    if (const auto inc = section(o, document, "Includes"); !inc.is_none())
        includes_from_named_block(c, inc, b.list(op, "includes"));

    const auto list = b.list(op, "messages");
    declared_messages(c, list);
    const auto messages = items(b, list);
    for (const auto m : messages) {
        if (string_of(b, b.find(m, "subject")).find(derived_subject_hole) !=
            std::string_view::npos)
            b.set(m, "subject_is_derived", true);
    }
    derived_subject(c, op, messages, frontmatter_or(o, "entity_singular"));
    domain_imports(c, op, messages, b.find(op, "component"));
    utility_imports(c, op, messages);
    const auto unsupported = shell_commands(b, op, messages);

    bool unmapped = false;
    for (const auto m : messages) {
        for (const auto f : items(b, b.find(m, "fields"))) {
            unmapped = unmapped || (!b.find(f, "cpp_type").is_none() &&
                b.find(f, "ts_type").is_none());
        }
    }
    // The key's first part is the project's product, which the model
    // does not state; any one-part root matches, the last key winning.
    const auto lowered = [&](std::string_view address) {
        std::string_view value;
        for (std::size_t i = 0; i < o.file_property_keys.size(); ++i) {
            const std::string_view key = o.file_property_keys[i];
            const auto dot = key.find('.');
            if (dot != std::string_view::npos && dot > 0 &&
                key.substr(dot + 1) == address)
                value = o.file_property_values[i];
        }
        return to_lower(trim(value));
    };
    if (unmapped) {
        const auto ts = lowered("ts.protocol.enabled");
        if (ts != "nil" && ts != "false" && ts != "no" && ts != "0")
            throw model_error("no TypeScript projection for a field type");
    }
    if (!unsupported.empty()) {
        const auto shell = lowered("cpp.shell-command.enabled");
        if (shell == "true" || shell == "yes" || shell == "1")
            throw model_error("no shell token form for a field");
    }
}

}
