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
#include "compass.platform/filesystem/new_file.hpp"

#include <algorithm>
#include <cerrno>
#include <string>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace compass::platform::filesystem {

namespace {

constexpr std::intptr_t no_handle = -1;

#if defined(_WIN32)

[[noreturn]] void fail(const char* what, const std::filesystem::path& path) {
    throw std::system_error(static_cast<int>(::GetLastError()),
        std::system_category(), std::string(what) + " " + path.string());
}

HANDLE native(std::intptr_t handle) noexcept {
    return reinterpret_cast<HANDLE>(handle);
}

#else

[[noreturn]] void fail(const char* what, const std::filesystem::path& path) {
    throw std::system_error(errno, std::generic_category(),
        std::string(what) + " " + path.string());
}

#endif

}

std::optional<new_file> new_file::create(const std::filesystem::path& path) {
#if defined(_WIN32)
    const HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        if (::GetLastError() == ERROR_FILE_EXISTS)
            return std::nullopt;
        fail("cannot create", path);
    }
    return new_file(path, reinterpret_cast<std::intptr_t>(h));
#else
    const int fd = ::open(path.c_str(),
        O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
    if (fd == -1) {
        if (errno == EEXIST)
            return std::nullopt;
        fail("cannot create", path);
    }
    return new_file(path, fd);
#endif
}

new_file::new_file(std::filesystem::path path, std::intptr_t handle) noexcept
    : path_(std::move(path)), handle_(handle) {}

new_file::~new_file() {
    release();
}

new_file::new_file(new_file&& other) noexcept
    : path_(std::move(other.path_)),
      handle_(std::exchange(other.handle_, no_handle)) {}

new_file& new_file::operator=(new_file&& other) noexcept {
    if (this != &other) {
        release();
        path_ = std::move(other.path_);
        handle_ = std::exchange(other.handle_, no_handle);
    }
    return *this;
}

void new_file::write(std::span<const std::byte> bytes) {
    while (!bytes.empty()) {
#if defined(_WIN32)
        const auto wanted = static_cast<DWORD>(
            std::min<std::size_t>(bytes.size(), 1u << 30));
        DWORD n = 0;
        if (!::WriteFile(native(handle_), bytes.data(), wanted, &n, nullptr))
            fail("cannot write", path_);
#else
        const auto n = ::write(static_cast<int>(handle_), bytes.data(),
            bytes.size());
        if (n < 0) {
            if (errno == EINTR)
                continue;
            fail("cannot write", path_);
        }
#endif
        bytes = bytes.subspan(static_cast<std::size_t>(n));
    }
}

void new_file::close() {
    const auto handle = std::exchange(handle_, no_handle);
    if (handle == no_handle)
        return;
#if defined(_WIN32)
    if (!::CloseHandle(native(handle)))
        fail("cannot write", path_);
#else
    if (::close(static_cast<int>(handle)) != 0)
        fail("cannot write", path_);
#endif
}

void new_file::release() noexcept {
    const auto handle = std::exchange(handle_, no_handle);
    if (handle == no_handle)
        return;
#if defined(_WIN32)
    ::CloseHandle(native(handle));
#else
    ::close(static_cast<int>(handle));
#endif
}

}
