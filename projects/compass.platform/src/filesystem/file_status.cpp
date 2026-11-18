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
#include "compass.platform/filesystem/file_status.hpp"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#endif

namespace compass::platform::filesystem {

namespace {

#if defined(_WIN32)

[[noreturn]] void throw_last_error(const char* operation,
    const std::filesystem::path& path) {
    throw std::system_error(static_cast<int>(::GetLastError()),
        std::system_category(),
        std::string(operation) + " " + path.generic_string());
}

/**
 * @brief A FILETIME, which counts 100 ns ticks from 1601, as a time since
 * the Unix epoch.
 */
file_time to_file_time(const FILETIME& t) noexcept {
    constexpr std::int64_t ticks_to_unix_epoch = 116'444'736'000'000'000;
    constexpr std::int64_t ticks_per_second = 10'000'000;
    const auto ticks = static_cast<std::int64_t>(
        (std::uint64_t{t.dwHighDateTime} << 32) | t.dwLowDateTime) -
        ticks_to_unix_epoch;
    return {ticks / ticks_per_second,
        static_cast<std::uint32_t>(ticks % ticks_per_second * 100)};
}

std::uint64_t to_size(DWORD high, DWORD low) noexcept {
    return (std::uint64_t{high} << 32) | low;
}

class file_handle final {
public:
    // Backup semantics open a directory too, so status() refuses it as not
    // regular, as on POSIX, rather than the open failing as access denied.
    explicit file_handle(const std::filesystem::path& path) : path_(path) {
        handle_ = ::CreateFileW(path.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (handle_ == INVALID_HANDLE_VALUE)
            throw_last_error("open", path);
    }
    ~file_handle() { ::CloseHandle(handle_); }
    file_handle(const file_handle&) = delete;
    file_handle& operator=(const file_handle&) = delete;

    file_status status() const {
        BY_HANDLE_FILE_INFORMATION info{};
        if (!::GetFileInformationByHandle(handle_, &info))
            throw_last_error("stat", path_);
        if ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
            ::GetFileType(handle_) != FILE_TYPE_DISK)
            throw std::system_error(EINVAL, std::generic_category(),
                "read of non-regular file " + path_.generic_string());
        file_status s;
        s.size = to_size(info.nFileSizeHigh, info.nFileSizeLow);
        s.modified = to_file_time(info.ftLastWriteTime);
        s.changed = to_file_time(info.ftCreationTime);
        return s;
    }

    /**
     * @brief Reads into @p bytes from @p done on, until the end of the
     * file, and returns how many bytes the file held.
     */
    std::size_t read_rest(std::string& bytes, std::size_t done) const {
        for (;;) {
            if (done == bytes.size())
                bytes.resize(bytes.size() + 4096);
            const auto wanted = static_cast<DWORD>(
                std::min<std::size_t>(bytes.size() - done, 1u << 30));
            DWORD n = 0;
            if (!::ReadFile(handle_, bytes.data() + done, wanted, &n, nullptr))
                throw_last_error("read", path_);
            if (n == 0)
                return done;
            done += n;
        }
    }

private:
    const std::filesystem::path& path_;
    HANDLE handle_;
};

#else

[[noreturn]] void throw_errno(const char* operation,
    const std::filesystem::path& path) {
    throw std::system_error(errno, std::generic_category(),
        std::string(operation) + " " + path.generic_string());
}

file_time to_file_time(const struct timespec& t) noexcept {
    return {static_cast<std::int64_t>(t.tv_sec),
        static_cast<std::uint32_t>(t.tv_nsec)};
}

file_status to_status(const struct stat& st) noexcept {
    file_status s;
    s.size = static_cast<std::uint64_t>(st.st_size);
#if defined(__APPLE__)
    s.modified = to_file_time(st.st_mtimespec);
    s.changed = to_file_time(st.st_ctimespec);
#else
    s.modified = to_file_time(st.st_mtim);
    s.changed = to_file_time(st.st_ctim);
#endif
    s.device = static_cast<std::uint64_t>(st.st_dev);
    s.inode = static_cast<std::uint64_t>(st.st_ino);
    s.user = static_cast<std::uint32_t>(st.st_uid);
    s.group = static_cast<std::uint32_t>(st.st_gid);
    return s;
}

class file_handle final {
public:
    explicit file_handle(const std::filesystem::path& path) : path_(path) {
        fd_ = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd_ == -1)
            throw_errno("open", path);
    }
    ~file_handle() { ::close(fd_); }
    file_handle(const file_handle&) = delete;
    file_handle& operator=(const file_handle&) = delete;

    file_status status() const {
        struct stat st{};
        if (::fstat(fd_, &st) == -1)
            throw_errno("fstat", path_);
        if (!S_ISREG(st.st_mode))
            throw std::system_error(EINVAL, std::generic_category(),
                "read of non-regular file " + path_.generic_string());
        return to_status(st);
    }

    /**
     * @brief Reads into @p bytes from @p done on, until the end of the
     * file, and returns how many bytes the file held.
     */
    std::size_t read_rest(std::string& bytes, std::size_t done) const {
        for (;;) {
            if (done == bytes.size())
                bytes.resize(bytes.size() + 4096);
            const auto n = ::read(fd_, bytes.data() + done,
                bytes.size() - done);
            if (n == 0)
                return done;
            if (n > 0)
                done += static_cast<std::size_t>(n);
            else if (errno != EINTR)
                throw_errno("read", path_);
        }
    }

private:
    const std::filesystem::path& path_;
    int fd_ = -1;
};

#endif

}

std::optional<file_status> regular_file_status(
    const std::filesystem::path& path) {
#if defined(_WIN32)
    WIN32_FILE_ATTRIBUTE_DATA info{};
    if (!::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &info))
        return std::nullopt;
    constexpr DWORD not_regular = FILE_ATTRIBUTE_DIRECTORY |
        FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE;
    if ((info.dwFileAttributes & not_regular) != 0)
        return std::nullopt;
    file_status s;
    s.size = to_size(info.nFileSizeHigh, info.nFileSizeLow);
    s.modified = to_file_time(info.ftLastWriteTime);
    s.changed = to_file_time(info.ftCreationTime);
    return s;
#else
    struct stat st{};
    if (::lstat(path.c_str(), &st) == -1 || !S_ISREG(st.st_mode))
        return std::nullopt;
    return to_status(st);
#endif
}

file_status read_file(const std::filesystem::path& path, std::string& bytes) {
    const file_handle file(path);
    const auto status = file.status();
    bytes.resize(static_cast<std::size_t>(status.size));
    bytes.resize(file.read_rest(bytes, 0));
    return status;
}

void set_modified_time(const std::filesystem::path& path, file_time t) {
#if defined(_WIN32)
    const HANDLE h = ::CreateFileW(path.c_str(), FILE_WRITE_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        throw_last_error("set the time of", path);
    const auto ticks = static_cast<std::uint64_t>(
        t.seconds * 10'000'000 + t.nanoseconds / 100 + 116'444'736'000'000'000);
    FILETIME written{static_cast<DWORD>(ticks),
        static_cast<DWORD>(ticks >> 32)};
    const BOOL set = ::SetFileTime(h, nullptr, nullptr, &written);
    const DWORD error = ::GetLastError();
    ::CloseHandle(h);
    if (!set) {
        ::SetLastError(error);
        throw_last_error("set the time of", path);
    }
#else
    const struct timespec times[2] = {{0, UTIME_OMIT},
        {static_cast<time_t>(t.seconds), static_cast<long>(t.nanoseconds)}};
    if (::utimensat(AT_FDCWD, path.c_str(), times, 0) != 0)
        throw_errno("set the time of", path);
#endif
}

file_time file_clock_now() noexcept {
#if defined(_WIN32)
    FILETIME t{};
    ::GetSystemTimeAsFileTime(&t);
    return to_file_time(t);
#else
    struct timespec t{};
#if defined(__APPLE__)
    // macOS stamps files from the precise clock and has no coarse one.
    ::clock_gettime(CLOCK_REALTIME, &t);
#else
    ::clock_gettime(CLOCK_REALTIME_COARSE, &t);
#endif
    return to_file_time(t);
#endif
}

}
