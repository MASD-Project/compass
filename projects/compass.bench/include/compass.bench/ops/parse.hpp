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
#ifndef COMPASS_BENCH_OPS_PARSE_HPP
#define COMPASS_BENCH_OPS_PARSE_HPP

#include <cstdint>
#include <optional>
#include <string_view>
#include <tree_sitter/api.h>
#include "compass.bench/types/parse_result.hpp"

namespace compass::bench::ops {

/**
 * @brief The language a file with @p extension is parsed as, if any.
 *
 * The C and C++ extensions of the include stage map to cpp, .sql to sql.
 */
[[nodiscard]] std::optional<types::language> language_of(
    std::string_view extension);

/**
 * @brief The short name of @p l, as the report prints it.
 */
[[nodiscard]] std::string_view name_of(types::language l);

/**
 * @brief The tree-sitter grammar of @p l.
 */
[[nodiscard]] const TSLanguage* grammar_of(types::language l);

/**
 * @brief Counts the ERROR and MISSING nodes at or below @p node.
 *
 * Each node counts once, so an ERROR nested in another ERROR counts twice,
 * once for each node. Subtrees without errors are skipped.
 */
[[nodiscard]] std::uint32_t count_error_nodes(TSNode node);

}

#endif
