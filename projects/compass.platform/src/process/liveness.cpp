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
#include "compass.platform/process/liveness.hpp"

#if defined(_WIN32)
#include <windows.h>
#else
#include <cerrno>
#include <limits>
#include <signal.h>
#include <sys/types.h>
#endif

namespace compass::platform::process {

bool is_running(std::uint64_t process_id) noexcept {
#if defined(_WIN32)
    if (process_id == 0 || process_id > MAXDWORD)
        return false;
    const HANDLE process = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
        FALSE, static_cast<DWORD>(process_id));
    if (process == nullptr)
        return ::GetLastError() == ERROR_ACCESS_DENIED;
    // A process that has exited still opens while any handle to it is open.
    DWORD code = 0;
    const bool running =
        ::GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
    ::CloseHandle(process);
    return running;
#else
    // kill takes 0 and negative IDs as process groups.
    if (process_id == 0 || process_id >
        static_cast<std::uint64_t>(std::numeric_limits<pid_t>::max()))
        return false;
    return ::kill(static_cast<pid_t>(process_id), 0) == 0 || errno == EPERM;
#endif
}

}
