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
#ifndef COMPASS_CROSSCHECK_OPS_CURVE_WIRING_HPP
#define COMPASS_CROSSCHECK_OPS_CURVE_WIRING_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/**
 * @brief The curve configuration wiring that ORE Studio's
 * generate_curve_section_parameters.py reads from the projection that maps
 * curveconfig.xml and from the ORE schema: which column of which table
 * holds which leaf of each section's entries. The curve-sections-wiring and
 * curve-column-guards generators both write from it.
 *
 * Every list keeps the order the script's dicts and lists give it, because
 * the files are written in that order.
 */
namespace compass::crosscheck::ops::curve {

inline constexpr std::string_view projection_path =
    "projects/ores.ore/core/src/domain/curve_configuration_projection.cpp";
inline constexpr std::string_view schema_path =
    "external/ore/xsd/curveconfig.xsd";
inline constexpr std::string_view schema_name = "curveconfig.xsd";
inline constexpr std::string_view definition_entity = "curve_definition";

/**
 * @brief A column of an entry's table and the leaf it reads.
 */
struct parameter {
    std::string entity;
    std::string column;
    std::string ore;
    std::string xpath;
};

struct entity_vector {
    std::string entity;
    std::string vector;
};

/**
 * @brief A child element an entry holds, and the column or the entity
 * that records its presence; each is empty for none.
 */
struct block {
    std::string element;
    std::string presence;
    std::string presence_entity;
};

/**
 * @brief A leaf one segment kind reads, and the part that holds it.
 */
struct kind_leaf {
    std::string column;
    std::string ore;
    std::string entity;
    std::string vector;
};

struct segment_kind {
    std::string kind;
    std::vector<std::string> parts;
    std::vector<kind_leaf> leaves;
};

/**
 * @brief A child element an entry holds rows of, and the columns that
 * place its leaves.
 */
struct collection {
    std::string element;
    std::string entity;
    std::string vector;
    std::string parent_key;
    std::string kind_column;
    std::string quote_parent;
    std::string part_parent;
    std::vector<segment_kind> kinds;
};

struct section {
    std::string name;
    std::string entry_element;
    std::vector<entity_vector> entities;
    std::vector<parameter> parameters;
    std::vector<block> blocks;
    std::vector<collection> collections;
};

/**
 * @brief The entry element and the leaves the schema declares for one
 * section.
 */
struct schema_section {
    std::string section;
    std::string entry;
    std::vector<std::string> leaves;
};

/**
 * @brief leaves_of_schema(): nothing where the script stops, on a type
 * the schema does not declare.
 */
[[nodiscard]] std::optional<std::vector<schema_section>> leaves_of_schema(
    std::string_view schema);

struct wiring_result {
    std::vector<section> sections;
    std::vector<std::string> problems;
};

/**
 * @brief wiring(): the sections and the problems; nothing where the
 * script stops, on a section table it cannot read.
 */
[[nodiscard]] std::optional<wiring_result> wiring(std::string_view projection,
    const std::vector<schema_section>& declared);

/**
 * @brief kind_leaves(): the kinds of a segment, and the problems.
 */
[[nodiscard]] std::pair<std::vector<segment_kind>, std::vector<std::string>>
kind_leaves(std::string_view projection);

using types_by_part =
    std::vector<std::pair<std::string, std::vector<std::string>>>;

/**
 * @brief default_curve_types(): the default curve types that have each
 * part, by part, and the problems.
 */
[[nodiscard]] std::pair<types_by_part, std::vector<std::string>>
default_curve_types(std::string_view projection);

/**
 * @brief textwrap.fill(@p text, width=@p width, with @p indent as both
 * indents and break_on_hyphens=False), for text of single-space separated
 * words.
 */
[[nodiscard]] std::string fill(std::string_view text, std::size_t width,
    std::string_view indent);

}

#endif
