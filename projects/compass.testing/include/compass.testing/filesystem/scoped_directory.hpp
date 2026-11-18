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
#ifndef COMPASS_TESTING_FILESYSTEM_SCOPED_DIRECTORY_HPP
#define COMPASS_TESTING_FILESYSTEM_SCOPED_DIRECTORY_HPP

#include <filesystem>
#include <string_view>

namespace compass::testing::filesystem {

/**
 * @brief A new, empty directory under the system temporary directory, named
 * <prefix>.<process ID>.<number>, removed with everything in it on
 * destruction.
 *
 * Constructing one first removes every <prefix>.<process ID>.<number> whose
 * process is no longer running, so a test run that is killed or crashes is
 * cleaned up by the next run of the same suite. A fixture holds it as its
 * first member, so a throw later in the fixture's constructor still
 * removes it.
 */
class scoped_directory final {
public:
    /**
     * @brief Creates the directory. @p prefix names the suite, such as
     * compass.store.tests, so a directory left behind says whose it is.
     */
    explicit scoped_directory(std::string_view prefix);
    ~scoped_directory();
    scoped_directory(const scoped_directory&) = delete;
    scoped_directory& operator=(const scoped_directory&) = delete;

    /**
     * @brief The directory's absolute path, by its long name on Windows.
     */
    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

}

#endif
