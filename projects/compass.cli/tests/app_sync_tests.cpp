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
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.cli/app/application.hpp"
#include "compass.cli/config/local_settings.hpp"
#include "compass.db/io/connection.hpp"
#include "compass.testing/database/scoped_cluster.hpp"
#include "scoped_git_repo.hpp"

namespace {

using namespace compass::cli;
using tests::scoped_git_repo;
using Catch::Matchers::ContainsSubstring;

struct outcome {
    int status = -1;
    std::string out;
    std::string err;
};

outcome run(std::vector<std::string> args, std::string_view input = {}) {
    const std::vector<std::string_view> views(args.begin(), args.end());
    std::istringstream in{std::string(input)};
    std::ostringstream out;
    std::ostringstream err;
    const auto status = app::run(views, "/", in, out, err);
    return {status, out.str(), err.str()};
}

std::string fresh_database() {
    std::string why;
    auto* cluster = compass::testing::database::shared_cluster(
        "compass.cli.tests", why);
    if (cluster == nullptr)
        SKIP(why);
    return cluster->create_database();
}

/**
 * @brief A repository whose compass database is a fresh one of the suite's
 * cluster, initialised by compass db init.
 */
struct synced_repo {
    scoped_git_repo repo;
    std::string database = fresh_database();

    synced_repo() {
        const auto r = run({"db", "init", "-C", checkout(), "--database",
            database});
        REQUIRE(r.status == 0);
    }

    [[nodiscard]] std::string checkout() const {
        return repo.checkout().generic_string();
    }

    outcome command(std::vector<std::string> args) const {
        args.insert(args.begin() + 1, {"-C", checkout(), "-j", "2"});
        return run(std::move(args));
    }

    [[nodiscard]] std::string query(const std::string& sql) const {
        compass::db::io::connection c(database);
        return std::string(c.exec(sql).value(0, 0));
    }

    [[nodiscard]] std::string placed(const std::string& environment) const {
        return query("SELECT count(*) FROM corpus.placement p JOIN "
            "corpus.environment e ON e.id = p.environment WHERE e.name = '" +
            environment + "'");
    }
};

std::string payload(const std::string& file) {
    return "{\"session_id\": \"s\", \"transcript_path\": \"/t.jsonl\", "
           "\"cwd\": \"/\", \"permission_mode\": \"default\", "
           "\"hook_event_name\": \"PostToolUse\", \"tool_name\": \"Edit\", "
           "\"tool_input\": {\"file_path\": \"" + file + "\", "
           "\"old_string\": \"a\", \"new_string\": \"b\"}, "
           "\"tool_response\": {\"filePath\": \"" + file + "\", \"type\": "
           "\"update\"}, \"tool_use_id\": \"toolu_1\"}";
}

}

TEST_CASE("with no database configured, sync says how to configure one and "
    "check says nothing of it", "[app][sync]") {
    const scoped_git_repo repo;
    const auto sync = run({"sync", "-C", repo.checkout().generic_string()});
    CHECK(sync.status == app::exit_failure);
    CHECK_THAT(sync.err, ContainsSubstring("compass db init --database"));
    const auto check = run({"check", "-C", repo.checkout().generic_string(),
        "--no-blame"});
    CHECK_THAT(check.err, !ContainsSubstring("database"));
}

TEST_CASE("db init records the connection in the repository's local "
    "settings", "[app][sync]") {
    const synced_repo s;
    const auto file = config::repository_settings_path(s.repo.checkout());
    CHECK(file == s.repo.checkout() / ".git" / "compass" / "local.org");
    const auto settings = config::read_local_settings(s.repo.checkout());
    REQUIRE(settings);
    CHECK(settings->database == s.database);
    const auto status = s.command({"db", "status"});
    CHECK(status.status == 0);
    CHECK_THAT(status.out, ContainsSubstring("schema: version 1 of 1"));
    CHECK_THAT(status.out, ContainsSubstring("text search: english"));
}

TEST_CASE("sync places every document of the worktree", "[app][sync]") {
    const synced_repo s;
    const auto r = s.command({"sync"});
    CHECK(r.status == 0);
    CHECK_THAT(r.out, ContainsSubstring("synced checkout: 4 documents"));
    CHECK(s.placed("checkout") == "4");
    CHECK(s.query("SELECT count(*) FROM corpus.document") == "4");

    const auto again = s.command({"sync"});
    CHECK_THAT(again.out, ContainsSubstring("0 new to the corpus"));
    CHECK_THAT(again.out, ContainsSubstring("0 added, 0 changed, 0 removed"));
}

TEST_CASE("sync names the environment by the local settings or the option",
    "[app][sync]") {
    const synced_repo s;
    config::write_local_setting(
        config::worktree_settings_path(s.repo.checkout()), "environment",
        "bright_faraday");
    s.command({"sync"});
    CHECK(s.placed("bright_faraday") == "4");
    s.command({"sync", "--environment", "other"});
    CHECK(s.placed("other") == "4");
    CHECK(s.query("SELECT count(*) FROM corpus.document") == "4");
}

TEST_CASE("sync --commit syncs a release from git's objects", "[app][sync]") {
    const synced_repo s;
    s.repo.git("-c user.name=t -c user.email=t@t commit -q -m base");
    s.repo.write("doc/task_c.org", "#+title: Task: c\n#+type: task\n");
    const auto r = s.command({"sync", "--commit", "HEAD", "--environment",
        "v1"});
    CHECK(r.status == 0);
    CHECK(s.placed("v1") == "4");
    CHECK(s.query("SELECT kind FROM corpus.environment WHERE name = 'v1'") ==
        "release");
}

TEST_CASE("check and generate sync the worktree when a database is "
    "configured", "[app][sync]") {
    const synced_repo s;
    s.repo.write("doc/task_c.org", "#+title: Task: c\n#+type: task\n");
    s.repo.git("add doc/task_c.org");
    s.command({"check", "--no-blame"});
    CHECK(s.placed("checkout") == "5");
    std::filesystem::remove(s.repo.checkout() / "doc/task_c.org");
    s.repo.git("add -A");
    s.command({"generate"});
    CHECK(s.placed("checkout") == "4");
}

TEST_CASE("the edit hook syncs the one file it names", "[app][sync]") {
    const synced_repo s;
    s.command({"sync"});
    const auto before = s.query("SELECT count(*) FROM corpus.document");
    s.repo.write("doc/task_b.org",
        "#+title: Task: b\n#+type: task\n* Goal\nEdited.\n");
    run({"hook", "-j", "2"},
        payload((s.repo.checkout() / "doc/task_b.org").generic_string()));
    CHECK(s.placed("checkout") == "4");
    CHECK(s.query("SELECT count(*) FROM corpus.document") ==
        std::to_string(std::stoi(before) + 1));
    CHECK(s.query("SELECT count(*) FROM corpus.section WHERE text = "
        "'Edited.'") == "1");
}

TEST_CASE("db prune drops an environment and collects its documents",
    "[app][sync]") {
    const synced_repo s;
    s.command({"sync"});
    s.repo.write("doc/task_b.org", "#+title: Task: b, other\n#+type: task\n");
    s.command({"sync", "--environment", "other"});
    CHECK(s.query("SELECT count(*) FROM corpus.document") == "5");
    const auto r = s.command({"db", "prune", "--environment", "other"});
    CHECK(r.status == 0);
    CHECK_THAT(r.out, ContainsSubstring("1 documents removed, 4 left"));
    CHECK(s.query("SELECT count(*) FROM corpus.environment") == "1");
}

TEST_CASE("db hooks install writes the git hooks and leaves another's alone",
    "[app][sync]") {
    const scoped_git_repo repo;
    const auto hooks = repo.checkout() / ".git" / "hooks";
    scoped_git_repo::write_file(hooks / "post-merge", "#!/bin/sh\necho mine\n");
    const auto r = run({"db", "hooks", "install", "-C",
        repo.checkout().generic_string()});
    CHECK(r.status == app::exit_findings);
    CHECK_THAT(r.err, ContainsSubstring("post-merge exists and is not "
        "compass's"));
    CHECK(scoped_git_repo::read_file(hooks / "post-merge") ==
        "#!/bin/sh\necho mine\n");
    for (const auto* name : {"post-checkout", "post-rewrite"}) {
        const auto text = scoped_git_repo::read_file(hooks / name);
        CHECK_THAT(text, ContainsSubstring("sync -C \"$PWD\""));
        CHECK_THAT(text, ContainsSubstring("Written by compass db hooks "
            "install"));
    }
    const auto again = run({"db", "hooks", "install", "-C",
        repo.checkout().generic_string()});
    CHECK_THAT(again.out, ContainsSubstring("post-checkout already current"));
}
