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
#ifndef COMPASS_CROSSCHECK_OPS_GENERATED_SHAPE_HPP
#define COMPASS_CROSSCHECK_OPS_GENERATED_SHAPE_HPP

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"

namespace compass::crosscheck::ops {

/**
 * @brief A column the generated repository row binds in the binary format
 * of its member's type, with the key or bind of the query builder: the
 * column, the C++ type the row declares for it, or empty when the row
 * declares no member for it, and the SQL type the create script gives it.
 * heading is the title of the model's heading that states the column, a
 * view into the document, or empty when no heading states it.
 */
struct bound_column {
    std::string column;
    std::string member_type;
    std::string sql_type;
    std::string_view heading;
};

/**
 * @brief An index the model declares: its name and its column list as the
 * model writes it, which the create script copies, and the title of the
 * heading that declares it, a view into the document.
 */
struct declared_index {
    std::string name;
    std::string columns;
    std::string_view heading;
};

/**
 * @brief What the code an entity or junction model generates states about
 * its table and its structs, read from the context ores.codegen renders
 * that code from rather than from the code.
 *
 * name is the entity's singular. table is the table its create script
 * makes and table_columns that table's columns, in the order the script
 * writes them. members are the members of the domain struct, a field of a
 * domain group as member.field. iterator is the name the table writer's
 * loop gives a row, display the members the writer streams, each as the
 * writer reaches it, and display_heading the title of the Table display
 * heading that lists them. indexes are the indexes the model declares, and
 * bound the columns the repository row binds in binary, the template's
 * own (tenant, version, audit and validity) left out.
 *
 * creates_table, has_row and has_table_writer say whether code generation
 * renders the create script, the repository row and the table writer for
 * the model; what the shape states of one it does not render is in no
 * generated file.
 */
struct generated_shape {
    bool creates_table = false;
    bool has_row = false;
    bool has_table_writer = false;
    std::string name;
    std::string table;
    std::vector<std::string> table_columns;
    std::set<std::string, std::less<>> members;
    std::string iterator;
    std::vector<std::string> display;
    std::string_view display_heading;
    std::vector<declared_index> indexes;
    std::vector<bound_column> bound;
};

/**
 * @brief The shape of the entity or junction model @p d, loaded as
 * ores.codegen loads it, with the documents it reads found through @p v,
 * and enriched by the steps of compass.codegen's context that decide the
 * shape. None when @p d is no entity or junction model or does not load,
 * which the codegen codec reports.
 */
[[nodiscard]] std::optional<generated_shape> generated_shape_of(
    const compass::codec::types::corpus_view& v,
    const compass::codec::types::document& d);

}

#endif
