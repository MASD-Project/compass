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
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
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
        std::system_category(),
        std::string(what) + " " + path.generic_string());
}

// FileRenameInfoEx and its flags, which the SDK declares only for a
// _WIN32_WINNT of Windows 10 1607 or later. The flags sit where
// FILE_RENAME_INFO's ReplaceIfExists does, widened to a DWORD.
constexpr auto file_rename_info_ex =
    static_cast<FILE_INFO_BY_HANDLE_CLASS>(22);
constexpr DWORD rename_replace_if_exists = 0x1;
constexpr DWORD rename_posix_semantics = 0x2;

HANDLE native(std::intptr_t handle) noexcept {
    return reinterpret_cast<HANDLE>(handle);
}

#else

[[noreturn]] void fail(const char* what, const std::filesystem::path& path) {
    throw std::system_error(errno, std::generic_category(),
        std::string(what) + " " + path.generic_string());
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

void replace_file(const std::filesystem::path& from,
    const std::filesystem::path& to) {
    const auto what = "cannot rename " + from.generic_string() + " to";
#if defined(_WIN32)
    // MoveFileEx fails while another process has the target open, so the
    // rename takes POSIX semantics, which Windows 10 has had since 1607.
    const HANDLE h = ::CreateFileW(from.c_str(), DELETE | SYNCHRONIZE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        fail(what.c_str(), to);
    const std::wstring target = L"\\\\?\\" +
        std::filesystem::absolute(to).make_preferred().wstring();
    const auto name_bytes = target.size() * sizeof(wchar_t);
    std::vector<std::byte> buffer(
        sizeof(FILE_RENAME_INFO) + name_bytes + sizeof(wchar_t));
    auto* info = reinterpret_cast<FILE_RENAME_INFO*>(buffer.data());
    const DWORD flags = rename_replace_if_exists | rename_posix_semantics;
    std::memcpy(buffer.data(), &flags, sizeof flags);
    info->RootDirectory = nullptr;
    info->FileNameLength = static_cast<DWORD>(name_bytes);
    std::memcpy(info->FileName, target.c_str(), name_bytes + sizeof(wchar_t));
    const BOOL renamed = ::SetFileInformationByHandle(h,
        file_rename_info_ex, info, static_cast<DWORD>(buffer.size()));
    const DWORD error = ::GetLastError();
    ::CloseHandle(h);
    if (renamed)
        return;
    // A file system without POSIX semantics, such as FAT, takes the
    // plain replacing move.
    if ((error == ERROR_INVALID_PARAMETER || error == ERROR_NOT_SUPPORTED) &&
        ::MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING))
        return;
    ::SetLastError(error);
    fail(what.c_str(), to);
#else
    if (std::rename(from.c_str(), to.c_str()) != 0)
        fail(what.c_str(), to);
#endif
}

}
