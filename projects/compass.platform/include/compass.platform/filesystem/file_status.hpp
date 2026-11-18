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
#ifndef COMPASS_PLATFORM_FILESYSTEM_FILE_STATUS_HPP
#define COMPASS_PLATFORM_FILESYSTEM_FILE_STATUS_HPP

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace compass::platform::filesystem {

/**
 * @brief A time stamp of a file: seconds and nanoseconds since the Unix
 * epoch.
 */
struct file_time {
    std::int64_t seconds = 0;
    std::uint32_t nanoseconds = 0;

    [[nodiscard]] constexpr std::int64_t count_ns() const noexcept {
        return seconds * 1'000'000'000 + nanoseconds;
    }
};

/**
 * @brief What the file system records about a regular file, in the fields
 * git keeps in its index.
 *
 * On Windows, @c changed is the creation time, and @c device, @c inode,
 * @c user and @c group are zero, as git for Windows records them.
 */
struct file_status {
    std::uint64_t size = 0;
    file_time modified;
    file_time changed;
    std::uint64_t device = 0;
    std::uint64_t inode = 0;
    std::uint32_t user = 0;
    std::uint32_t group = 0;
};

/**
 * @brief The status of the regular file at @p path, which is not followed
 * when it is a symbolic link. Nothing when there is no such file or it is
 * not a regular file.
 */
[[nodiscard]] std::optional<file_status> regular_file_status(
    const std::filesystem::path& path);

/**
 * @brief Reads the whole file at @p path into @p bytes and returns the
 * status of the file it read, taken from the open file.
 *
 * @throws std::system_error naming the path when the file cannot be opened
 * or read, or is not a regular file, which it refuses before reading.
 */
file_status read_file(const std::filesystem::path& path, std::string& bytes);

/**
 * @brief Sets the modification time of the file at @p path to @p t, which
 * Windows keeps to 100 ns.
 *
 * @throws std::system_error naming the path when it cannot be set.
 */
void set_modified_time(const std::filesystem::path& path, file_time t);

/**
 * @brief The time as the file system stamps a file it modifies now. It
 * comes from the coarse clock the kernel stamps files with, so a file
 * modified after this call has a modification time no earlier than it.
 */
[[nodiscard]] file_time file_clock_now() noexcept;

}

#endif
