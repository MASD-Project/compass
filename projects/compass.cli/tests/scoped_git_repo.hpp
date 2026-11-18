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
#ifndef COMPASS_CLI_TESTS_SCOPED_GIT_REPO_HPP
#define COMPASS_CLI_TESTS_SCOPED_GIT_REPO_HPP

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <catch2/catch_test_macros.hpp>
#include "compass.platform/process/environment.hpp"
#include "compass.platform/process/run_program.hpp"

namespace compass::cli::tests {

/**
 * @brief A git repository in a fresh directory, with git's system and
 * global configuration out of reach, holding a task contract, a complete
 * task, a task with no Result section and a document with no type, all
 * added to the index.
 */
class scoped_git_repo final {
public:
    scoped_git_repo() {
        static int counter = 0;
        root_ = std::filesystem::temp_directory_path() /
            ("compass.cli.tests." +
                std::to_string(
                    compass::platform::process::current_process_id()) +
                "." + std::to_string(counter++));
        std::filesystem::create_directories(checkout());
        std::filesystem::create_directories(root_ / "home");
        using platform::process::set_environment_variable;
        set_environment_variable("HOME", (root_ / "home").string());
        set_environment_variable("XDG_CONFIG_HOME",
            (root_ / "home" / ".config").string());
        set_environment_variable("GIT_CONFIG_NOSYSTEM", "1");
        git("init -q");
        write("doc/meta/document_type_task.org",
            "#+title: Document type: task\n#+type: meta\n\n* Contract\n\n"
            "- Folder :: =task.org=\n- Level :: =s1=\n"
            "- TODO vocabulary :: =BACKLOG | DONE=\n"
            "- Required sections ::\n  - =* Goal= — one paragraph.\n"
            "  - =* Result= — filled at the end.\n");
        write("doc/task_a.org",
            ":PROPERTIES:\n:ID: A-1\n:END:\n#+title: Task: a\n#+type: task\n"
            "* Goal\n* Result\n");
        write("doc/task_b.org", "#+title: Task: b\n#+type: task\n* Goal\n");
        write("readme.org", "Plain text.\n");
        git("add -A");
    }
    ~scoped_git_repo() { std::filesystem::remove_all(root_); }
    scoped_git_repo(const scoped_git_repo&) = delete;
    scoped_git_repo& operator=(const scoped_git_repo&) = delete;

    /**
     * @brief Writes a file under the checkout an hour in the past, so no
     * index written from now on finds it racily clean.
     */
    void write(const std::string& relative, std::string_view content) const {
        write_file(checkout() / relative, content);
    }

    static void write_file(const std::filesystem::path& path,
        std::string_view content) {
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

    static std::string read_file(const std::filesystem::path& path) {
        std::ifstream is(path, std::ios::binary);
        std::ostringstream os;
        os << is.rdbuf();
        return os.str();
    }

    void git(const std::string& command) const {
        const auto line = "git -C '" + checkout().string() + "' " + command +
            " >/dev/null 2>&1";
        REQUIRE(compass::platform::process::run_program(
            {{"sh", "-c", line}, {}, {}}).exit_code == 0);
    }

    std::filesystem::path checkout() const { return root_ / "checkout"; }
    std::filesystem::path output_root() const { return root_ / "out"; }

private:
    std::filesystem::path root_;
};

}

#endif
