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
#ifndef COMPASS_CPP_OPS_EXPORTED_TYPES_HPP
#define COMPASS_CPP_OPS_EXPORTED_TYPES_HPP

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

/**
 * @brief The rules of ORE Studio's check_exported_types_are_compiled.py
 * over the text of one file.
 */
namespace compass::cpp::ops {

/**
 * @brief The export macro an export.hpp defines: the first
 * "#define ORES_..._EXPORT BOOST_SYMBOL..." at the start of a line, when
 * the header also has an "#ifdef ORES_..._LIBRARY" guard; nothing
 * otherwise.
 */
[[nodiscard]] std::optional<std::string_view> export_macro(
    std::string_view header);

/**
 * @brief Every name of an #include "name" in @p text, in order, wherever
 * it is, comments included.
 */
[[nodiscard]] std::vector<std::string_view> quoted_includes(
    std::string_view text);

/**
 * @brief One class or struct a header exports with a macro.
 * offset is where its line starts.
 */
struct exported_type {
    std::string_view name;
    std::uint32_t offset = 0;
    std::string_view macro;
};

/**
 * @brief The types @p header declares as "class MACRO name" or
 * "struct MACRO name" at the start of a line, for @p macro; none for a
 * header that says AUTO-GENERATED FILE.
 */
[[nodiscard]] std::vector<exported_type> exported_types(
    std::string_view header, std::string_view macro);

/**
 * @brief The types @p header declares as exported_types finds them, for
 * every ORES_..._EXPORT macro at once, each with its macro.
 */
[[nodiscard]] std::vector<exported_type> exported_types(
    std::string_view header);

}

#endif
