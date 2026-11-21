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
#ifndef COMPASS_ENV_OPS_TEMPLATES_HPP
#define COMPASS_ENV_OPS_TEMPLATES_HPP

#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace compass::env::ops {

/**
 * @brief The value of a name a template holds, or nothing when no input,
 * variable or field has that name.
 */
using resolver =
    std::function<std::optional<std::string>(std::string_view name)>;

/**
 * @brief @p text with each {name} replaced by its value, and each
 * {name:-default} by its value or, when that is empty, the default. {{ and
 * }} stand for { and }. A name is letters, digits, '_' and '.'.
 *
 * @return why it cannot be expanded: a brace that opens no name, or a name
 * @p resolve does not know.
 */
[[nodiscard]] std::expected<std::string, std::string> expand(
    std::string_view text, const resolver& resolve);

/**
 * @brief Whether the condition @p when holds.
 *
 * A condition is a template, which holds when its expansion is not empty;
 * or two templates joined by " = " or " != ", the operator with a blank on
 * each side, which holds when their expansions are equal or differ. The
 * first such operator outside braces splits the condition, and the blanks
 * around each side are dropped. Either side may name values: {A} = {B}.
 *
 * @return why it cannot be expanded, as expand() says.
 */
[[nodiscard]] std::expected<bool, std::string> holds(std::string_view when,
    const resolver& resolve);

/**
 * @brief The names @p text holds, in order, or why it does not read.
 */
[[nodiscard]] std::expected<std::vector<std::string>, std::string> names_in(
    std::string_view text);

}

#endif
