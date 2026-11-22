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
#ifndef COMPASS_CODEGEN_CODECS_CODEGEN_HPP
#define COMPASS_CODEGEN_CODECS_CODEGEN_HPP

#include <filesystem>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codec/types/descriptor.hpp"
#include "compass.codec/types/formatter.hpp"
#include "compass.codegen/ops/generate.hpp"

namespace compass::codegen::codecs {

/**
 * @brief Generates what ores.codegen's regenerate --all writes, as one
 * corpus unit: loads the models, builds their contexts and renders every
 * output. Off by default.
 *
 * Options, every path relative to the checkout:
 * - library: the directory of ores.codegen's library, which holds
 *   templates/, data/ and component_catalogue.org. Required.
 * - convention_component: the component of the catalogue whose modeling
 *   directory declares the convention families. Required.
 * - year: the copyright year of the licence blocks. Required: it is
 *   configuration, never the clock.
 * - clang_format and prettier: the programs that format C++ and
 *   TypeScript outputs, each a comma-separated list of candidates of
 *   which the first found is used: a name is searched on PATH, a path is
 *   relative to the checkout. Empty for none.
 * - clang_format_style and prettier_style: their style files.
 *
 * The unit reads documents by path, the library's files and the listings
 * of projects/ and of the library, so it runs again only when one of them
 * changes, not for any other edit.
 */
extern const compass::codec::types::descriptor codegen;

/**
 * @brief The settings the options @p o give, for the compass running at
 * @p executable, against whose directory @c @compass resolves.
 * @throws std::runtime_error naming the option when library, product or
 * convention_component is empty, or year is empty or not a year, and
 * naming the directory when a library outside the checkout is not one.
 */
[[nodiscard]] ops::generation_settings settings_of(
    const compass::codec::types::option_values& o,
    const std::filesystem::path& executable);

/**
 * @brief settings_of for the compass that is running.
 */
[[nodiscard]] ops::generation_settings settings_of(
    const compass::codec::types::option_values& o);

/**
 * @brief The formatters ores.codegen runs over what it writes, from the
 * options @p o, as generate.py's clang_format_files and
 * prettier_format_files run them: clang-format twice over C++ with its
 * style file, and prettier once over TypeScript with its configuration. A
 * formatter whose option is empty is left out.
 * @throws std::runtime_error when no candidate of a non-empty program
 * option is found, or its style option is empty.
 */
[[nodiscard]] std::vector<compass::codec::types::formatter> formatters_of(
    const compass::codec::types::option_values& o,
    const std::filesystem::path& checkout);

/**
 * @brief A reader whose every read of the checkout goes through @p v, so
 * the unit records what it reads, and which reads a library outside the
 * checkout from disk; the codec's environment keys those reads.
 */
[[nodiscard]] ops::checkout_reader reader_of(
    const compass::codec::types::corpus_view& v);

}

#endif
