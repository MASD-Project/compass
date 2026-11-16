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
#include <string>
#include <system_error>
#include <utility>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace compass::platform::filesystem {

namespace {

[[noreturn]] void throw_error(int error, const char* operation,
    const std::filesystem::path& path) {
    throw std::system_error(error, std::generic_category(),
        std::string(operation) + " failed for " + path.string());
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
    if (data_ != nullptr)
        ::munmap(const_cast<std::byte*>(data_), size_);
    data_ = nullptr;
    size_ = 0;
}

}
