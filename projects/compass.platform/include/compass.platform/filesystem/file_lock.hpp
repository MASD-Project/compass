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
#ifndef COMPASS_PLATFORM_FILESYSTEM_FILE_LOCK_HPP
#define COMPASS_PLATFORM_FILESYSTEM_FILE_LOCK_HPP

#include <cstdint>
#include <filesystem>

namespace compass::platform::filesystem {

/**
 * @brief An exclusive advisory lock on a file, which is created when
 * missing. Every process that locks the same file contends for it. It is
 * held until destruction, and the system releases it when the process
 * dies.
 */
class file_lock final {
public:
    /**
     * @brief Locks @p path, waiting for the lock when @p wait, else taking
     * it only when it is free.
     *
     * @throws std::system_error naming the path when the file cannot be
     * opened or locked.
     */
    file_lock(const std::filesystem::path& path, bool wait);
    ~file_lock();

    file_lock(const file_lock&) = delete;
    file_lock& operator=(const file_lock&) = delete;

    [[nodiscard]] bool held() const noexcept { return held_; }

private:
    std::intptr_t handle_;
    bool held_ = false;
};

}

#endif
