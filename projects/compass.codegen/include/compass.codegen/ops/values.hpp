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
#ifndef COMPASS_CODEGEN_OPS_VALUES_HPP
#define COMPASS_CODEGEN_OPS_VALUES_HPP

#include <cstdint>
#include <string_view>
#include <vector>
#include "compass.model/types/model_tables.hpp"

/**
 * @brief Reading the value tree with Python's semantics, for the
 * enrichment steps that port ores.codegen's dict code. A missing member is
 * value_id::none(), which every function here takes as Python takes a
 * missing key read with .get(): falsy, and no string.
 */
namespace compass::codegen::ops::values {

using compass::model::types::model_tables;
using compass::model::types::value_id;
using compass::model::types::value_kind;

[[nodiscard]] inline bool is(const model_tables& t, value_id v,
    value_kind k) noexcept {
    return !v.is_none() && t.values.kinds[v] == k;
}

/**
 * @brief The member @p key of @p object, or none when @p object is not an
 * object or has no such member.
 */
[[nodiscard]] inline value_id member(const model_tables& t, value_id object,
    std::string_view key) noexcept {
    if (!is(t, object, value_kind::object))
        return value_id::none();
    for (auto c = t.values.firsts[object]; !c.is_none();
         c = t.values.nexts[c]) {
        if (compass::model::types::text_of(t.texts, t.values.keys[c]) == key)
            return c;
    }
    return value_id::none();
}

[[nodiscard]] inline std::string_view key_of(const model_tables& t,
    value_id v) noexcept {
    return compass::model::types::text_of(t.texts, t.values.keys[v]);
}

/**
 * @brief The text of a string value; empty for any other value.
 */
[[nodiscard]] inline std::string_view str(const model_tables& t,
    value_id v) noexcept {
    if (!is(t, v, value_kind::string))
        return {};
    return compass::model::types::text_of(t.texts,
        compass::model::types::text_id{
            static_cast<std::uint32_t>(t.values.scalars[v])});
}

[[nodiscard]] inline std::string_view str(const model_tables& t,
    value_id object, std::string_view key) noexcept {
    return str(t, member(t, object, key));
}

/**
 * @brief Python's bool() of the value; a missing value is false.
 */
[[nodiscard]] inline bool truthy(const model_tables& t, value_id v) noexcept {
    if (v.is_none())
        return false;
    switch (t.values.kinds[v]) {
    case value_kind::null: return false;
    case value_kind::boolean:
    case value_kind::integer: return t.values.scalars[v] != 0;
    case value_kind::string: return !str(t, v).empty();
    case value_kind::list:
    case value_kind::object: return !t.values.firsts[v].is_none();
    }
    return false;
}

[[nodiscard]] inline bool truthy(const model_tables& t, value_id object,
    std::string_view key) noexcept {
    return truthy(t, member(t, object, key));
}

/**
 * @brief Whether the member is the boolean true, as `x is True` tests it.
 */
[[nodiscard]] inline bool is_true(const model_tables& t, value_id object,
    std::string_view key) noexcept {
    const auto v = member(t, object, key);
    return is(t, v, value_kind::boolean) && t.values.scalars[v] != 0;
}

/**
 * @brief The members of an object or the items of a list, in order; none
 * for any other value.
 */
[[nodiscard]] inline std::vector<value_id> items(const model_tables& t,
    value_id v) {
    std::vector<value_id> out;
    if (!is(t, v, value_kind::list) && !is(t, v, value_kind::object))
        return out;
    for (auto c = t.values.firsts[v]; !c.is_none(); c = t.values.nexts[c])
        out.push_back(c);
    return out;
}

[[nodiscard]] inline std::vector<value_id> items(const model_tables& t,
    value_id object, std::string_view key) {
    return items(t, member(t, object, key));
}

}

#endif
