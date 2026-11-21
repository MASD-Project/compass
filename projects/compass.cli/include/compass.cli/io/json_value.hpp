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
#ifndef COMPASS_CLI_IO_JSON_VALUE_HPP
#define COMPASS_CLI_IO_JSON_VALUE_HPP

#include <cstdint>
#include <optional>
#include <string_view>
#include "compass.model/types/model_tables.hpp"

/**
 * Reading the members of JSON another program wrote, once
 * compass::model::io::read_json has read it.
 */
namespace compass::cli::io {

/**
 * @brief The member @p key of the object @p object, or none when it has
 * no such member or is not an object.
 */
[[nodiscard]] inline model::types::value_id member(
    const model::types::model_tables& t, model::types::value_id object,
    std::string_view key) {
    namespace mt = model::types;
    if (object.is_none() || t.values.kinds[object] != mt::value_kind::object)
        return mt::value_id::none();
    for (auto v = t.values.firsts[object]; !v.is_none();
         v = t.values.nexts[v]) {
        if (mt::text_of(t.texts, t.values.keys[v]) == key)
            return v;
    }
    return mt::value_id::none();
}

/**
 * @brief The text of the string @p v, or nothing when it is not one.
 */
[[nodiscard]] inline std::optional<std::string_view> string_of(
    const model::types::model_tables& t, model::types::value_id v) {
    namespace mt = model::types;
    if (v.is_none() || t.values.kinds[v] != mt::value_kind::string)
        return std::nullopt;
    return mt::text_of(t.texts,
        mt::text_id{static_cast<std::uint32_t>(t.values.scalars[v])});
}

/**
 * @brief The integer @p v, or nothing when it is not one.
 */
[[nodiscard]] inline std::optional<std::int64_t> integer_of(
    const model::types::model_tables& t, model::types::value_id v) {
    if (v.is_none() ||
        t.values.kinds[v] != model::types::value_kind::integer)
        return std::nullopt;
    return t.values.scalars[v];
}

}

#endif
