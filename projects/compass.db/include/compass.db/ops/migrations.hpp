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
#ifndef COMPASS_DB_OPS_MIGRATIONS_HPP
#define COMPASS_DB_OPS_MIGRATIONS_HPP

#include <cstdint>
#include <span>
#include <string_view>

namespace compass::db::ops {

/**
 * @brief One step of the schema: its version, a name for compass db status,
 * and the SQL that takes the schema from the version before to it.
 */
struct migration {
    std::uint32_t version = 0;
    std::string_view name;
    std::string_view sql;
};

/**
 * @brief Every migration this compass knows, in version order from 1 with
 * no gap. A migration, once released, is never edited: a change to the
 * schema is a new one.
 */
[[nodiscard]] std::span<const migration> migrations() noexcept;

/**
 * @brief The version of the last migration: the schema this compass reads
 * and writes.
 */
[[nodiscard]] std::uint32_t known_version() noexcept;

/**
 * @brief The SQL that creates the bookkeeping every migration relies on:
 * the compass schema with its migration and setting tables. Idempotent.
 */
[[nodiscard]] std::string_view bookkeeping_sql() noexcept;

}

#endif
