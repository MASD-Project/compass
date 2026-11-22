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
#include "compass.testing/database/scoped_cluster.hpp"

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include "compass.platform/process/environment.hpp"
#include "compass.platform/process/executable.hpp"
#include "compass.platform/process/run_program.hpp"

#ifndef _WIN32
#include <pwd.h>
#include <unistd.h>
#endif

namespace compass::testing::database {

namespace {

namespace fs = std::filesystem;
namespace process = compass::platform::process;

bool holds_initdb(const fs::path& dir) {
    std::error_code ec;
    return fs::is_regular_file(dir / "initdb", ec) ||
        fs::is_regular_file(dir / "initdb.exe", ec);
}

/**
 * @brief The newest /usr/lib/postgresql/<version>/bin that holds initdb.
 */
std::optional<fs::path> debian_programs() {
    std::optional<fs::path> best;
    unsigned best_version = 0;
    std::error_code ec;
    for (fs::directory_iterator it("/usr/lib/postgresql", ec), end;
         !ec && it != end; it.increment(ec)) {
        const auto name = it->path().filename().string();
        unsigned version = 0;
        const auto [p, e] = std::from_chars(name.data(),
            name.data() + name.size(), version);
        if (e != std::errc{} || p != name.data() + name.size())
            continue;
        const auto bin = it->path() / "bin";
        if (version > best_version && holds_initdb(bin)) {
            best = bin;
            best_version = version;
        }
    }
    return best;
}

/**
 * @brief Whether this process runs as root, and the postgres user exists
 * to run the server as; on Windows, never.
 */
bool needs_postgres_user() {
#ifdef _WIN32
    return false;
#else
    return geteuid() == 0;
#endif
}

void hand_to_postgres(const fs::path& dir) {
#ifndef _WIN32
    const passwd* pw = getpwnam("postgres");
    if (pw == nullptr)
        throw std::runtime_error("running as root, and there is no postgres "
            "user to run the PostgreSQL server as");
    if (chown(dir.c_str(), pw->pw_uid, pw->pw_gid) != 0)
        throw std::system_error(errno, std::generic_category(),
            "cannot give " + dir.string() + " to the postgres user");
#else
    static_cast<void>(dir);
#endif
}

}

std::optional<fs::path> server_programs() {
    if (const auto given = process::environment_variable(
            "COMPASS_TEST_POSTGRES_BIN");
        given && holds_initdb(*given))
        return fs::path(*given);
    if (const auto initdb = process::find_program("initdb"))
        return initdb->parent_path();
    return debian_programs();
}

scoped_cluster::scoped_cluster(std::string_view prefix,
    const fs::path& programs)
    : directory_(prefix), programs_(programs),
      data_(directory_.path() / "data"),
      as_postgres_(needs_postgres_user()) {
    if (as_postgres_)
        hand_to_postgres(directory_.path());
    run("initdb", {"-D", data_.string(), "-U", "compass", "-A", "trust",
        "-E", "UTF8", "--locale=C", "-N"}, true);
    {
        std::ofstream conf(data_ / "postgresql.conf", std::ios::app);
        conf << "\nlisten_addresses = ''\n"
             << "unix_socket_directories = '" << directory_.path().string()
             << "'\n"
             << "fsync = off\n"
             << "synchronous_commit = off\n"
             << "full_page_writes = off\n";
        if (!conf)
            throw std::runtime_error("cannot write postgresql.conf");
    }
    run("pg_ctl", {"-D", data_.string(), "-l",
        (directory_.path() / "server.log").string(), "-w", "-s", "start"},
        true);
    started_ = true;
}

scoped_cluster::~scoped_cluster() {
    if (!started_)
        return;
    try {
        run("pg_ctl", {"-D", data_.string(), "-m", "immediate", "-w", "-s",
            "stop"}, true);
    } catch (const std::exception&) {
        // A server that does not stop finds its directory gone and stops.
    }
}

void scoped_cluster::run(const std::string& program,
    std::vector<std::string> arguments, bool as_server) const {
    process::program_call call;
    if (as_server && as_postgres_)
        call.arguments = {"runuser", "-u", "postgres", "--"};
    call.arguments.push_back((programs_ / program).string());
    std::ranges::move(arguments, std::back_inserter(call.arguments));
    call.directory = directory_.path();
    call.errors = process::error_stream::merge;
    const auto r = process::run_program(call);
    if (r.exit_code != 0) {
        std::string log;
        if (std::ifstream in(directory_.path() / "server.log"); in)
            log.assign(std::istreambuf_iterator<char>(in), {});
        throw std::runtime_error(program + " failed with exit code " +
            std::to_string(r.exit_code) + ":\n" + r.output + log);
    }
}

std::string scoped_cluster::create_database() {
    const auto name = "test_" + std::to_string(++databases_);
    run("createdb", {"-h", directory_.path().string(), "-U", "compass",
        name}, false);
    return connection_string(name);
}

std::string scoped_cluster::connection_string(
    std::string_view database) const {
    return "host='" + directory_.path().string() + "' user=compass dbname=" +
        std::string(database);
}

scoped_cluster* shared_cluster(std::string_view prefix, std::string& why) {
    static std::once_flag once;
    static std::unique_ptr<scoped_cluster> cluster;
    static std::string reason;
    std::call_once(once, [&] {
        const auto programs = server_programs();
        if (!programs) {
            reason = "no PostgreSQL server programs: initdb is not on PATH "
                "nor in /usr/lib/postgresql/<version>/bin, and "
                "COMPASS_TEST_POSTGRES_BIN is not set";
            return;
        }
        cluster = std::make_unique<scoped_cluster>(prefix, *programs);
    });
    why = reason;
    return cluster.get();
}

}
