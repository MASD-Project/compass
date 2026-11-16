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
#include "compass.cli/io/listing.hpp"

#include <cerrno>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <fcntl.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace compass::cli::io {

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

/**
 * @brief Runs git in @p checkout with @p args and returns its standard
 * output.
 */
std::string run_git(const std::filesystem::path& checkout,
    std::vector<std::string> args) {
    pipe_ends pipe;

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, pipe.write_end(), STDOUT_FILENO);

    const std::string dir = checkout.string();
    args.insert(args.begin(), {"git", "-C", dir});
    std::vector<char*> argv;
    for (auto& arg : args)
        argv.push_back(arg.data());
    argv.push_back(nullptr);

    pid_t pid = 0;
    const int spawn_error =
        ::posix_spawnp(&pid, "git", &actions, nullptr, argv.data(), environ);
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
        throw std::runtime_error("git " + args[3] + " failed for " + dir);
    return output;
}

bool is_plain_file(const std::filesystem::path& path) {
    struct stat st {};
    return ::lstat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

}

std::vector<std::string> list_org_files(
    const std::filesystem::path& checkout) {
    const auto raw = run_git(checkout,
        {"ls-files", "-z", "--cached", "--others", "--exclude-standard"});
    std::vector<std::string> out;
    std::string_view rest = raw;
    while (!rest.empty()) {
        const auto end = rest.find('\0');
        const auto path = rest.substr(0, end);
        rest = end == std::string_view::npos ? std::string_view{}
                                             : rest.substr(end + 1);
        // git lists a path with conflicts once per stage, one after another.
        if (!out.empty() && out.back() == path)
            continue;
        if (path.ends_with(".org") && is_plain_file(checkout / path))
            out.emplace_back(path);
    }
    return out;
}

}
