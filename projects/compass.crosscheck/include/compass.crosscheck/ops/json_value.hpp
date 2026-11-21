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
#ifndef COMPASS_CROSSCHECK_OPS_JSON_VALUE_HPP
#define COMPASS_CROSSCHECK_OPS_JSON_VALUE_HPP

#include <optional>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief A value as Python's json.load gives it: None, a bool, an int, a
 * float, a str, a list or a dict that keeps its keys in order. compass.model's
 * reader has no real numbers, and a generator that renders str(value) of a
 * number must know an int from a float, so this keeps the kind and the text
 * as written.
 */
namespace compass::crosscheck::ops {

struct json_value {
    enum class kind { null, boolean, integer, real, string, list, object };
    kind what = kind::null;
    /**
     * @brief A string's text, a number as written, or true or false.
     */
    std::string text;
    /**
     * @brief A list's items, or an object's values in key order.
     */
    std::vector<json_value> items;
    /**
     * @brief An object's keys, one per item.
     */
    std::vector<std::string> keys;

    bool operator==(const json_value&) const = default;

    [[nodiscard]] static json_value string(std::string text);
    [[nodiscard]] static json_value boolean(bool value);
    [[nodiscard]] static json_value integer(long long value);

    /**
     * @brief d.get(@p key): the member, or null when this is not an object
     * or has no such member. A key stated twice is its last value, as
     * json.load keeps it.
     */
    [[nodiscard]] const json_value* get(std::string_view key) const noexcept;

    /**
     * @brief The text of a string; empty for anything else.
     */
    [[nodiscard]] std::string_view str() const noexcept;

    /**
     * @brief The text of member @p key when it is a string; empty
     * otherwise.
     */
    [[nodiscard]] std::string_view str(std::string_view key) const noexcept;

    /**
     * @brief bool(value).
     */
    [[nodiscard]] bool truthy() const noexcept;

    [[nodiscard]] bool is(kind k) const noexcept { return what == k; }

    /**
     * @brief Whether the value is an int or a float, as isinstance(v,
     * (int, float)) tests it; a bool is not counted.
     */
    [[nodiscard]] bool is_number() const noexcept {
        return what == kind::integer || what == kind::real;
    }
};

/**
 * @brief json.loads(@p text); nothing where it raises.
 */
[[nodiscard]] std::optional<json_value> read_json_value(std::string_view text);

/**
 * @brief str(@p v) of a scalar as Python spells it: None, True, False, an
 * int, repr of a float, or the string itself.
 */
[[nodiscard]] std::string python_str(const json_value& v);

}

#endif
