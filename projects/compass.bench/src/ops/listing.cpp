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
#include "compass.bench/ops/listing.hpp"

#include <cerrno>
#include <stdexcept>
#include <string>
#include <system_error>
#include <fcntl.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace compass::bench::ops {

namespace {

[[noreturn]] void throw_errno(int error, const char* operation) {
    throw std::system_error(error, std::generic_category(), operation);
}

class pipe_ends final {
public:
    pipe_ends() {
        if (::pipe2(fds_, O_CLOEXEC) == -1)
            throw_errno(errno, "pipe2");
    }
    ~pipe_ends() {
        close_read();
        close_write();
    }
    pipe_ends(const pipe_ends&) = delete;
    pipe_ends& operator=(const pipe_ends&) = delete;

    int read_end() const noexcept { return fds_[0]; }
    int write_end() const noexcept { return fds_[1]; }
    void close_read() noexcept { close_one(fds_[0]); }
    void close_write() noexcept { close_one(fds_[1]); }

private:
    static void close_one(int& fd) noexcept {
        if (fd != -1)
            ::close(fd);
        fd = -1;
    }

    int fds_[2] = {-1, -1};
};

}

std::string git_ls_files(const std::filesystem::path& checkout) {
    pipe_ends pipe;

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, pipe.write_end(), STDOUT_FILENO);

    std::string dir = checkout.string();
    std::string arg0 = "git", arg1 = "-C", arg3 = "ls-files", arg4 = "-z";
    char* argv[] = {arg0.data(), arg1.data(), dir.data(), arg3.data(),
        arg4.data(), nullptr};

    pid_t pid = 0;
    const int spawn_error =
        ::posix_spawnp(&pid, "git", &actions, nullptr, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    if (spawn_error != 0)
        throw_errno(spawn_error, "posix_spawnp git");
    pipe.close_write();

    std::string output;
    char buffer[1 << 16];
    for (;;) {
        const ssize_t n = ::read(pipe.read_end(), buffer, sizeof buffer);
        if (n > 0) {
            output.append(buffer, static_cast<std::size_t>(n));
        } else if (n == 0) {
            break;
        } else if (errno != EINTR) {
            const int error = errno;
            ::waitpid(pid, nullptr, 0);
            throw_errno(error, "read from git");
        }
    }

    int status = 0;
    while (::waitpid(pid, &status, 0) == -1) {
        if (errno != EINTR)
            throw_errno(errno, "waitpid git");
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        throw std::runtime_error("git ls-files failed for " + dir);
    return output;
}

std::vector<std::string_view> parse_listing(std::string_view raw) {
    std::vector<std::string_view> r;
    while (!raw.empty()) {
        const auto nul = raw.find('\0');
        r.push_back(raw.substr(0, nul));
        if (nul == std::string_view::npos)
            break;
        raw.remove_prefix(nul + 1);
    }
    return r;
}

std::string_view extension_of(std::string_view path) {
    const auto slash = path.rfind('/');
    const auto name =
        slash == std::string_view::npos ? path : path.substr(slash + 1);
    const auto dot = name.rfind('.');
    if (dot == std::string_view::npos || dot == 0)
        return {};
    return name.substr(dot);
}

void add_file(types::file_table& table, std::string_view path,
    std::uint64_t size) {
    const auto extension = extension_of(path);
    auto it = table.extension_index.find(extension);
    if (it == table.extension_index.end()) {
        const auto id = static_cast<std::uint32_t>(table.extensions.size());
        table.extensions.emplace_back(extension);
        it = table.extension_index.emplace(table.extensions.back(), id).first;
    }
    table.paths.emplace_back(path);
    table.sizes.push_back(size);
    table.extension_ids.push_back(it->second);
}

types::file_table build_file_table(const std::filesystem::path& checkout,
    std::span<const std::string_view> entries) {
    types::file_table table;
    table.paths.reserve(entries.size());
    table.sizes.reserve(entries.size());
    table.extension_ids.reserve(entries.size());

    std::string full = checkout.string() + "/";
    const auto prefix = full.size();
    for (const auto entry : entries) {
        full.resize(prefix);
        full.append(entry);
        struct stat info {};
        if (::lstat(full.c_str(), &info) == -1) {
            add_file(table, entry, 0);
            continue;
        }
        if (!S_ISREG(info.st_mode))
            continue;
        add_file(table, entry, static_cast<std::uint64_t>(info.st_size));
    }
    return table;
}

}
