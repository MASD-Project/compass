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
#ifndef COMPASS_PLATFORM_FILESYSTEM_MAPPED_FILE_HPP
#define COMPASS_PLATFORM_FILESYSTEM_MAPPED_FILE_HPP

#include <cstddef>
#include <filesystem>
#include <span>
#include <string_view>

namespace compass::platform::filesystem {

/**
 * @brief Read-only memory-mapped view of a whole file.
 *
 * Owns the mapping and unmaps it on destruction. Move-only. An empty file
 * yields an empty view without mapping anything. Opening or mapping failures
 * throw std::system_error naming the path.
 */
class mapped_file final {
public:
    explicit mapped_file(const std::filesystem::path& path);
    ~mapped_file();

    mapped_file(const mapped_file&) = delete;
    mapped_file& operator=(const mapped_file&) = delete;

    mapped_file(mapped_file&& other) noexcept;
    mapped_file& operator=(mapped_file&& other) noexcept;

    [[nodiscard]] std::span<const std::byte> bytes() const noexcept {
        return {data_, size_};
    }

    [[nodiscard]] std::string_view text() const noexcept {
        return {reinterpret_cast<const char*>(data_), size_};
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

private:
    void release() noexcept;

    const std::byte* data_ = nullptr;
    std::size_t size_ = 0;
};

}

#endif
