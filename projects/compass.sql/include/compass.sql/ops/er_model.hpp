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
#ifndef COMPASS_SQL_OPS_ER_MODEL_HPP
#define COMPASS_SQL_OPS_ER_MODEL_HPP

#include <cstdint>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief The entity-relationship model of a schema, read from its create
 * scripts as ORE Studio's plantuml_er_parse_sql.py reads them: by regular
 * expression, statement shape by statement shape, so a script it misreads
 * is misread here the same way.
 */
namespace compass::sql::ops {

/**
 * @brief A table prefix of COMPONENT_PREFIXES: the component it names, as
 * the script lists them, in its order.
 */
struct er_component {
    std::string prefix;
    std::string name;
    std::string description;
    std::string color;
    double order = 99;
};

/**
 * @brief The stereotype and colour of a table classification, from
 * TABLE_STYLES.
 */
struct er_style {
    std::string classification;
    std::string stereotype;
    std::string color;
};

/**
 * @brief The script's tables: the components in order, the columns it
 * takes as keys by name, and the styles.
 */
struct er_conventions {
    std::vector<er_component> components;
    std::set<std::string, std::less<>> key_columns;
    std::vector<er_style> styles;
};

/**
 * @brief A create script: its path and its text, as read_text() gives it.
 */
struct er_script {
    std::string_view path;
    std::string_view text;
};

struct er_column {
    std::string name;
    std::string type;
    bool nullable = true;
    bool is_pk = false;
    bool is_fk = false;
    bool is_unique = false;
};

/**
 * @brief A column of a table's key section, as the diagram shows it.
 */
struct er_key_column {
    std::string name;
    std::string type;
    bool is_fk = false;
};

/**
 * @brief A table as the diagram draws it: its key columns, and its other
 * columns with the temporal ones last.
 */
struct er_table {
    std::string name;
    std::string stereotype;
    std::string color;
    std::string description;
    std::vector<er_key_column> key;
    std::vector<er_column> columns;
};

struct er_package {
    std::string name;
    std::string description;
    std::string color;
    std::vector<er_table> tables;
};

/**
 * @brief An edge from the parent table to the child.
 */
struct er_relationship {
    std::string from_table;
    std::string to_table;
    std::string cardinality;
    std::string label;
};

/**
 * @brief A table as the scripts define it: its name, the script and the
 * 1-based line of its last definition, as the script keeps a table a
 * later script defines again, and the names of its columns.
 */
struct er_defined_table {
    std::string name;
    std::string path;
    std::uint32_t line = 0;
    std::vector<std::string> columns;
};

/**
 * @brief The packages in their order, each with its tables by name, the
 * relationships in the order the scripts state them, and every table in
 * the order first defined.
 */
struct er_model {
    std::vector<er_package> packages;
    std::vector<er_relationship> relationships;
    std::vector<er_defined_table> tables;
};

/**
 * @brief The model of @p scripts, read in the order given, which is the
 * script's: pathlib's order of every *_create.sql under the create
 * directory.
 */
[[nodiscard]] er_model er_model_of(std::span<const er_script> scripts,
    const er_conventions& conventions);

}

#endif
