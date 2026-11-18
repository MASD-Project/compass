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
#include "compass.platform/filesystem/file_lock.hpp"

#include <cerrno>
#include <string>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace compass::platform::filesystem {

#if defined(_WIN32)

namespace {

[[noreturn]] void fail(const char* what, const std::filesystem::path& path) {
    throw std::system_error(static_cast<int>(::GetLastError()),
        std::system_category(), std::string(what) + " " + path.string());
}

}

file_lock::file_lock(const std::filesystem::path& path, bool wait) {
    const HANDLE h = ::CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        fail("cannot open", path);
    handle_ = reinterpret_cast<std::intptr_t>(h);
    OVERLAPPED whole{};
    const DWORD flags = LOCKFILE_EXCLUSIVE_LOCK |
        (wait ? 0 : LOCKFILE_FAIL_IMMEDIATELY);
    if (::LockFileEx(h, flags, 0, MAXDWORD, MAXDWORD, &whole)) {
        held_ = true;
        return;
    }
    const auto error = ::GetLastError();
    if (error == ERROR_LOCK_VIOLATION && !wait)
        return;
    ::CloseHandle(h);
    ::SetLastError(error);
    fail("cannot lock", path);
}

file_lock::~file_lock() {
    ::CloseHandle(reinterpret_cast<HANDLE>(handle_));
}

#else

namespace {

[[noreturn]] void fail(const char* what, const std::filesystem::path& path) {
    throw std::system_error(errno, std::generic_category(),
        std::string(what) + " " + path.string());
}

}

file_lock::file_lock(const std::filesystem::path& path, bool wait) {
    const int fd = ::open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (fd == -1)
        fail("cannot open", path);
    handle_ = fd;
    while (true) {
        if (::flock(fd, LOCK_EX | (wait ? 0 : LOCK_NB)) == 0) {
            held_ = true;
            return;
        }
        if (errno == EINTR)
            continue;
        if (errno == EWOULDBLOCK && !wait)
            return;
        const int error = errno;
        ::close(fd);
        errno = error;
        fail("cannot lock", path);
    }
}

file_lock::~file_lock() {
    ::close(static_cast<int>(handle_));
}

#endif

}
