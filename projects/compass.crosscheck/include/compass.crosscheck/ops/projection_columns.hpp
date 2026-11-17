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
#ifndef COMPASS_CROSSCHECK_OPS_PROJECTION_COLUMNS_HPP
#define COMPASS_CROSSCHECK_OPS_PROJECTION_COLUMNS_HPP

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"

/**
 * @brief The columns of ORE Studio's projections that carry a leaf of an
 * ORE document: the curve family columns generate_curve_column_guards.py
 * maps, and the convention kind terms generate_convention_kind_terms.py
 * seeds. Those two generators write them and the value set and element
 * map generators read them, so they are derived once, here.
 */
namespace compass::crosscheck::ops {

/**
 * @brief A curve family column: its code (entity|column), entity, column,
 * table and the leaf it holds.
 */
struct projection_column {
    std::string code;
    std::string entity;
    std::string column;
    std::string table;
    std::string xpath;
};

/**
 * @brief generate_curve_column_guards.rows(), in order of code; nothing
 * where the script raises.
 */
[[nodiscard]] std::optional<std::vector<projection_column>> curve_columns(
    const compass::codec::types::corpus_view& v);

/**
 * @brief A convention term: the kind, entity and column it travels under,
 * its leaf of the conventions document and its place among its kind's.
 */
struct convention_term {
    std::string code;
    std::string kind;
    std::string entity;
    std::string column;
    std::string xpath;
    int position = 0;
};

struct convention_terms_result {
    std::string schema_name;
    std::vector<convention_term> rows;
};

/**
 * @brief generate_convention_kind_terms.rows(), in the mapper's order;
 * nothing where the script reports a problem or stops.
 */
[[nodiscard]] std::optional<convention_terms_result> convention_terms(
    const compass::codec::types::corpus_view& v);

/**
 * @brief The table the refdata model of @p entity names, as
 * generate_convention_kind_terms.table_of() reads it; nothing where it
 * exits.
 */
[[nodiscard]] std::optional<std::string> convention_table(
    const compass::codec::types::corpus_view& v, std::string_view entity);

}

#endif
