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
#ifndef COMPASS_PLATFORM_FILESYSTEM_OPEN_EXISTING_HPP
#define COMPASS_PLATFORM_FILESYSTEM_OPEN_EXISTING_HPP

#if defined(_WIN32)

#include <chrono>
#include <filesystem>
#include <thread>
#include <windows.h>

namespace compass::platform::filesystem::detail {

/**
 * @brief Opens the existing file @p path for reading, shared with every
 * other open, with @p flags, or gives INVALID_HANDLE_VALUE with the last
 * error set.
 *
 * For a moment while another thread or process replaces the file at the
 * name, Windows refuses an open as access denied: the store's concurrent
 * writers test saw it in 3 of 30 runs, never with bytes that differed. So
 * that error is tried again, ten times over about 45 ms, before it is
 * believed. A file that is really denied costs those 45 ms and fails as
 * before.
 */
inline HANDLE open_existing(const std::filesystem::path& path, DWORD flags) {
    constexpr int attempts = 10;
    for (int attempt = 1;; ++attempt) {
        const HANDLE h = ::CreateFileW(path.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, flags, nullptr);
        if (h != INVALID_HANDLE_VALUE || attempt == attempts ||
            ::GetLastError() != ERROR_ACCESS_DENIED)
            return h;
        std::this_thread::sleep_for(std::chrono::milliseconds(attempt));
    }
}

}

#endif

#endif
