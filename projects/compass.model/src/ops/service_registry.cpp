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
#include <string>
#include "loaders.hpp"
#include "text.hpp"

namespace compass::model::ops {

namespace {

using types::value_id;

/**
 * @brief Every bullet of every list of the child of @p n titled @p title.
 */
std::vector<std::string_view> bullets(const outline& o, node_id n,
    std::string_view title) {
    std::vector<std::string_view> out;
    const auto sub = section(o, n, title);
    if (sub.is_none())
        return out;
    for (std::size_t l = 0; l < o.list_nodes.size(); ++l) {
        if (o.list_nodes[l] != sub)
            continue;
        const auto end = l + 1 < o.list_item_firsts.size()
            ? o.list_item_firsts[l + 1]
            : static_cast<std::uint32_t>(o.list_items.size());
        for (auto i = o.list_item_firsts[l]; i < end; ++i)
            out.push_back(trim(o.list_items[i]));
    }
    return out;
}

void prefixes(builder& b, value_id service, std::string_view key,
    const outline& o, node_id n, std::string_view title) {
    const auto list = b.list(service, key);
    for (const auto line : bullets(o, n, title))
        b.set(b.append_object(list), "prefix", line);
}

}

void load_service_registry(load_context& c, value_id root) {
    auto& b = c.b;
    const auto& o = c.o;
    const auto registry = b.object(root, "service_registry");
    const auto services = b.list(registry, "services");
    for (const auto n : children(o, node_id{0})) {
        const auto replicas = property(o, n, "replicas");
        if (!replicas.found)
            continue;
        const auto service = b.append_object(services);
        b.set(service, "name", o.titles[n.value]);
        for (const auto key : {"psql_var", "env_key", "iam_role",
            "description", "display_name", "summary", "account_var", "role",
            "email", "runtime", "entry_point"}) {
            if (const auto r = property(o, n, key); r.found)
                b.set(service, key, r.value);
        }
        prefixes(b, service, "dml_prefixes", o, n, "DML prefixes");
        const auto tables = b.list(service, "select_tables");
        for (const auto line : bullets(o, n, "Select tables")) {
            const auto item = b.detached_object();
            std::string_view rest = line;
            for (;;) {
                const auto comma = rest.find(',');
                const auto part = rest.substr(0, comma);
                if (const auto eq = part.find('='); eq != std::string_view::npos)
                    b.set(item, trim(part.substr(0, eq)),
                        trim(part.substr(eq + 1)));
                if (comma == std::string_view::npos)
                    break;
                rest.remove_prefix(comma + 1);
            }
            if (truthy(b, item))
                b.append_copy(tables, item);
        }
        prefixes(b, service, "select_prefixes", o, n, "Select prefixes");
        prefixes(b, service, "execute_prefixes", o, n, "Execute prefixes");
        const auto count = parse_int(replicas.value);
        if (!count)
            throw model_error(":replicas: is not an integer");
        b.set_integer(service, "replicas", *count);
        const auto enabled = property(o, n, "enabled");
        b.set(service, "enabled",
            to_lower(trim(enabled.found ? enabled.value : "true")) == "true");
        const auto depends = b.list(service, "depends_on");
        std::string_view rest = property(o, n, "depends_on").value;
        for (;;) {
            const auto comma = rest.find(',');
            const auto d = trim(rest.substr(0, comma));
            if (!d.empty())
                b.append(depends, d);
            if (comma == std::string_view::npos)
                break;
            rest.remove_prefix(comma + 1);
        }
        const auto extra = b.list(service, "extra_args");
        for (const auto line : bullets(o, n, "Extra args"))
            b.append(extra, line);
    }
}

}
