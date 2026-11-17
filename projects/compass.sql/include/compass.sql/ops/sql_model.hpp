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
#ifndef COMPASS_SQL_OPS_SQL_MODEL_HPP
#define COMPASS_SQL_OPS_SQL_MODEL_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "compass.sql/ops/lexer.hpp"

/**
 * @brief What one SQL file states, read from the tokens of ops::lex: the
 * file's statements split at code semicolons and meta-commands, at the top
 * level and inside every dollar body. Names are lower-cased, without
 * quotes or schema; an offset is that of the statement's first token in
 * the file.
 */
namespace compass::sql::ops {

/**
 * @brief A \i or \ir line, or psql's long forms \include and
 * \include_relative: whether it is relative and the path it names.
 */
struct sql_include {
    bool relative = false;
    std::string path;
    std::uint32_t offset = 0;
};

struct sql_column {
    std::string name;
    std::string type;
};

/**
 * @brief A create table: its columns in order and the columns of its
 * primary key, stated on a column or as a constraint.
 */
struct sql_table {
    std::string name;
    std::uint32_t offset = 0;
    std::vector<sql_column> columns;
    std::vector<std::string> primary_key;
};

/**
 * @brief A create index: its name, table and the text between its
 * parentheses, lower-cased with spaces collapsed.
 */
struct sql_index {
    std::string name;
    std::string table;
    std::string columns;
    bool unique = false;
    bool if_not_exists = false;
    std::uint32_t offset = 0;
};

/**
 * @brief A create policy, and what its expression compares: whether it is
 * restrictive, names party_id, or compares the current tenant with the
 * system tenant in either order.
 */
struct sql_policy {
    std::string name;
    std::string table;
    std::uint32_t offset = 0;
    bool restrictive = false;
    bool names_party_id = false;
    bool widens_to_system_tenant = false;
};

/**
 * @brief A lookup a function guards: a select from table whose where
 * clause compares each column with a parameter, followed by a raise of a
 * "not found" exception.
 */
struct sql_guard {
    std::string table;
    std::vector<std::pair<std::string, std::string>> columns;
};

/**
 * @brief An insert into table of a list of values: each column with the
 * text of the value it is given.
 */
struct sql_insert {
    std::string table;
    std::vector<std::pair<std::string, std::string>> values;
};

/**
 * @brief A create function: its parameters by name, what its body guards
 * and inserts, and the tables its body inserts into, updates or deletes
 * from, in the order the body names them.
 */
struct sql_function {
    std::string name;
    std::uint32_t offset = 0;
    std::vector<std::string> parameters;
    std::vector<sql_guard> guards;
    std::vector<sql_insert> inserts;
    std::vector<std::string> writes;
};

/**
 * @brief A call of a function named NAME_fn with at least one literal
 * argument: each top-level argument's value when it is one string
 * literal, and nothing otherwise.
 */
struct sql_call {
    std::string function;
    std::uint32_t offset = 0;
    std::vector<std::optional<std::string>> arguments;
};

/**
 * @brief Where a statement or call the model reads does not close its
 * parentheses.
 */
struct sql_unbalanced {
    std::uint32_t offset = 0;
    bool call = false;
};

struct sql_file {
    std::vector<sql_include> includes;
    std::vector<sql_table> tables;
    std::vector<sql_index> indexes;
    std::vector<std::string> row_level_security;
    std::vector<sql_policy> policies;
    std::vector<sql_function> functions;
    std::vector<sql_call> calls;
    std::vector<sql_unbalanced> unbalanced;
};

/**
 * @brief The includes of @p text, lexed as @p tokens: what model_of gives
 * as its includes, without reading anything else.
 */
[[nodiscard]] std::vector<sql_include> includes_of(std::string_view text,
    const lexed& tokens);

/**
 * @brief The model of @p text, lexed as @p tokens.
 */
[[nodiscard]] sql_file model_of(std::string_view text, const lexed& tokens);

[[nodiscard]] inline sql_file model_of(std::string_view text) {
    return model_of(text, lex(text));
}

}

#endif
