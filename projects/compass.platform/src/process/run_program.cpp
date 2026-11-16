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
#include "compass.platform/process/run_program.hpp"

#include <cerrno>
#include <stdexcept>
#include <system_error>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace compass::platform::process {

namespace {

[[noreturn]] void throw_errno(int error, const std::string& operation) {
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

class file_actions final {
public:
    file_actions() { posix_spawn_file_actions_init(&actions_); }
    ~file_actions() { posix_spawn_file_actions_destroy(&actions_); }
    file_actions(const file_actions&) = delete;
    file_actions& operator=(const file_actions&) = delete;

    posix_spawn_file_actions_t* get() noexcept { return &actions_; }

private:
    posix_spawn_file_actions_t actions_;
};

int wait_for(pid_t pid, const std::string& program) {
    int status = 0;
    while (::waitpid(pid, &status, 0) == -1) {
        if (errno != EINTR)
            throw_errno(errno, "waitpid " + program);
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

}

program_result run_program(const program_call& call) {
    if (call.arguments.empty())
        throw std::invalid_argument("run_program: no program named");
    const auto& program = call.arguments.front();
    pipe_ends pipe;
    file_actions actions;
    posix_spawn_file_actions_adddup2(actions.get(), pipe.write_end(),
        STDOUT_FILENO);
    if (!call.input.empty())
        posix_spawn_file_actions_addopen(actions.get(), STDIN_FILENO,
            call.input.c_str(), O_RDONLY, 0);
    if (!call.directory.empty())
        posix_spawn_file_actions_addchdir_np(actions.get(),
            call.directory.c_str());

    auto arguments = call.arguments;
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1);
    for (auto& a : arguments)
        argv.push_back(a.data());
    argv.push_back(nullptr);

    pid_t pid = 0;
    const int spawn_error = ::posix_spawnp(&pid, program.c_str(),
        actions.get(), nullptr, argv.data(), environ);
    if (spawn_error != 0)
        throw_errno(spawn_error, "posix_spawnp " + program);
    pipe.close_write();

    program_result r;
    char buffer[1 << 16];
    for (;;) {
        const ssize_t n = ::read(pipe.read_end(), buffer, sizeof buffer);
        if (n > 0) {
            r.output.append(buffer, static_cast<std::size_t>(n));
        } else if (n == 0) {
            break;
        } else if (errno != EINTR) {
            const int error = errno;
            ::waitpid(pid, nullptr, 0);
            throw_errno(error, "read from " + program);
        }
    }
    r.exit_code = wait_for(pid, program);
    return r;
}

}
