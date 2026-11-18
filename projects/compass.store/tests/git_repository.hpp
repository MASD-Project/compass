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
#ifndef COMPASS_STORE_TESTS_GIT_REPOSITORY_HPP
#define COMPASS_STORE_TESTS_GIT_REPOSITORY_HPP

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <catch2/catch_test_macros.hpp>
#include "compass.platform/process/environment.hpp"
#include "compass.platform/process/run_program.hpp"
#include "scoped_directory.hpp"

namespace compass::store::tests {

/**
 * @brief A git repository in a fresh directory, with git's system and
 * global configuration out of reach of both git and the code under test.
 */
class git_repository final {
public:
    git_repository() {
        const auto home = dir_.root() / "home";
        std::filesystem::create_directories(home);
        using platform::process::set_environment_variable;
        set_environment_variable("HOME", home.string());
        set_environment_variable("XDG_CONFIG_HOME",
            (home / ".config").string());
        set_environment_variable("GIT_CONFIG_NOSYSTEM", "1");
        std::filesystem::create_directories(root());
        git("init -q");
    }

    [[nodiscard]] std::filesystem::path root() const {
        return dir_.root() / "repo";
    }

    void write(const std::string& path, std::string_view content) const {
        dir_.write("repo/" + path, content);
    }

    /**
     * @brief Moves the modification time of @p path by @p shift from now,
     * an hour back by default, so the index git writes next is later than
     * the file and the entry is not racily clean.
     */
    void age(const std::string& path,
        std::chrono::seconds shift = std::chrono::hours(-1)) const {
        std::filesystem::last_write_time(root() / path,
            std::filesystem::file_time_type::clock::now() + shift);
    }

    /**
     * @brief Runs git in the repository and returns its standard output
     * with the trailing newline removed.
     */
    std::string git(const std::string& command) const {
        const auto line = "git -C '" + root().string() +
            "' -c user.name=t -c user.email=t@example.org"
            " -c commit.gpgsign=false " + command + " 2>/dev/null";
        auto r = platform::process::run_program({{"sh", "-c", line}, {}, {}});
        auto out = std::move(r.output);
        last_status_ = r.exit_code;
        while (!out.empty() && out.back() == '\n')
            out.pop_back();
        return out;
    }

    [[nodiscard]] int last_status() const noexcept { return last_status_; }

    /**
     * @brief The ID `git hash-object --no-filters` gives the bytes of
     * @p path, which is the ID of the file as it is on disk.
     */
    [[nodiscard]] std::string hash_object(const std::string& path) const {
        return git("hash-object --no-filters -- '" + path + "'");
    }

    [[nodiscard]] std::filesystem::path index_file() const {
        return root() / ".git" / "index";
    }

private:
    scoped_directory dir_;
    mutable int last_status_ = 0;
};

}

#endif
