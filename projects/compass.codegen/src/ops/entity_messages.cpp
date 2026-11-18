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
#include <iterator>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "compass.codegen/ops/values.hpp"
#include "compass.model/ops/typescript.hpp"
#include "entity_text.hpp"
#include "entity_wire.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::value_id;

namespace {

constexpr std::string_view result_type = "ores::utility::domain::result";
constexpr std::string_view precondition_type =
    "ores::utility::domain::precondition";
constexpr std::string_view intent_type =
    "ores::utility::domain::change_intent";
constexpr std::string_view order_type = "ores::utility::domain::order";
constexpr std::string_view scope_type = "ores::utility::domain::scope";

/*
 * One derived protocol message, as _ts_message shapes it.
 */
struct message {
    std::string name;
    std::vector<wire::field> fields;
    std::string response_type{};
    std::string subject{};
    std::string verb{};
};

wire::field f(std::string name, std::string cpp_type,
    std::string default_value = {}) {
    return {.name = std::move(name), .cpp_type = std::move(cpp_type),
        .default_value = std::move(default_value), .domain_member = {}};
}

std::string str(std::string_view s) { return std::string(s); }

std::string request_subject(std::string_view component,
    std::string_view plural, std::string_view verb) {
    return str(component) + ".v1." + str(plural) + "." + str(verb);
}

std::string versions_subject(std::string_view component,
    std::string_view plural, std::string_view verb) {
    return str(component) + ".v1." + str(plural) + "_versions." + str(verb);
}

/*
 * paged_list_messages: the request and response every paged list shares.
 */
std::vector<message> paged_list(const std::string& name, std::string subject,
    std::vector<wire::field> fields, const std::optional<wire::field>& filter,
    const std::string& collection, const std::string& collection_type,
    std::string verb = {}) {
    fields.push_back(f("offset", "std::uint32_t", "0"));
    fields.push_back(f("limit", "std::uint32_t", "100"));
    fields.push_back(f("order", str(order_type)));
    if (filter)
        fields.push_back(*filter);
    return {
        {.name = name + "_request", .fields = std::move(fields),
         .response_type = name + "_response", .subject = std::move(subject),
         .verb = std::move(verb)},
        {.name = name + "_response",
         .fields = {f("result", str(result_type)),
             f(collection, "std::vector<" + collection_type + ">"),
             f("total", "std::uint64_t")}}};
}

bool is_write_only(std::string_view name) {
    for (const auto prefix : {"put_", "put_many_", "delete_", "delete_many_"}) {
        if (name.starts_with(prefix))
            return true;
    }
    for (const auto suffix : {"_write", "_change", "_removal"}) {
        if (name.ends_with(suffix))
            return true;
    }
    return false;
}

std::vector<message> derived_messages(const builder& b, value_id entity,
    const std::set<std::string, std::less<>>& siblings) {
    const auto& t = b.tables();
    const auto component = str(values::str(t, entity, "component"));
    const auto singular = str(values::str(t, entity, "entity_singular"));
    const auto plural = values::member(t, entity, "entity_plural").is_none() ?
        singular + "s" : values::py_str(t,
            values::member(t, entity, "entity_plural"));
    const auto plural_short = values::truthy(t, entity, "entity_plural_short") ?
        values::py_str(t, values::member(t, entity, "entity_plural_short")) :
        plural;
    const auto domain_type = "ores::" + component + "::domain::" + singular;
    const auto key = singular + "_key";
    const auto stem = siblings.contains(singular + "_version") ? plural :
        singular;
    const auto payload = singular == "result" ? str("result_value") : singular;
    const auto optional_row = "std::optional<" + domain_type + ">";

    const auto filter = wire::filter_members(t, entity);
    std::vector<wire::field> filter_fields;
    for (const auto& m : filter.members)
        filter_fields.push_back(f(m.member, "std::optional<" + m.cpp_type + ">"));
    if (!filter.searchable.empty())
        filter_fields.push_back(f("search", "std::optional<std::string>"));

    std::vector<message> out{
        {.name = key, .fields = wire::key_record_fields(b.tables(), entity)},
        {.name = singular + "_write", .fields = wire::write_record_for(t, entity)},
        {.name = singular + "_change",
         .fields = {f("write", singular + "_write"),
             f("precondition", str(precondition_type))}},
        {.name = singular + "_removal",
         .fields = {f("key", key), f("precondition", str(precondition_type),
             "ores::utility::domain::removal_precondition")}},
        {.name = singular + "_lookup",
         .fields = {f("key", key), f(singular, optional_row)}}};
    if (!filter_fields.empty())
        out.push_back({.name = plural + "_filter", .fields = filter_fields});
    out.push_back({.name = singular + "_event",
        .fields = {f("event_id", "boost::uuids::uuid"), f("key", key),
            f("action", "std::string"), f("version", "std::uint32_t"),
            f("occurred_at", "std::chrono::system_clock::time_point"),
            f("correlation_id", "std::optional<std::string>")}});
    const auto audited = values::truthy(t, entity, "has_audit_columns");
    if (audited) {
        out.push_back({.name = stem + "_version_key",
            .fields = {f(singular, key), f("version", "std::uint32_t")}});
        out.push_back({.name = stem + "_versions_filter",
            .fields = {f("version", "std::optional<std::uint32_t>"),
                f("from_version", "std::optional<std::uint32_t>"),
                f("to_version", "std::optional<std::uint32_t>")}});
    }

    std::optional<wire::field> list_filter;
    if (!filter_fields.empty())
        list_filter = f("filter", "std::optional<" + plural + "_filter>");
    auto plain = paged_list("list_" + plural,
        request_subject(component, plural, "list"), {}, list_filter,
        plural_short, domain_type);
    if (values::truthy(t, entity, "has_as_of_lookup"))
        plain[0].fields.push_back(f("as_of", "std::optional<std::string>"));
    std::ranges::move(plain, std::back_inserter(out));

    const auto pair = [&](const std::string& name, std::string_view verb,
        std::vector<wire::field> request, std::vector<wire::field> response) {
        out.push_back({.name = name + "_request", .fields = std::move(request),
            .response_type = name + "_response",
            .subject = request_subject(component, plural, verb)});
        response.insert(response.begin(), f("result", str(result_type)));
        out.push_back({.name = name + "_response",
            .fields = std::move(response)});
    };
    pair("get_" + singular, "get", {f("key", key)}, {f(payload, optional_row)});
    pair("get_many_" + plural, "get_many", {f("keys", "std::vector<" + key + ">")},
        {f("entries", "std::vector<" + singular + "_lookup>")});
    pair("put_" + singular, "put",
        {f("change", singular + "_change"), f("intent", str(intent_type))},
        {f(payload, optional_row)});
    pair("put_many_" + plural, "put_many",
        {f("changes", "std::vector<" + singular + "_change>"),
         f("intent", str(intent_type))},
        {f(plural_short, "std::vector<" + domain_type + ">")});
    pair("delete_" + singular, "delete",
        {f("removal", singular + "_removal"), f("intent", str(intent_type))},
        {});
    pair("delete_many_" + plural, "delete_many",
        {f("removals", "std::vector<" + singular + "_removal>"),
         f("intent", str(intent_type))},
        {});

    for (const auto& relation : wire::relation_columns(t, entity)) {
        auto scoped = paged_list("list_by_" + relation + "_" + plural,
            request_subject(component, plural, "list_by_" + relation),
            {f(relation, wire::column_cpp_type(t, entity, relation)),
             f("scope", str(scope_type),
                 "ores::utility::domain::scope::direct")},
            list_filter, plural_short, domain_type);
        std::ranges::move(scoped, std::back_inserter(out));
    }

    if (audited) {
        auto versions = paged_list("list_" + stem + "_versions",
            versions_subject(component, plural, "list"), {f("key", key)},
            f("filter", "std::optional<" + stem + "_versions_filter>"),
            "versions", domain_type, "list_versions");
        std::ranges::move(versions, std::back_inserter(out));
        const auto name = "get_" + stem + "_version";
        out.push_back({.name = name + "_request",
            .fields = {f("key", stem + "_version_key")},
            .response_type = name + "_response",
            .subject = versions_subject(component, plural, "get"),
            .verb = "get_version"});
        out.push_back({.name = name + "_response",
            .fields = {f("result", str(result_type)),
                f("version", optional_row)}});
    }
    return out;
}

void write_message(builder& b, value_id list, const message& m,
    const compass::model::ops::custom_types& custom) {
    const auto o = b.append_object(list);
    b.set(o, "name", m.name);
    b.set(o, "name_pascal", compass::model::ops::pascal_case(m.name));
    b.set(o, "comment", "");
    const auto fields = b.list(o, "fields");
    for (const auto& field : m.fields)
        wire::append_field(b, fields, field, custom);
    b.set(o, "derived", true);
    if (!m.verb.empty())
        b.set(o, "verb", m.verb);
    if (!m.response_type.empty())
        b.set(o, "response_type", m.response_type);
    if (!m.subject.empty()) {
        b.set(o, "subject", m.subject);
        b.set(o, "requires_session", "true");
    }
}

}

void derive_messages(builder& b, value_id entity,
    const types::entity_registry& registry,
    const std::set<std::string, std::less<>>& siblings) {
    const auto& t = b.tables();
    const auto read_only = values::truthy(t, entity, "read_only") ||
        values::truthy(t, entity, "client_read_only");
    const auto derived = derived_messages(b, entity, siblings);
    const auto declared = values::items(t, entity, "declared_messages");
    const auto list = b.list(entity, "messages");
    for (const auto& m : derived) {
        if (!read_only || !is_write_only(m.name))
            write_message(b, list, m, registry.custom_types);
    }
    for (const auto d : declared) {
        if (values::truthy(t, d, "subject") &&
            values::member(t, d, "requires_session").is_none())
            b.set(d, "requires_session", "true");
        if (!read_only || !is_write_only(values::str(t, d, "name")))
            b.append_copy(list, d);
    }
}

bool write_filter_members(builder& b, value_id entity) {
    const auto filter = wire::filter_members(b.tables(), entity);
    const auto list = b.list(entity, "filter_members");
    bool has_range = false;
    using kind = wire::filter_member::kind;
    for (const auto& m : filter.members) {
        const auto o = b.append_object(list);
        b.set(o, "member", m.member);
        b.set(o, "column", m.column);
        b.set(o, "cpp_type", m.cpp_type);
        switch (m.k) {
        case kind::equals:
            b.set(o, "is_equals", true);
            b.set(o, "read_type", m.read_type);
            b.set(o, "is_nullable", m.is_nullable);
            break;
        case kind::one_of:
            b.set(o, "is_one_of", true);
            b.set(o, "is_key", m.is_key);
            b.set(o, "read_type", m.read_type);
            break;
        case kind::range:
            has_range = true;
            b.set(o, "is_range", true);
            b.set(o, m.is_from ? "is_from" : "is_to", true);
            b.set(o, "is_timestamp", m.is_timestamp);
            break;
        }
    }
    return has_range;
}

void derive_filter_record(builder& b, value_id entity,
    const types::entity_site& site) {
    const bool has_range = write_filter_members(b, entity);
    const auto& t = b.tables();
    const auto filter = wire::filter_members(t, entity);
    std::string search = "{";
    for (const auto& s : filter.searchable)
        search += (search.size() > 1 ? ", \"" : "\"") + s + "\"";
    b.set(entity, "search_columns", search + "}");
    b.set(entity, "has_search", !filter.searchable.empty());
    b.set(entity, "has_range_filter", has_range);
    b.set(entity, "has_filter_record",
        (!filter.members.empty() || !filter.searchable.empty()) &&
        site.protocol_enabled);
    b.set(entity, "filter_type", "messaging::" + values::py_str(t,
        values::member(t, entity, "entity_plural")) + "_filter");
    const std::string singular(values::str(t, entity, "entity_singular"));
    b.set(entity, "payload_member",
        singular == "result" ? std::string("result_value") : singular);
}

void derive_ts_alias(builder& b, value_id entity) {
    const auto& t = b.tables();
    std::vector<std::string_view> cpp_types;
    for (const auto m : values::items(t, entity, "messages")) {
        for (const auto field : values::items(t, m, "fields"))
            cpp_types.push_back(values::str(t, field, "cpp_type"));
    }
    const auto singular = std::string(values::str(t, entity, "entity_singular"));
    const auto pascal = compass::model::ops::pascal_case(singular);
    const auto taken = compass::model::ops::ts_utility_imports(cpp_types);
    if (std::ranges::none_of(taken,
            [&](const auto& i) { return i.pascal == pascal; }))
        return;
    const auto alias = pascal + "Entity";
    const auto own = "ores::" + std::string(values::str(t, entity, "component")) +
        "::domain::" + singular;
    for (const auto key : {"messages", "declared_messages"}) {
        for (const auto m : values::items(t, entity, key)) {
            for (const auto field : values::items(t, m, "fields")) {
                if (values::str(t, field, "cpp_type").find(own) ==
                        std::string_view::npos ||
                    !values::truthy(t, field, "ts_type"))
                    continue;
                b.set(field, "ts_type", text::replace_word(
                    values::str(t, field, "ts_type"), pascal, alias));
            }
        }
    }
    b.set(entity, "entity_ts_alias", alias);
}

void derive_write_fields(builder& b, value_id entity,
    const types::entity_registry& registry) {
    const auto& t = b.tables();
    const auto fields = wire::write_record_for(t, entity);
    const auto keys = wire::key_column_names(t, entity);
    struct preserved {
        std::string name;
        std::string member;
    };
    std::vector<preserved> kept;
    for (const auto c : wire::write_record_columns(t, entity)) {
        auto name = wire::column_name(t, c);
        if (!wire::is_service_preserved(name) ||
            std::ranges::find(keys, name) != keys.end())
            continue;
        kept.push_back({name, std::string(values::str(t, c, "group_prefix")) +
            name});
    }
    std::vector<std::pair<std::string, value_id>> by_member;
    for (const auto g : values::items(t, entity, "domain_group_fields"))
        by_member.emplace_back(values::str(t, g, "name"), g);
    const auto member_of = [&](const std::string& name) {
        for (const auto& [n, g] : by_member) {
            if (n == name)
                return g;
        }
        return value_id::none();
    };

    const auto list = b.list(entity, "write_fields");
    for (const auto& f : fields)
        wire::append_field(b, list, f, registry.custom_types);
    const auto written = values::items(t, list);
    for (std::size_t i = 0; i < fields.size(); ++i) {
        const auto& f = fields[i];
        const auto member = member_of(f.domain_member.value_or(""));
        const std::string wire_type(values::strip(f.cpp_type));
        const auto member_type = values::truthy(t, member, "cpp_type") ?
            std::string(values::strip(values::str(t, member, "cpp_type"))) :
            wire_type;
        if (f.render_is_enum && !member_type.empty() &&
            member_type != wire_type) {
            b.set(written[i], "enum_from_string", member_type + "_from_string");
            b.set(written[i], "enum_default",
                values::truthy(t, member, "default_value") ?
                values::py_str(t, values::member(t, member, "default_value")) :
                member_type + "{}");
        }
        if (wire_type != "std::string")
            continue;
        if (member_type == "std::chrono::year_month_day" ||
            member_type == "std::optional<std::chrono::year_month_day>")
            b.set(written[i], "date_from_string", true);
        else if (member_type == "std::chrono::system_clock::time_point" ||
            member_type ==
                "std::optional<std::chrono::system_clock::time_point>")
            b.set(written[i], "timestamp_from_string", true);
    }

    const auto preserved_list = b.list(entity, "preserved_fields");
    for (const auto& p : kept) {
        const auto o = b.append_object(preserved_list);
        b.set(o, "name", p.name);
        b.set(o, "member", p.member);
    }
    b.set(entity, "has_preserved_fields", !kept.empty());
}

void derive_message_struct_names(builder& b, value_id entity) {
    const auto& t = b.tables();
    for (const auto key : {"messages", "declared_messages"}) {
        for (const auto m : values::items(t, entity, key)) {
            const std::string name(values::str(t, m, "name"));
            b.set(m, "struct_name", name);
        }
    }
}

}
