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
#ifndef COMPASS_DB_IO_SCHEMA_HPP
#define COMPASS_DB_IO_SCHEMA_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "compass.db/io/connection.hpp"

namespace compass::db::io {

/**
 * @brief The text search configuration a database is created with when
 * none is given.
 */
inline constexpr std::string_view default_text_search = "english";

/**
 * @brief Where a database's schema stands against this compass. version
 * is 0 for a database compass never initialised. text_search is the
 * configuration every section's tsvector is computed with, empty before
 * init.
 */
struct schema_status {
    std::uint32_t version = 0;
    std::uint32_t known = 0;
    std::string text_search;

    [[nodiscard]] bool initialised() const noexcept { return version != 0; }
    [[nodiscard]] bool newer() const noexcept { return version > known; }
    [[nodiscard]] bool current() const noexcept { return version == known; }
};

/**
 * @brief Reads the schema version and settings of the database of @p c.
 * @throws error when the database cannot be read.
 */
[[nodiscard]] schema_status read_status(connection& c);

/**
 * @brief Applies every migration the database lacks, each in its own
 * transaction, under a lock that makes a concurrent migrate wait, and
 * returns the versions applied.
 * @throws error when the schema is newer than this compass knows, or a
 * migration fails; the migrations before it stay applied.
 */
std::vector<std::uint32_t> migrate(connection& c);

/**
 * @brief Creates the schemas of a new database, or migrates an existing
 * one, and records @p text_search as its text search configuration.
 * @throws error when @p text_search is not a configuration the server
 * has, when the database already uses another, or as migrate does.
 */
schema_status init(connection& c, std::string_view text_search);

/**
 * @brief Why a command cannot use a database whose schema stands as @p s,
 * and what to run: one never initialised, one newer than this compass,
 * and one older, which only compass db migrate brings up to date. Nothing
 * when the schema is this compass's.
 */
[[nodiscard]] std::optional<std::string> refusal_of(const schema_status& s);

/**
 * @brief Refuses a database a command cannot use: one never initialised,
 * one newer than this compass, and one older, which only compass db
 * migrate brings up to date.
 * @throws error saying which and what to run.
 */
void require_current(connection& c);

}

#endif
