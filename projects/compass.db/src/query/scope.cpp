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
#include "compass.db/query/scope.hpp"

#include <format>
#include <optional>
#include <string_view>
#include "compass.db/query/search.hpp"

namespace compass::db::query {

environment_row find_environment(io::connection& c, const std::string& name) {
    const auto r = c.exec(R"sql(
SELECT e.id,
       coalesce(to_char(e.synced_at AT TIME ZONE 'UTC',
                        'YYYY-MM-DD"T"HH24:MI:SS"Z"'), ''),
       (SELECT value FROM compass.setting WHERE key = 'text_search')
FROM corpus.environment e WHERE e.name = $1
)sql", {name});
    if (r.rows() == 0)
        throw unknown_environment(name);
    if (r.is_null(0, 2))
        throw io::error("the database has no text search configuration; "
            "fix: run compass db init", "");
    return {std::string(r.value(0, 0)), std::string(r.value(0, 1)),
        std::string(r.value(0, 2))};
}

void where_documents(statement& s, const std::string& environment_id,
    const types::view_filter& v, const types::document_filter& f) {
    const auto env = s.bind(environment_id) + "::bigint";
    s.sql += "WHERE p.environment = " + env + "\n";
    s.sql += "  AND (" + v.condition + ")\n";
    const auto equal = [&](std::string_view column,
                           const std::optional<std::string>& value) {
        if (value)
            s.sql += std::format("  AND d.{} = {}\n", column, s.bind(*value));
    };
    equal("type", f.type);
    equal("state", f.state);
    equal("level", f.level);
    equal("audience", f.audience);
    for (const auto& tag : f.tags)
        s.sql += "  AND d.tags @> ARRAY[" + s.bind(tag) + "::text]\n";
    if (f.sprint)
        s.sql += "  AND d.hash IN (SELECT corpus.sprint_members(" + env +
            ", " + s.bind(*f.sprint) + "))\n";
    for (const auto& k : f.keywords) {
        const auto name = s.bind(k.name);
        s.sql += std::format("  AND d.keywords @> jsonb_build_object("
            "lower({}::text), jsonb_build_array({}::text))\n", name,
            s.bind(k.value));
    }
    for (const auto& p : f.properties) {
        const auto name = s.bind(p.name);
        s.sql += std::format("  AND d.properties @> jsonb_build_object("
            "{}::text, {}::text)\n", name, s.bind(p.value));
    }
}

}
