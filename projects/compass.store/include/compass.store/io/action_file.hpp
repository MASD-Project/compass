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
#ifndef COMPASS_STORE_IO_ACTION_FILE_HPP
#define COMPASS_STORE_IO_ACTION_FILE_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include "compass.store/types/blob_id.hpp"

namespace compass::store::io {

/**
 * @brief The body of the action object @p key under @p dir, or nothing when
 * it is missing or damaged. A damaged object reads as missing: the action
 * runs again and its next write replaces the object.
 */
[[nodiscard]] std::optional<std::string> read_action(
    const std::filesystem::path& dir, const types::blob_id& key);

/**
 * @brief Puts the action object @p key holding @p body under @p dir, laid
 * out as objects are, by temporary file and rename, replacing any object of
 * that key.
 *
 * @throws std::filesystem::filesystem_error when the write fails.
 */
void write_action(const std::filesystem::path& dir,
    const types::blob_id& key, std::string_view body);

}

#endif
