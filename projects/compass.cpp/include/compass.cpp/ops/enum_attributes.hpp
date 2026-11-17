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
#ifndef COMPASS_CPP_OPS_ENUM_ATTRIBUTES_HPP
#define COMPASS_CPP_OPS_ENUM_ATTRIBUTES_HPP

#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief The rules of ORE Studio's check_enum_export_attributes.py over
 * the text of one file.
 */
namespace compass::cpp::ops {

/**
 * @brief @p text with its // and block comments and its string and
 * character literals turned into blanks, byte for byte, newlines kept, as
 * the script erases them: a literal runs to its closing quote across
 * lines, a backslash escapes the next byte, and there are no raw strings.
 */
[[nodiscard]] std::string erase_comments_and_literals(std::string_view text);

/**
 * @brief Adds to @p macros every macro the erased @p text defines whose
 * body is a symbol visibility or DLL storage attribute:
 * BOOST_SYMBOL_EXPORT, BOOST_SYMBOL_IMPORT, __declspec(dllimport),
 * __declspec(dllexport) or __attribute__((visibility. Lines continued by
 * a backslash are joined first.
 */
void add_visibility_macros(std::string_view erased,
    std::set<std::string, std::less<>>& macros);

/**
 * @brief One enum declaration: where its keyword is, the stripped line of
 * the erased text it is on, and the visibility macro after the keyword,
 * empty when there is none.
 */
struct enum_site {
    std::uint32_t offset = 0;
    std::string source;
    std::string macro;
};

/**
 * @brief Every enum the erased @p text declares: "enum", optionally
 * "class" or "struct", a name, perhaps after one of @p macros, then ":",
 * "{" or ";". An enum after "using" is not a declaration.
 */
[[nodiscard]] std::vector<enum_site> enum_sites(std::string_view erased,
    const std::set<std::string, std::less<>>& macros);

/**
 * @brief The enums of the erased @p text that enum_sites would find
 * decorated were the name after the keyword a macro, each with that name
 * as its macro, whether or not it is one. enum_sites finds a decorated
 * site exactly when its macro is one of the macros it is given.
 */
[[nodiscard]] std::vector<enum_site> macro_enum_sites(
    std::string_view erased);

}

#endif
