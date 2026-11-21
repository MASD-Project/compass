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
#ifndef COMPASS_PLATFORM_FILESYSTEM_NEW_FILE_HPP
#define COMPASS_PLATFORM_FILESYSTEM_NEW_FILE_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>

namespace compass::platform::filesystem {

/**
 * @brief A file this process created, open for writing. Move-only. A file
 * still open on destruction is closed and left on disk: whoever created it
 * removes it.
 */
class new_file final {
public:
    /**
     * @brief Creates the file at @p path, which must not exist. Nothing
     * when it already does. On POSIX an @p owner_only file is readable and
     * writable by its owner alone from the moment it exists, as a file of
     * secrets must be.
     *
     * @throws std::system_error naming the path for any other failure.
     */
    [[nodiscard]] static std::optional<new_file> create(
        const std::filesystem::path& path, bool owner_only = false);

    ~new_file();

    new_file(const new_file&) = delete;
    new_file& operator=(const new_file&) = delete;
    new_file(new_file&& other) noexcept;
    new_file& operator=(new_file&& other) noexcept;

    /**
     * @brief Appends all of @p bytes.
     *
     * @throws std::system_error naming the path when they cannot be written.
     */
    void write(std::span<const std::byte> bytes);

    /**
     * @brief Closes the file.
     *
     * @throws std::system_error naming the path when the close fails, which
     * may mean what was written is not all on disk.
     */
    void close();

private:
    new_file(std::filesystem::path path, std::intptr_t handle) noexcept;
    void release() noexcept;

    std::filesystem::path path_;
    std::intptr_t handle_;
};

/**
 * @brief Renames @p from to @p to, replacing any file there, in one step: a
 * reader opens the old file or the new one, and one already reading the
 * old file keeps reading it. On Windows a reader must open the file
 * sharing deletion, as read_file and mapped_file do.
 *
 * @throws std::system_error naming the paths when the rename fails.
 */
void replace_file(const std::filesystem::path& from,
    const std::filesystem::path& to);

}

#endif
