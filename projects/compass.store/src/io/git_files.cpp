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
#include "compass.store/io/git_files.hpp"

#include <cerrno>
#include <cstddef>
#include <string>
#include <system_error>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "compass.store/ops/blob_hash.hpp"
#include "compass.store/ops/git_index.hpp"

namespace compass::store::io {

namespace {

[[noreturn]] void throw_errno(const char* operation,
    const std::filesystem::path& file) {
    throw std::system_error(errno, std::generic_category(),
        std::string(operation) + " " + file.string());
}

/**
 * @brief A file opened read-only, with the stat data of what was opened.
 */
class read_only_file final {
public:
    explicit read_only_file(const std::filesystem::path& file) : file_(file) {
        fd_ = ::open(file.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd_ == -1)
            throw_errno("open", file);
        if (::fstat(fd_, &stat_) == -1) {
            ::close(fd_);
            throw_errno("fstat", file);
        }
    }
    ~read_only_file() { ::close(fd_); }
    read_only_file(const read_only_file&) = delete;
    read_only_file& operator=(const read_only_file&) = delete;

    [[nodiscard]] const struct stat& status() const noexcept { return stat_; }

    void read_into(std::string& bytes) const {
        bytes.resize(static_cast<std::size_t>(stat_.st_size));
        std::size_t done = 0;
        for (;;) {
            if (done == bytes.size())
                bytes.resize(bytes.size() + 4096);
            const auto n = ::read(fd_, bytes.data() + done, bytes.size() - done);
            if (n == 0)
                break;
            if (n > 0)
                done += static_cast<std::size_t>(n);
            else if (errno != EINTR)
                throw_errno("read", file_);
        }
        bytes.resize(done);
    }

private:
    const std::filesystem::path& file_;
    int fd_ = -1;
    struct stat stat_{};
};

types::index_time to_time(const struct timespec& t) noexcept {
    return {static_cast<std::uint32_t>(t.tv_sec),
        static_cast<std::uint32_t>(t.tv_nsec)};
}

}

types::git_index read_git_index(const std::filesystem::path& file) {
    const read_only_file f(file);
    std::string bytes;
    f.read_into(bytes);
    auto index = ops::parse_git_index(bytes);
    index.written = to_time(f.status().st_mtim);
    return index;
}

std::optional<types::stat_data> stat_of(const std::filesystem::path& file) {
    struct stat st{};
    if (::lstat(file.c_str(), &st) == -1 || !S_ISREG(st.st_mode))
        return std::nullopt;
    types::stat_data s;
    s.ctime = to_time(st.st_ctim);
    s.mtime = to_time(st.st_mtim);
    s.dev = static_cast<std::uint32_t>(st.st_dev);
    s.ino = static_cast<std::uint32_t>(st.st_ino);
    s.uid = static_cast<std::uint32_t>(st.st_uid);
    s.gid = static_cast<std::uint32_t>(st.st_gid);
    s.size = static_cast<std::uint32_t>(st.st_size);
    return s;
}

types::blob_id hash_file(const std::filesystem::path& file) {
    thread_local std::string bytes;
    const read_only_file f(file);
    f.read_into(bytes);
    return ops::blob_id_of(bytes);
}

}
