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
#include <string>
#include <string_view>
#include <vector>
#include "compass.cpp/ops/source_model.hpp"

/**
 * @brief What one source says about the Windows export macros: the facts
 * of ORE Studio's check_exported_types_are_compiled.py and
 * check_enum_export_attributes.py, read from its model.
 */
namespace compass::cpp::ops {

/**
 * @brief The export macro an export.hpp defines: the first
 * "#define ORES_..._EXPORT BOOST_SYMBOL...", when the header also has an
 * "#ifdef ORES_..._LIBRARY" guard; nothing otherwise.
 */
[[nodiscard]] std::optional<std::string_view> export_macro(const lexed& l,
    const source_model& m);

/**
 * @brief The names of the #include "name" directives of @p m, in order.
 */
[[nodiscard]] std::vector<std::string_view> quoted_includes(
    const source_model& m);

/**
 * @brief One class or struct a header exports with a macro. offset is
 * where the line of its name starts.
 */
struct exported_type {
    std::string_view name;
    std::uint32_t offset = 0;
    std::string_view macro;
};

/**
 * @brief The classes and structs @p m declares at namespace scope with an
 * ORES_..._EXPORT macro, each with its macro; none for a @p source that
 * says AUTO-GENERATED FILE.
 */
[[nodiscard]] std::vector<exported_type> exported_types(
    std::string_view source, const source_model& m);

/**
 * @brief True when @p body names a symbol visibility or DLL storage
 * attribute: BOOST_SYMBOL_EXPORT, BOOST_SYMBOL_IMPORT,
 * __declspec(dllimport), __declspec(dllexport) or
 * __attribute__((visibility.
 */
[[nodiscard]] bool names_visibility_attribute(std::string_view body) noexcept;

/**
 * @brief The object-like macros @p m defines whose body names a
 * visibility attribute.
 */
[[nodiscard]] std::vector<std::string_view> visibility_macros(
    const source_model& m);

/**
 * @brief An enum declared with an identifier between its key and its
 * name: where its keyword is, that identifier, which decorates it when it
 * is a visibility macro, and the stripped line of code it is on.
 */
struct enum_site {
    std::uint32_t offset = 0;
    std::string_view macro;
    std::string_view source;
};

[[nodiscard]] std::vector<enum_site> macro_enum_sites(const lexed& l,
    const source_model& m);

}

#endif
