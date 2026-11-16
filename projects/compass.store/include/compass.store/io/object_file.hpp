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
#ifndef COMPASS_STORE_IO_OBJECT_FILE_HPP
#define COMPASS_STORE_IO_OBJECT_FILE_HPP

#include <cstddef>
#include <filesystem>
#include <span>

namespace compass::store::io {

/**
 * @brief Puts @p bytes at @p target, whole or not at all.
 *
 * The bytes go to a temporary file in the directory of @p target, which is
 * created when missing, and the file is then renamed over @p target. A
 * reader therefore maps the old object, the new one or none, never part of
 * one, and any number of writers of the same object all succeed and leave
 * one. An object that is already there is replaced.
 *
 * The file is not synced: the store is a cache, and an object a crash left
 * incomplete is rejected when it is loaded and written again. A crash can
 * leave the temporary file behind; its name starts with "tmp_".
 *
 * @throws std::filesystem::filesystem_error when the directory, the file or
 * the rename fails. No temporary file is left then.
 */
void write_object(const std::filesystem::path& target,
    std::span<const std::byte> bytes);

}

#endif
