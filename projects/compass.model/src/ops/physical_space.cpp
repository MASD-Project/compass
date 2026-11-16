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
#include "loaders.hpp"
#include "text.hpp"

namespace compass::model::ops {

namespace {

using overrides = std::vector<std::pair<std::string, bool>>;

/**
 * @brief _parse_physical_space_table: the rows of the document's
 * * Physical space table whose Enabled is true or false.
 */
void append_table(const outline& o, overrides& out) {
    const auto table = section(o, node_id{0}, "Physical space");
    if (table.is_none())
        return;
    for (const auto& row : first_table_rows(o, table)) {
        auto address = trim(row.value_or("Address"));
        while (!address.empty() && address.front() == '=')
            address.remove_prefix(1);
        while (!address.empty() && address.back() == '=')
            address.remove_suffix(1);
        const auto enabled = to_lower(trim(row.value_or("Enabled")));
        if (!address.empty() && (enabled == "true" || enabled == "false"))
            out.emplace_back(std::string(address) + ".enabled",
                enabled == "true");
    }
}

/**
 * @brief _load_profile_address_overrides: the table of the profile
 * @p slug, parsed once for the bytes the finder hands out.
 */
const overrides& profile_table(std::string_view slug,
    const document_finder& find, load_cache& cache) {
    std::string file(slug);
    std::ranges::replace(file, '-', '_');
    const auto path = "projects/modeling/variability_" + file + ".org";
    const auto d = find(path);
    if (!d)
        throw model_error("unknown profile '" + std::string(slug) + "'");
    auto& cached = cache.get().physical_spaces[path];
    if (cached.source == d->source.data() && cached.size == d->source.size())
        return cached.value;
    cached = {d->source.data(), d->source.size(), {}};
    outline o;
    build_outline(d->source, *d->tables, o);
    append_table(o, cached.value);
    return cached.value;
}

}

std::vector<std::pair<std::string, bool>>
physical_space_overrides(const outline& o, const document_finder& find,
    load_cache& cache) {
    overrides out;
    const auto profiles = property(o, section(o, node_id{0}, "Flags"),
        "profile");
    std::string_view rest = profiles.value;
    while (profiles.found) {
        const auto comma = rest.find(',');
        const auto slug = trim(rest.substr(0, comma));
        if (!slug.empty()) {
            const auto& table = profile_table(slug, find, cache);
            out.insert(out.end(), table.begin(), table.end());
        }
        if (comma == std::string_view::npos)
            break;
        rest.remove_prefix(comma + 1);
    }
    append_table(o, out);
    return out;
}

}
