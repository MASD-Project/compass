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
#include "compass.platform/filesystem/mapped_file.hpp"

#include <cerrno>
#include <cstdint>
#include <string>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#include <algorithm>
#include <windows.h>
#include "open_existing.hpp"
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace compass::platform::filesystem {

#if defined(_WIN32)

namespace {

[[noreturn]] void throw_error(int error, const char* operation,
    const std::filesystem::path& path) {
    throw std::system_error(error, std::generic_category(),
        std::string(operation) + " failed for " + path.generic_string());
}

[[noreturn]] void throw_last_error(const char* operation,
    const std::filesystem::path& path) {
    throw std::system_error(static_cast<int>(::GetLastError()),
        std::system_category(),
        std::string(operation) + " failed for " + path.generic_string());
}

class file_handle final {
public:
    explicit file_handle(HANDLE h) noexcept : h_(h) {}
    ~file_handle() { ::CloseHandle(h_); }
    file_handle(const file_handle&) = delete;
    file_handle& operator=(const file_handle&) = delete;

    HANDLE get() const noexcept { return h_; }

private:
    HANDLE h_;
};

}

// Windows refuses to delete a file while a view of it is mapped, and the
// store deletes packs that other processes are reading, so the bytes are
// read into memory instead of mapped.
mapped_file::mapped_file(const std::filesystem::path& path) {
    // Backup semantics open a directory too, so it is refused as not
    // regular, as on POSIX, rather than as access denied.
    const HANDLE raw = detail::open_existing(path,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_SEQUENTIAL_SCAN);
    if (raw == INVALID_HANDLE_VALUE)
        throw_last_error("open", path);
    const file_handle file(raw);

    BY_HANDLE_FILE_INFORMATION info{};
    if (!::GetFileInformationByHandle(file.get(), &info))
        throw_last_error("stat", path);
    if ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
        ::GetFileType(file.get()) != FILE_TYPE_DISK)
        throw_error(EINVAL, "map of non-regular file", path);

    const auto size = static_cast<std::size_t>(
        (std::uint64_t{info.nFileSizeHigh} << 32) | info.nFileSizeLow);
    if (size == 0)
        return;

    auto* bytes = new std::byte[size];
    std::size_t done = 0;
    while (done < size) {
        const auto wanted = static_cast<DWORD>(
            std::min<std::size_t>(size - done, 1u << 30));
        DWORD n = 0;
        if (!::ReadFile(file.get(), bytes + done, wanted, &n, nullptr) ||
            n == 0) {
            delete[] bytes;
            throw_last_error("read", path);
        }
        done += n;
    }
    data_ = bytes;
    size_ = size;
}

#else

namespace {

[[noreturn]] void throw_error(int error, const char* operation,
    const std::filesystem::path& path) {
    throw std::system_error(error, std::generic_category(),
        std::string(operation) + " failed for " + path.generic_string());
}

[[noreturn]] void throw_errno(const char* operation,
    const std::filesystem::path& path) {
    throw_error(errno, operation, path);
}

class file_descriptor final {
public:
    explicit file_descriptor(int fd) noexcept : fd_(fd) {}
    ~file_descriptor() { ::close(fd_); }
    file_descriptor(const file_descriptor&) = delete;
    file_descriptor& operator=(const file_descriptor&) = delete;
    int get() const noexcept { return fd_; }

private:
    int fd_;
};

}

mapped_file::mapped_file(const std::filesystem::path& path) {
    const int raw_fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (raw_fd == -1)
        throw_errno("open", path);
    const file_descriptor fd(raw_fd);

    struct stat info {};
    if (::fstat(fd.get(), &info) == -1)
        throw_errno("fstat", path);

    if (!S_ISREG(info.st_mode))
        throw_error(EINVAL, "map of non-regular file", path);

    const auto size = static_cast<std::size_t>(info.st_size);
    if (size == 0)
        return;

    void* address = ::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd.get(), 0);
    if (address == MAP_FAILED)
        throw_errno("mmap", path);

    data_ = static_cast<const std::byte*>(address);
    size_ = size;
}

#endif

mapped_file::~mapped_file() {
    release();
}

mapped_file::mapped_file(mapped_file&& other) noexcept
    : data_(std::exchange(other.data_, nullptr)),
      size_(std::exchange(other.size_, 0)) {}

mapped_file& mapped_file::operator=(mapped_file&& other) noexcept {
    if (this != &other) {
        release();
        data_ = std::exchange(other.data_, nullptr);
        size_ = std::exchange(other.size_, 0);
    }
    return *this;
}

void mapped_file::release() noexcept {
#if defined(_WIN32)
    delete[] data_;
#else
    if (data_ != nullptr)
        ::munmap(const_cast<std::byte*>(data_), size_);
#endif
    data_ = nullptr;
    size_ = 0;
}

}
