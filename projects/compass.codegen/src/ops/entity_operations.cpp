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
#include <string>
#include <utility>
#include <vector>
#include "compass.codegen/ops/values.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::model_tables;
using compass::model::types::value_id;

namespace {

/*
 * _OPERATION_PREFIXES: the verb a derived request's name states.
 */
std::string operation_verb(std::string_view name) {
    constexpr std::pair<std::string_view, std::string_view> prefixes[] = {
        {"list_by_", "list_scoped"}, {"put_many_", "put_many"},
        {"delete_many_", "delete_many"}, {"get_many_", "get_many"},
        {"list_", "list"}, {"get_", "get"}, {"put_", "put"},
        {"delete_", "delete"}};
    for (const auto& [prefix, verb] : prefixes) {
        if (name.starts_with(prefix))
            return std::string(verb);
    }
    return {};
}

bool is_write_verb(std::string_view verb) {
    return verb == "put" || verb == "put_many" || verb == "delete" ||
        verb == "delete_many";
}

/*
 * open_reads_of: the subjects :open_reads: names, split on whitespace.
 */
std::vector<std::string> open_reads(const model_tables& t, value_id entity) {
    const auto stated = values::truthy(t, entity, "open_reads") ?
        values::py_str(t, values::member(t, entity, "open_reads")) :
        std::string();
    std::vector<std::string> out;
    std::string word;
    for (const char c : stated + ' ') {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
            c == '\v') {
            if (!word.empty())
                out.push_back(std::move(word));
            word.clear();
        } else {
            word += c;
        }
    }
    return out;
}

}

void derive_operations(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto open = open_reads(t, entity);
    const auto list = b.list(entity, "operations");
    std::vector<std::string> verbs;
    for (const auto m : values::items(t, entity, "messages")) {
        if (!values::truthy(t, m, "derived") || !values::truthy(t, m, "subject"))
            continue;
        const auto name = values::py_str(t, values::member(t, m, "name"));
        const auto subject = values::py_str(t, values::member(t, m, "subject"));
        const auto verb = values::truthy(t, m, "verb") ?
            values::py_str(t, values::member(t, m, "verb")) :
            operation_verb(name);
        const auto fields = values::items(t, m, "fields");
        std::vector<std::string_view> names;
        for (const auto f : fields)
            names.push_back(values::str(t, f, "name"));
        const auto leading_type = fields.empty() ? std::string_view() :
            values::str(t, fields.front(), "cpp_type");
        const auto has = [&](std::string_view n) {
            return std::ranges::find(names, n) != names.end();
        };
        const auto is_write = is_write_verb(verb);
        const auto guarded_read = !is_write && !verb.empty() &&
            std::ranges::find(open, subject) == open.end();

        const auto o = b.append_object(list);
        b.set(o, "method", name.size() < 8 ? std::string() :
            name.substr(0, name.size() - 8));
        b.set(o, "request", name);
        const auto response = values::member(t, m, "response_type");
        if (response.is_none())
            b.set(o, "response", "");
        else
            b.set_copy(o, "response", response);
        b.set(o, "subject", subject);
        b.set(o, "verb", verb);
        b.set_copy(o, "fields", values::member(t, m, "fields"));
        b.set(o, "has_order", has("order"));
        b.set(o, "has_filter", has("filter"));
        b.set(o, "has_scope", has("scope"));
        b.set(o, "leading", names.empty() ? std::string_view() : names.front());
        b.set(o, "leading_is_uuid",
            leading_type.find("boost::uuids::uuid") != std::string_view::npos);
        b.set(o, "leading_is_timestamp",
            leading_type.find("time_point") != std::string_view::npos);
        b.set(o, "leading_is_optional",
            leading_type.find("optional") != std::string_view::npos);
        b.set(o, "is_write", is_write);
        b.set(o, "is_guarded", is_write || guarded_read);
        b.set(o, "is_put_one", verb == "put");
        b.set(o, "permission",
            verb == "delete" || verb == "delete_many" ? "delete" :
            verb == "put" || verb == "put_many" ? "write" :
            guarded_read ? "read" : "");
        b.set(o, "requires_session",
            values::py_str(t, values::member(t, m, "requires_session")) !=
                "false");
        if (std::ranges::find(verbs, verb) == verbs.end())
            verbs.push_back(verb);
    }
    const auto operations = values::items(t, list);
    for (const auto& verb : verbs) {
        const auto grouped = b.list(entity, verb + "_operations");
        for (const auto o : operations) {
            if (values::str(t, o, "verb") == verb)
                b.append_copy(grouped, o);
        }
    }
}

}
