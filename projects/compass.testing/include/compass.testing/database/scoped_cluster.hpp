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
#ifndef COMPASS_TESTING_DATABASE_SCOPED_CLUSTER_HPP
#define COMPASS_TESTING_DATABASE_SCOPED_CLUSTER_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "compass.testing/filesystem/scoped_directory.hpp"

namespace compass::testing::database {

/**
 * @brief The directory of the PostgreSQL server programs, initdb and
 * pg_ctl: COMPASS_TEST_POSTGRES_BIN when set, else the directory of initdb
 * on PATH, else the newest /usr/lib/postgresql/<version>/bin, where Debian
 * and Ubuntu put them. Nothing when none holds initdb.
 */
[[nodiscard]] std::optional<std::filesystem::path> server_programs();

/**
 * @brief A throwaway PostgreSQL cluster: made by initdb in a new
 * scoped_directory, started by pg_ctl on a Unix socket in that directory
 * with no TCP port, so it never meets a server of the machine, and on
 * destruction stopped and removed with the directory. Its superuser is
 * compass and every local connection is trusted.
 *
 * Run as root, as in a CI container, the server programs run as the
 * postgres user, since initdb refuses root.
 */
class scoped_cluster final {
public:
    /**
     * @brief Makes and starts the cluster with the programs in @p programs.
     * @p prefix names the suite, as for scoped_directory.
     * @throws std::runtime_error with the programs' output when the
     * cluster cannot be made or started.
     */
    scoped_cluster(std::string_view prefix,
        const std::filesystem::path& programs);
    ~scoped_cluster();
    scoped_cluster(const scoped_cluster&) = delete;
    scoped_cluster& operator=(const scoped_cluster&) = delete;

    /**
     * @brief Creates a new, empty database and returns its libpq
     * connection string.
     * @throws std::runtime_error when createdb fails.
     */
    std::string create_database();

    /**
     * @brief The libpq connection string of @p database in the cluster.
     */
    [[nodiscard]] std::string connection_string(
        std::string_view database) const;

    /**
     * @brief The directory the cluster lives in, its socket included.
     */
    [[nodiscard]] const std::filesystem::path& directory() const noexcept {
        return directory_.path();
    }

private:
    void run(const std::string& program, std::vector<std::string> arguments,
        bool as_server) const;

    filesystem::scoped_directory directory_;
    std::filesystem::path programs_;
    std::filesystem::path data_;
    bool as_postgres_ = false;
    bool started_ = false;
    unsigned databases_ = 0;
};

/**
 * @brief The one cluster of the test process, started on first use and
 * stopped at exit, or nothing when there are no server programs; @p why
 * then says so, for the test to skip with.
 */
[[nodiscard]] scoped_cluster* shared_cluster(std::string_view prefix,
    std::string& why);

}

#endif
