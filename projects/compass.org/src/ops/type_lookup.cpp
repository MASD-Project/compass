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
#include "compass.org/ops/type_lookup.hpp"

#include <algorithm>
#include <cstdint>
#include <ranges>

namespace compass::org::ops {

namespace {

types::type_entry_id entries_end(const types::type_registry& r,
    types::type_id t) noexcept {
    return t.next().value < r.types.size()
        ? r.types.todo_firsts[t.next()]
        : types::type_entry_id{static_cast<std::uint32_t>(r.entries.size())};
}

types::column_view<types::type_entry_id, types::name_id> entries(
    const types::type_registry& r, types::type_entry_id first,
    types::type_entry_id end) noexcept {
    return r.entries.names.slice(first.value, end.value - first.value);
}

}

std::string_view name_text(const types::type_registry& r,
    types::name_id n) noexcept {
    const auto first = r.names.firsts[n];
    const auto end = n.next().value < r.names.size()
        ? r.names.firsts[n.next()]
        : static_cast<std::uint32_t>(r.name_bytes.size());
    return {r.name_bytes.data() + first, end - first};
}

types::type_id find_type(const types::type_registry& r,
    std::string_view name) noexcept {
    const auto rows = std::views::iota(std::uint32_t{0},
        static_cast<std::uint32_t>(r.types.size()));
    const auto it = std::ranges::lower_bound(rows, name, {},
        [&](std::uint32_t row) {
            return name_text(r, r.types.names[types::type_id{row}]);
        });
    if (it == rows.end() ||
        name_text(r, r.types.names[types::type_id{*it}]) != name)
        return types::type_id::none();
    return {*it};
}

types::column_view<types::type_entry_id, types::name_id> active_todos(
    const types::type_registry& r, types::type_id t) noexcept {
    return entries(r, r.types.todo_firsts[t], r.types.done_firsts[t]);
}

types::column_view<types::type_entry_id, types::name_id> done_todos(
    const types::type_registry& r, types::type_id t) noexcept {
    return entries(r, r.types.done_firsts[t], r.types.section_firsts[t]);
}

types::column_view<types::type_entry_id, types::name_id> required_sections(
    const types::type_registry& r, types::type_id t) noexcept {
    return entries(r, r.types.section_firsts[t], entries_end(r, t));
}

bool is_stateful(const types::type_registry& r, types::type_id t) noexcept {
    return r.types.todo_firsts[t] != r.types.section_firsts[t];
}

}
