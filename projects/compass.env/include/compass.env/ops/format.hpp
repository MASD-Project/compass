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
#ifndef COMPASS_ENV_OPS_FORMAT_HPP
#define COMPASS_ENV_OPS_FORMAT_HPP

#include <array>
#include <cstdint>
#include <expected>
#include <string_view>
#include <vector>
#include "compass.codec/ops/data_tables.hpp"
#include "compass.env/types/env_format.hpp"

namespace compass::env::ops {

/**
 * @brief The codec whose check document gives the format.
 */
inline constexpr std::string_view codec_name = "env_format";

/**
 * @brief The rules of the format, in the order of their numbers.
 */
inline constexpr std::uint32_t unknown_version = 0;
inline constexpr std::uint32_t undeclared = 1;
inline constexpr std::uint32_t unknown_variable = 2;
inline constexpr std::uint32_t empty_version = 3;
inline constexpr std::uint32_t unreadable = 4;

/**
 * @brief The inputs compass gives every generation, which a caller may
 * still set: checkout, the checkout's absolute path; home, the home
 * directory; now, the time in UTC as YYYY-MM-DD HH:MM:SS UTC.
 */
inline constexpr std::array<std::string_view, 3> builtin_inputs{"checkout",
    "home", "now"};

/**
 * @brief The format the tables of a check document state.
 *
 * Settings (setting, value), Versions (version, date, adds, removes, format
 * only, description) and Variables (variable, kind, argument, added,
 * removed, section, comment, when, services, purpose) are required;
 * Comments (comment, text) and Inputs (input, purpose) are read when
 * present.
 *
 * @return the format, or the first thing that does not read, as an
 * unreadable finding at its line.
 */
[[nodiscard]] std::expected<types::env_format, types::finding> read_format(
    const codec::ops::data_tables& tables);

/**
 * @brief What is wrong with the versions of @p f: a variable that names no
 * version (unknown_version); a variable a version after the baseline adds
 * or removes without saying so (undeclared); a version that names a
 * variable no row adds or removes in it (unknown_variable); and a version
 * after the baseline that adds and removes nothing and is not format only
 * (empty_version).
 */
[[nodiscard]] std::vector<types::finding> check_format(
    const types::env_format& f);

/**
 * @brief The words of a list cell: names split at commas and blanks.
 */
[[nodiscard]] std::vector<std::string> list_of(std::string_view cell);

}

#endif
