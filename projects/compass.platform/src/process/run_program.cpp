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

#if defined(_WIN32)
#include <string_view>
#include <windows.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;
#endif

namespace compass::platform::process {

#if defined(_WIN32)

namespace {

[[noreturn]] void throw_last_error(const std::string& operation) {
    throw std::system_error(static_cast<int>(::GetLastError()),
        std::system_category(), operation);
}

class handle final {
public:
    handle() noexcept = default;
    explicit handle(HANDLE h) noexcept : h_(h) {}
    ~handle() { reset(); }
    handle(const handle&) = delete;
    handle& operator=(const handle&) = delete;

    HANDLE get() const noexcept { return h_; }
    HANDLE* out() noexcept { return &h_; }
    void reset() noexcept {
        if (h_ != nullptr && h_ != INVALID_HANDLE_VALUE)
            ::CloseHandle(h_);
        h_ = nullptr;
    }

private:
    HANDLE h_ = nullptr;
};

std::wstring widen(std::string_view s) {
    if (s.empty())
        return {};
    const int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(),
        static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0)
        throw_last_error("cannot convert an argument to UTF-16");
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
        out.data(), n);
    return out;
}

/**
 * @brief Appends @p argument to @p line quoted so that the C runtime's
 * parser, which every Windows program uses to split its command line,
 * gives it back unchanged.
 */
void append_quoted(std::wstring& line, const std::wstring& argument) {
    if (!line.empty())
        line += L' ';
    if (!argument.empty() &&
        argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        line += argument;
        return;
    }
    line += L'"';
    std::size_t backslashes = 0;
    for (const wchar_t c : argument) {
        if (c == L'\\') {
            ++backslashes;
            continue;
        }
        if (c == L'"')
            line.append(2 * backslashes + 1, L'\\');
        else
            line.append(backslashes, L'\\');
        backslashes = 0;
        line += c;
    }
    line.append(2 * backslashes, L'\\');
    line += L'"';
}

/**
 * @brief An inheritable copy of the standard handle @p which, or of the
 * null device when the parent has none.
 */
HANDLE inheritable_standard(DWORD which) {
    const HANDLE parent = ::GetStdHandle(which);
    HANDLE copy = nullptr;
    if (parent != nullptr && parent != INVALID_HANDLE_VALUE &&
        ::DuplicateHandle(::GetCurrentProcess(), parent,
            ::GetCurrentProcess(), &copy, 0, TRUE, DUPLICATE_SAME_ACCESS))
        return copy;
    SECURITY_ATTRIBUTES inherit{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    copy = ::CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, 0,
        nullptr);
    if (copy == INVALID_HANDLE_VALUE)
        throw_last_error("cannot open the null device");
    return copy;
}

/**
 * @brief The attribute list that limits what a child inherits to the
 * handles named, so a pipe another thread is spawning a program with does
 * not leak into this child and keep that pipe open.
 */
class inherited_handles final {
public:
    explicit inherited_handles(HANDLE (&handles)[3]) {
        SIZE_T size = 0;
        ::InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
        buffer_.resize(size);
        list_ = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(buffer_.data());
        if (!::InitializeProcThreadAttributeList(list_, 1, 0, &size))
            throw_last_error("cannot list inherited handles");
        // The same handle may stand for two of the three streams, and the
        // list must not name a handle twice.
        std::size_t n = 0;
        for (const HANDLE h : handles) {
            bool seen = false;
            for (std::size_t i = 0; i < n; ++i)
                seen = seen || unique_[i] == h;
            if (!seen)
                unique_[n++] = h;
        }
        if (!::UpdateProcThreadAttribute(list_, 0,
                PROC_THREAD_ATTRIBUTE_HANDLE_LIST, unique_, n * sizeof(HANDLE),
                nullptr, nullptr)) {
            ::DeleteProcThreadAttributeList(list_);
            throw_last_error("cannot list inherited handles");
        }
    }
    ~inherited_handles() { ::DeleteProcThreadAttributeList(list_); }
    inherited_handles(const inherited_handles&) = delete;
    inherited_handles& operator=(const inherited_handles&) = delete;

    LPPROC_THREAD_ATTRIBUTE_LIST get() const noexcept { return list_; }

private:
    std::vector<char> buffer_;
    LPPROC_THREAD_ATTRIBUTE_LIST list_ = nullptr;
    HANDLE unique_[3] = {};
};

}

program_result run_program(const program_call& call) {
    if (call.arguments.empty())
        throw std::invalid_argument("run_program: no program named");
    const auto& program = call.arguments.front();

    std::wstring line;
    for (const auto& a : call.arguments)
        append_quoted(line, widen(a));

    SECURITY_ATTRIBUTES inherit{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    handle read_end;
    handle write_end;
    if (!::CreatePipe(read_end.out(), write_end.out(), &inherit, 0))
        throw_last_error("cannot create a pipe for " + program);
    if (!::SetHandleInformation(read_end.get(), HANDLE_FLAG_INHERIT, 0))
        throw_last_error("cannot create a pipe for " + program);

    handle input;
    if (!call.input.empty()) {
        *input.out() = ::CreateFileW(call.input.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &inherit,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (input.get() == INVALID_HANDLE_VALUE)
            throw_last_error("cannot open " + call.input.generic_string());
    } else {
        *input.out() = inheritable_standard(STD_INPUT_HANDLE);
    }
    const handle error(inheritable_standard(STD_ERROR_HANDLE));

    HANDLE streams[3] = {input.get(), write_end.get(), error.get()};
    const inherited_handles inherited(streams);

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = input.get();
    startup.StartupInfo.hStdOutput = write_end.get();
    startup.StartupInfo.hStdError = error.get();
    startup.lpAttributeList = inherited.get();

    PROCESS_INFORMATION process{};
    const wchar_t* directory =
        call.directory.empty() ? nullptr : call.directory.c_str();
    if (!::CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE,
            EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
            nullptr, directory, &startup.StartupInfo, &process))
        throw_last_error("CreateProcess " + program);
    const handle child(process.hProcess);
    ::CloseHandle(process.hThread);
    write_end.reset();

    program_result r;
    char buffer[1 << 16];
    for (;;) {
        DWORD n = 0;
        if (::ReadFile(read_end.get(), buffer, sizeof buffer, &n, nullptr)) {
            if (n == 0)
                break;
            r.output.append(buffer, n);
            continue;
        }
        const auto error_code = ::GetLastError();
        if (error_code == ERROR_BROKEN_PIPE)
            break;
        ::WaitForSingleObject(child.get(), INFINITE);
        ::SetLastError(error_code);
        throw_last_error("read from " + program);
    }

    DWORD exit_code = 0;
    if (::WaitForSingleObject(child.get(), INFINITE) == WAIT_FAILED ||
        !::GetExitCodeProcess(child.get(), &exit_code))
        throw_last_error("wait for " + program);
    r.exit_code = static_cast<int>(exit_code);
    return r;
}

#else

namespace {

[[noreturn]] void throw_errno(int error, const std::string& operation) {
    throw std::system_error(error, std::generic_category(), operation);
}

class pipe_ends final {
public:
    pipe_ends() {
#if defined(__APPLE__)
        // macOS has no pipe2, so a program another thread starts between
        // these calls may inherit the pipe, which only delays end of file.
        if (::pipe(fds_) == -1)
            throw_errno(errno, "pipe");
        for (const int fd : fds_) {
            if (::fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {
                const int error = errno;
                close_read();
                close_write();
                throw_errno(error, "fcntl");
            }
        }
#else
        if (::pipe2(fds_, O_CLOEXEC) == -1)
            throw_errno(errno, "pipe2");
#endif
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

#endif

}
