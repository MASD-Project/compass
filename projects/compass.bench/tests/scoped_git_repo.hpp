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
#ifndef COMPASS_BENCH_TESTS_SCOPED_GIT_REPO_HPP
#define COMPASS_BENCH_TESTS_SCOPED_GIT_REPO_HPP

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <catch2/catch_test_macros.hpp>
#include "compass.bench/ops/listing.hpp"
#include "compass.bench/types/file_table.hpp"
#include "compass.platform/process/environment.hpp"
#include "compass.platform/process/run_program.hpp"

namespace compass::bench::tests {

/**
 * @brief A git repository and a store in a fresh directory, with git's
 * system and global configuration out of reach.
 */
class scoped_git_repo final {
public:
    scoped_git_repo() {
        static int counter = 0;
        root_ = std::filesystem::temp_directory_path() /
            ("compass.bench.tests." +
                std::to_string(
                    compass::platform::process::current_process_id()) +
                "." + std::to_string(counter++));
        std::filesystem::create_directories(checkout());
        std::filesystem::create_directories(root_ / "home");
        using platform::process::set_environment_variable;
        set_environment_variable("HOME", (root_ / "home").generic_string());
        set_environment_variable("XDG_CONFIG_HOME",
            (root_ / "home" / ".config").generic_string());
        set_environment_variable("GIT_CONFIG_NOSYSTEM", "1");
        git("init -q");
    }
    ~scoped_git_repo() { std::filesystem::remove_all(root_); }
    scoped_git_repo(const scoped_git_repo&) = delete;
    scoped_git_repo& operator=(const scoped_git_repo&) = delete;

    /**
     * @brief Writes a file an hour in the past, so no index written from
     * now on finds it racily clean.
     */
    void write(const std::string& relative, std::string_view content) const {
        const auto path = checkout() / relative;
        std::filesystem::create_directories(path.parent_path());
        {
            std::ofstream os(path, std::ios::binary);
            os.write(content.data(),
                static_cast<std::streamsize>(content.size()));
        }
        std::filesystem::last_write_time(path,
            std::filesystem::file_time_type::clock::now() -
                std::chrono::hours(1));
    }

    void git(const std::string& command) const {
        const auto line = "git -C '" + checkout().generic_string() + "' " +
            command + " >/dev/null 2>&1";
        REQUIRE(compass::platform::process::run_program(
            {{"sh", "-c", line}, {}, {}}).exit_code == 0);
    }

    types::file_table list() const {
        const auto raw = ops::git_ls_files(checkout());
        return ops::build_file_table(checkout(), ops::parse_listing(raw));
    }

    std::filesystem::path checkout() const { return root_ / "checkout"; }
    /**
     * @brief The packs of the store of the checkout, where app::store_objects
     * and the result cache put them.
     */
    std::filesystem::path packs() const {
        return checkout() / ".git" / "compass" / "packs";
    }

private:
    std::filesystem::path root_;
};

}

#endif
