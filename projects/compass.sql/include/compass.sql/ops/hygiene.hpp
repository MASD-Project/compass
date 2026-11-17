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
#ifndef COMPASS_SQL_OPS_HYGIENE_HPP
#define COMPASS_SQL_OPS_HYGIENE_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "compass.sql/ops/lexer.hpp"
#include "compass.sql/ops/sql_model.hpp"

/**
 * @brief The rules of ORE Studio's check_sql_hygiene.py over one SQL file,
 * read from its tokens (ops::lex) and its model (ops::model_of).
 */
namespace compass::sql::ops {

/**
 * @brief An index the file creates more than once with "create [unique]
 * index if not exists": its name, table and columns, as the model states
 * them, how often, and where it is first created.
 */
struct duplicate_index {
    std::string name;
    std::string table;
    std::string columns;
    std::uint32_t count = 0;
    std::uint32_t offset = 0;
};

[[nodiscard]] std::vector<duplicate_index> duplicate_indexes(
    const sql_file& model);

/**
 * @brief Where each comment that stands between two string literals
 * starts, dollar-quoted bodies included, in the order of the text.
 */
[[nodiscard]] std::vector<std::uint32_t> split_concatenations(
    std::string_view text, const lexed& tokens);

/**
 * @brief A statement a second run of the file fails on: where it starts
 * and why.
 */
struct not_re_runnable {
    std::uint32_t offset = 0;
    std::string why;
};

/**
 * @brief The top-level DDL of @p text that cannot run twice: create table,
 * sequence, extension, schema or materialized view without "if not
 * exists"; create trigger, rule, view, function or procedure without "or
 * replace"; create index without "if not exists"; create policy, type or
 * domain with no matching drop ... if exists in the file. DDL inside a
 * dollar body is not read. In the order of the text.
 */
[[nodiscard]] std::vector<not_re_runnable> not_re_runnables(
    std::string_view text, const lexed& tokens);

}

#endif
