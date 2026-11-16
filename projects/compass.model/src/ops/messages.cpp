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
#include <string>
#include "loaders.hpp"
#include "text.hpp"

namespace compass::model::ops {

namespace {

using types::value_id;

constexpr std::pair<std::string_view, std::string_view> ts_scalars[] = {
    {"std::string", "string"},
    {"bool", "boolean"},
    {"int", "number"},
    {"float", "number"},
    {"double", "number"},
    {"std::int8_t", "number"},
    {"std::int16_t", "number"},
    {"std::int32_t", "number"},
    {"std::int64_t", "number"},
    {"std::uint8_t", "number"},
    {"std::uint16_t", "number"},
    {"std::uint32_t", "number"},
    {"std::uint64_t", "number"},
    {"std::size_t", "number"},
    {"boost::uuids::uuid", "string"},
    {"utility::uuid::tenant_id", "string"},
    {"std::chrono::year_month_day", "string"},
    {"boost::asio::ip::address", "string"},
    {"std::chrono::system_clock::time_point", "string"},
};


bool is_identifier(std::string_view s) {
    if (s.empty() || !((s[0] >= 'a' && s[0] <= 'z') ||
        (s[0] >= 'A' && s[0] <= 'Z') || s[0] == '_'))
        return false;
    return std::ranges::all_of(s, [](char ch) {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '_';
    });
}

/**
 * @brief ^ores::(?!utility::)<ident>::domain::(<ident>)$, the group or empty.
 */
std::string_view domain_entity_name(std::string_view t) {
    constexpr std::string_view prefix = "ores::";
    if (!t.starts_with(prefix))
        return {};
    t.remove_prefix(prefix.size());
    if (t.starts_with("utility::"))
        return {};
    const auto sep = t.find("::");
    if (sep == std::string_view::npos || !is_identifier(t.substr(0, sep)))
        return {};
    t.remove_prefix(sep + 2);
    constexpr std::string_view domain = "domain::";
    if (!t.starts_with(domain))
        return {};
    t.remove_prefix(domain.size());
    return is_identifier(t) ? t : std::string_view{};
}

/**
 * @brief _TS_DOMAIN_STRING_TYPES: the domain member types the protocol
 * projection refuses that cross the wire as a string.
 */
bool is_domain_string_type(std::string_view t) {
    return t == "boost::uuids::uuid" || t == "std::chrono::year_month_day" ||
        t == "boost::asio::ip::address";
}

std::vector<std::string_view> split_template_arguments(std::string_view a) {
    std::vector<std::string_view> parts;
    int depth = 0;
    std::size_t start = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i] == '<')
            ++depth;
        else if (a[i] == '>')
            --depth;
        else if (a[i] == ',' && depth == 0) {
            parts.push_back(trim(a.substr(start, i - start)));
            start = i + 1;
        }
    }
    parts.push_back(trim(a.substr(start)));
    return parts;
}

std::string ts_of(std::string_view t, const custom_types& custom) {
    const auto wrapped = [&](std::string_view open) {
        return t.starts_with(open) && t.ends_with(">");
    };
    if (wrapped("std::optional<")) {
        const auto inner = ts_of(t.substr(14, t.size() - 15), custom);
        return inner.empty() ? inner : inner + " | null";
    }
    if (wrapped("std::vector<")) {
        const auto inner = ts_of(t.substr(12, t.size() - 13), custom);
        return inner.empty() ? inner : inner + "[]";
    }
    if (wrapped("std::map<")) {
        const auto args = split_template_arguments(t.substr(9, t.size() - 10));
        if (args.size() != 2)
            return {};
        const auto value = ts_of(args[1], custom);
        return value.empty() ? value : "Record<string, " + value + ">";
    }
    for (const auto& [cpp, ts] : ts_scalars) {
        if (cpp == t)
            return std::string(ts);
    }
    for (const auto& [name, header] : custom) {
        if (name == t)
            return "string";
    }
    if (const auto name = domain_entity_name(t); !name.empty())
        return pascal_case(name);
    if (t.starts_with("domain::"))
        return "string";
    for (const auto& u : ts_utility_types()) {
        if (u.cpp == t)
            return std::string(u.pascal);
    }
    if (t.find("::") == std::string_view::npos)
        return pascal_case(t);
    return {};
}

bool is_yes(std::string_view lowered) {
    return lowered == "true" || lowered == "yes" || lowered == "1";
}

/**
 * @brief [a-z][a-z0-9]*(-[a-z0-9]+)*, matched in full.
 */
bool is_command_verb(std::string_view v) {
    if (v.empty() || !(v[0] >= 'a' && v[0] <= 'z') || v.back() == '-')
        return false;
    char previous = 0;
    for (const char ch : v) {
        const bool word = (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9');
        if (!word && ch != '-')
            return false;
        if (ch == '-' && previous == '-')
            return false;
        previous = ch;
    }
    return true;
}

std::string indent_block(std::string_view text, std::size_t spaces) {
    std::string out;
    const auto lines = lines_of(text);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i != 0)
            out += '\n';
        if (!trim(lines[i]).empty())
            out.append(spaces, ' ');
        out += lines[i];
    }
    return out;
}

}

std::span<const ts_utility_type> ts_utility_types() noexcept {
    static constexpr ts_utility_type types[] = {
        {"ores::utility::domain::hierarchy_node", "HierarchyNode",
            "utility/hierarchy"},
        {"ores::utility::domain::result", "Result", "utility/protocol"},
        {"ores::utility::domain::precondition", "Precondition",
            "utility/protocol"},
        {"ores::utility::domain::change_intent", "ChangeIntent",
            "utility/protocol"},
        {"ores::utility::domain::order", "Order", "utility/protocol"},
        {"ores::utility::domain::scope", "Scope", "utility/protocol"},
        {"ores::trading::domain::instrument_batch", "InstrumentBatch",
            "generated/trading/instrument_batch"},
        {"ores::trading::domain::trade_envelope_data", "TradeEnvelopeData",
            "trading/payload"},
        {"ores::security::jwt::storage_grant", "StorageGrant",
            "security/payload"},
        {"ores::ore::messaging::ore_document", "OreDocument",
            "generated/ore/protocol/ore_document_protocol"},
        {"ores::trading::messaging::trade_summaries_filter",
            "TradeSummariesFilter",
            "generated/trading/protocol/trade_summary_protocol"},
    };
    return types;
}

const custom_types& custom_type_headers(const document_finder& find,
    load_cache& cache) {
    static const custom_types none;
    constexpr std::string_view path = "projects/modeling/cpp_custom_types.org";
    const auto d = find(path);
    if (!d)
        return none;
    auto& cached = cache.get().registries[std::string(path)];
    if (cached.source == d->source.data() && cached.size == d->source.size())
        return cached.value;
    outline o;
    build_outline(d->source, *d->tables, o);
    const auto registry = section(o, node_id{0}, "The registry");
    std::map<std::string, std::string> rows;
    const auto bare = [](std::string_view s) {
        s = trim(s);
        while (!s.empty() && s.front() == '=')
            s.remove_prefix(1);
        while (!s.empty() && s.back() == '=')
            s.remove_suffix(1);
        return std::string(s);
    };
    for (const auto& row : first_table_rows(o, registry)) {
        const auto name = bare(row.value_or("Type"));
        const auto header = bare(row.value_or("Header"));
        if (name.empty() || header.empty())
            continue;
        const auto it = rows.find(name);
        if (it != rows.end() && it->second != header)
            throw model_error("cpp_custom_types.org binds '" + name +
                "' to two headers");
        rows[name] = header;
    }
    cached = {d->source.data(), d->source.size(),
        custom_types(rows.begin(), rows.end())};
    return cached.value;
}

std::string ts_type(std::string_view cpp_type, const custom_types& custom) {
    return ts_of(cpp_type, custom);
}

std::string ts_domain_type(std::string_view cpp_type,
    const custom_types& custom) {
    cpp_type = trim(cpp_type);
    if (cpp_type.starts_with("std::optional<") && cpp_type.ends_with(">")) {
        const auto inner = ts_domain_type(
            cpp_type.substr(14, cpp_type.size() - 15), custom);
        return inner.empty() ? inner : inner + " | null";
    }
    if (is_domain_string_type(cpp_type))
        return "string";
    return ts_of(cpp_type, custom);
}

std::vector<ts_import> field_group_ts_imports(std::string_view component,
    std::string_view own, const std::vector<std::string>& cpp_types,
    const custom_types& custom) {
    std::map<std::string, std::string> found;
    const auto walk = [&](this const auto& self, std::string_view t) -> void {
        t = trim(t);
        if (t.empty())
            return;
        for (const std::string_view open : {"std::optional<", "std::vector<"}) {
            if (t.starts_with(open) && t.ends_with(">")) {
                self(t.substr(open.size(), t.size() - open.size() - 1));
                return;
            }
        }
        if (t.starts_with("std::map<") && t.ends_with(">")) {
            const auto args = split_template_arguments(t.substr(9, t.size() - 10));
            if (args.size() == 2)
                self(args[1]);
            return;
        }
        if (is_domain_string_type(t) || std::ranges::any_of(ts_scalars,
                [&](const auto& s) { return s.first == t; }))
            return;
        if (t.starts_with("domain::") || std::ranges::any_of(custom,
                [&](const auto& row) { return row.first == t; }))
            return;
        for (const auto& u : ts_utility_types()) {
            if (u.cpp == t) {
                found[std::string(u.pascal)] =
                    "../../../" + std::string(u.module) + ".js";
                return;
            }
        }
        std::string_view owner = component;
        auto name = domain_entity_name(t);
        if (!name.empty()) {
            const auto rest = t.substr(6);
            owner = rest.substr(0, rest.find("::"));
        } else if (t.find("::") != std::string_view::npos) {
            return;
        } else {
            name = t;
        }
        if (name == own)
            return;
        found[pascal_case(name)] = owner == component
            ? "./" + std::string(name) + ".js"
            : "../../" + std::string(owner) + "/domain/" + std::string(name) +
                ".js";
    };
    for (const auto& t : cpp_types)
        walk(t);
    std::vector<ts_import> out;
    for (auto& [pascal, module] : found)
        out.push_back({pascal, std::move(module)});
    return out;
}

void declared_messages(load_context& c, value_id list) {
    auto& b = c.b;
    const auto& o = c.o;
    const auto section_node = section(o, node_id{0}, "Messages");
    for (const auto n : children(o, section_node)) {
        const auto title = o.titles[n.value];
        const auto entry = b.append_object(list);
        b.set(entry, "name", title);
        b.set(entry, "name_pascal", pascal_case(title));
        std::map<std::string, std::string_view> lowered;
        for (const auto& p : properties(o, n))
            lowered[to_lower(p.key)] = p.value;
        const auto get = [&](const std::string& key) -> lookup {
            const auto it = lowered.find(key);
            return it == lowered.end() ? lookup{} : lookup{true, it->second};
        };
        const auto subject = get("subject");
        if (subject.found)
            b.set(entry, "subject", subject.value);
        if (const auto r = get("response"); r.found)
            b.set(entry, "response_type", r.value);
        const auto auth = to_lower(trim(get("auth").value));
        if (!auth.empty() && auth != "none")
            throw model_error("message states :auth: other than none");
        std::string_view requires_session;
        if (subject.found) {
            requires_session = auth == "none" ? "false" : "true";
            b.set(entry, "requires_session", requires_session);
        }
        const auto destructive = to_lower(trim(get("destructive").value));
        if (!destructive.empty()) {
            if (!is_yes(destructive))
                throw model_error("message states :destructive: not yes");
            b.set(entry, "destructive", true);
        }
        const auto http_route = to_lower(trim(get("http_route").value));
        if (!http_route.empty()) {
            if (!is_yes(http_route))
                throw model_error("message states :http_route: not yes");
            b.set(entry, "http_route", true);
        }
        const auto rate_policy = trim(get("rate_policy").value);
        if (!rate_policy.empty()) {
            if (requires_session != "false")
                throw model_error("message states :rate_policy: with a "
                    "session");
            b.set(entry, "rate_policy", rate_policy);
        }
        const auto verb = trim(get("shell_command").value);
        if (!verb.empty()) {
            if (!is_command_verb(verb))
                throw model_error("message states a bad :shell_command:");
            b.set(entry, "shell_command", verb);
        }
        const auto timeout = trim(get("request_timeout_seconds").value);
        if (!timeout.empty()) {
            if (requires_session != "true")
                throw model_error("message states :request_timeout_seconds: "
                    "without a session");
            const auto seconds = parse_int(timeout).value_or(0);
            if (seconds <= 0)
                throw model_error("message states a bad "
                    ":request_timeout_seconds:");
            b.set_integer(entry, "request_timeout_seconds", seconds);
        }
        const auto adopts = trim(get("shell_adopts_token_for").value);
        if (!adopts.empty())
            b.set(entry, "shell_adopts_token_for", adopts);
        const auto comment = named_block(o, n, "comment");
        if (!comment.value.empty())
            b.set(entry, "comment", comment.value);

        const auto fields = b.list(entry, "fields");
        bool adopted = adopts.empty();
        for (const auto f : children(o, n)) {
            const auto field = b.append_object(fields);
            const auto name = o.titles[f.value];
            b.set(field, "name", name);
            adopted = adopted || name == adopts;
            std::map<std::string, std::string_view> fp;
            for (const auto& p : properties(o, f))
                fp[to_lower(p.key)] = p.value;
            if (const auto it = fp.find("cpp_type"); it != fp.end()) {
                b.set(field, "cpp_type", it->second);
                const auto mapped = ts_type(it->second,
                    custom_type_headers(c.find, c.cache));
                if (!mapped.empty())
                    b.set(field, "ts_type", mapped);
            }
            if (const auto it = fp.find("default"); it != fp.end())
                b.set(field, "default", it->second);
            const auto field_comment = named_block(o, f, "comment");
            b.set(field, "comment", field_comment.value.empty()
                ? std::string() : indent_block(field_comment.value, 4));
        }
        if (!adopted)
            throw model_error("message states :shell_adopts_token_for: "
                "that is not one of its fields");
    }
}

}
