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
#ifndef COMPASS_CROSSCHECK_OPS_PYTHON_LITERALS_HPP
#define COMPASS_CROSSCHECK_OPS_PYTHON_LITERALS_HPP

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/**
 * @brief The data a gate script keeps in a Python module, read as the
 * literal it is written as. Some gates read their tables from a Python
 * module beside them; the codec reads the same module, so the table has one
 * home.
 */
namespace compass::crosscheck::ops {

struct python_entry;

/**
 * @brief A literal: a string (adjacent literals joined), a name, a tuple,
 * list or set, a dict, or a call with keyword arguments.
 */
struct python_value {
    enum class kind { string, name, sequence, dict, call };
    kind what = kind::string;
    std::string text;
    std::vector<python_value> items;
    std::vector<python_entry> entries;
};

/**
 * @brief A key of a dict or a keyword of a call, and its value. A dict key
 * that is a tuple has its items in key_items and an empty key.
 */
struct python_entry {
    std::string key;
    python_value value;
    std::vector<python_value> key_items;
};

/**
 * @brief The value of the module-level assignment to @p name in @p source,
 * a line that starts with the name, an optional annotation and =; nothing
 * when there is none or the value is not a literal of these kinds.
 */
[[nodiscard]] std::optional<python_value> python_assignment(
    std::string_view source, std::string_view name);

/**
 * @brief The strings of a sequence of strings; nothing when @p v is not
 * one.
 */
[[nodiscard]] std::optional<std::vector<std::string>> strings_of(
    const python_value& v);

}

#endif
