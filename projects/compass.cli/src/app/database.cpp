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
#include "compass.cli/app/database.hpp"

#include <array>
#include <exception>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include "compass.cli/app/application.hpp"
#include "compass.cli/app/sync.hpp"
#include "compass.cli/app/test_export.hpp"
#include "compass.cli/config/local_settings.hpp"
#include "compass.cli/io/listing.hpp"
#include "compass.db/io/connection.hpp"
#include "compass.db/io/corpus.hpp"
#include "compass.db/io/schema.hpp"
#include "compass.platform/filesystem/new_file.hpp"
#include "compass.platform/process/executable.hpp"
#include "compass.platform/process/run_program.hpp"

namespace compass::cli::app {

namespace {

namespace fs = std::filesystem;

/**
 * @brief The line every hook compass writes holds, so a later install
 * knows the hook is compass's to rewrite.
 */
constexpr std::string_view hook_marker =
    "# Written by compass db hooks install";

constexpr std::array<std::string_view, 3> hook_names{
    "post-checkout", "post-merge", "post-rewrite"};

/**
 * @brief The local settings of @p checkout, or a message saying why they
 * cannot be read.
 */
config::local_settings settings_of(const fs::path& checkout) {
    auto s = config::read_local_settings(checkout);
    if (!s)
        throw std::runtime_error(s.error());
    return std::move(*s);
}

/**
 * @brief A connection to the database the local settings of @p checkout
 * name.
 * @throws std::runtime_error when they name none.
 */
db::io::connection connect(const config::local_settings& s) {
    if (s.database.empty())
        throw std::runtime_error("no database is configured; fix: run "
            "compass db init --database <connection string>");
    return db::io::connection(s.database);
}

void write_sync(std::ostream& to, const sync_report& r, bool steps) {
    const auto& w = r.written;
    to << std::format("compass: synced {}: {} documents placed, {} new to "
        "the corpus ({} parsed), placements {} added, {} changed, {} removed, "
        "in {:.0f} ms\n", r.environment, r.placed, r.loaded + r.parsed,
        r.parsed, w.added, w.changed, w.removed, r.ms);
    if (r.collection.ran)
        to << std::format("compass: collected the corpus: {} documents "
            "removed, {} left\n", r.collection.removed, r.collection.left);
    for (const auto& e : r.errors)
        to << "compass: warning: not synced: " << e << "\n";
    if (!steps)
        return;
    for (const auto& [name, ms] : r.steps)
        to << std::format("compass: sync step {}: {:.1f} ms\n", name, ms);
}

/**
 * @brief The compass a git hook runs: compass-ng from PATH when it is
 * there, as an install puts it, else this binary.
 */
std::string hook_program() {
    if (platform::process::find_program("compass-ng"))
        return "compass-ng";
    return platform::process::executable_path().generic_string();
}

/**
 * @brief The directory git runs the hooks of @p checkout from, as git
 * rev-parse --git-path hooks names it, so core.hooksPath and linked
 * worktrees are followed.
 */
fs::path hooks_dir(const fs::path& checkout) {
    platform::process::program_call call;
    call.arguments = {"git", "rev-parse", "--git-path", "hooks"};
    call.directory = checkout;
    call.errors = platform::process::error_stream::merge;
    const auto r = platform::process::run_program(call);
    auto path = r.output;
    while (!path.empty() && (path.back() == '\n' || path.back() == '\r'))
        path.pop_back();
    if (r.exit_code != 0 || path.empty())
        throw std::runtime_error("git cannot name the hooks directory: " +
            r.output);
    const fs::path p(path);
    return p.is_absolute() ? p : checkout / p;
}

int install_hooks(const fs::path& checkout, std::ostream& out,
    std::ostream& err) {
    const auto dir = hooks_dir(checkout);
    const auto program = hook_program();
    const auto text = std::format(
        "#!/bin/sh\n"
        "{}; it syncs compass's database in the\n"
        "# background after a checkout, merge or rewrite. Keep this line to "
        "let a\n"
        "# later install rewrite the file.\n"
        "\"{}\" sync -C \"$PWD\" >/dev/null 2>&1 &\n",
        hook_marker, program);
    int status = exit_success;
    for (const auto name : hook_names) {
        const auto path = dir / name;
        std::error_code ec;
        if (fs::exists(path, ec)) {
            std::ifstream is(path, std::ios::binary);
            const std::string old{std::istreambuf_iterator<char>(is), {}};
            if (old.find(hook_marker) == std::string::npos) {
                err << "compass: " << path.generic_string() << " exists and "
                    "is not compass's, so it was left as it is; to sync "
                    "there too, add this line to it:\n  \"" << program
                    << "\" sync -C \"$PWD\" >/dev/null 2>&1 &\n";
                status = exit_findings;
                continue;
            }
        }
        const bool changed = platform::filesystem::update_file(path, text);
        fs::permissions(path, fs::perms::owner_all | fs::perms::group_read |
            fs::perms::group_exec | fs::perms::others_read |
            fs::perms::others_exec, ec);
        out << "compass: " << path.generic_string()
            << (changed ? " written\n" : " already current\n");
    }
    return status;
}

int init(const types::invocation& i, std::ostream& out) {
    auto s = settings_of(i.checkout);
    const auto& given = i.db.database;
    if (!given.empty())
        s.database = given;
    auto c = connect(s);
    const auto text_search = i.db.text_search.empty()
        ? std::string(db::io::default_text_search)
        : i.db.text_search;
    const auto status = db::io::init(c, text_search);
    if (!given.empty()) {
        const auto file = config::repository_settings_path(i.checkout);
        config::write_local_setting(file, "database", given);
        s.database_file = file;
    }
    out << std::format("compass: the database is at schema version {} "
        "with text search configuration {}; its connection is in {}\n",
        status.version, status.text_search,
        s.database_file.generic_string());
    return exit_success;
}

int migrate(const types::invocation& i, std::ostream& out) {
    auto c = connect(settings_of(i.checkout));
    const auto applied = db::io::migrate(c);
    if (applied.empty())
        out << "compass: the database's schema is current\n";
    for (const auto v : applied)
        out << "compass: applied migration " << v << "\n";
    return exit_success;
}

std::uint64_t count_of(db::io::connection& c, std::string_view table) {
    return std::stoull(std::string(c.exec(std::format(
        "SELECT count(*) FROM {}", table)).value(0, 0)));
}

int status(const types::invocation& i, std::ostream& out) {
    const auto s = settings_of(i.checkout);
    auto c = connect(s);
    const auto st = db::io::read_status(c);
    out << std::format("database: configured in {}\n",
        s.database_file.generic_string());
    out << std::format("schema: version {} of {}\n", st.version, st.known);
    if (const auto why = db::io::refusal_of(st)) {
        out << "compass: " << *why << "\n";
        return exit_findings;
    }
    out << "text search: " << st.text_search << "\n";
    for (const auto table : {"corpus.document", "corpus.section",
             "corpus.link", "test.scenario"})
        out << std::format("{}: {} rows\n", table, count_of(c, table));
    out << "size: " << c.exec("SELECT pg_size_pretty(pg_database_size("
        "current_database()))").value(0, 0) << "\n";
    for (const auto& e : db::io::environments(c))
        out << std::format("environment {}: {}, {} documents, synced {}{}\n",
            e.name, e.kind, e.documents,
            e.synced_at.empty() ? "never" : e.synced_at,
            e.root.empty() ? "" : ", at " + e.root);
    return exit_success;
}

int prune(const types::invocation& i, std::ostream& out) {
    auto c = connect(settings_of(i.checkout));
    db::io::require_current(c);
    if (!i.environment.empty()) {
        if (!db::io::drop_environment(c, i.environment)) {
            out << "compass: there is no environment " << i.environment
                << "\n";
            return exit_findings;
        }
        out << "compass: dropped " << i.environment << "\n";
    } else {
        for (const auto& e : db::io::environments(c)) {
            std::error_code ec;
            if (e.kind != "worktree" || e.root.empty() ||
                fs::is_directory(e.root, ec))
                continue;
            db::io::drop_environment(c, e.name);
            out << "compass: dropped " << e.name << ", whose worktree "
                << e.root << " is gone\n";
        }
    }
    const auto r = db::io::collect(c);
    if (r.skipped)
        out << "compass: another collection is running; nothing collected\n";
    else
        out << std::format("compass: collected the corpus: {} documents "
            "removed, {} left\n", r.removed, r.left);
    return exit_success;
}

}

int run_db(const types::invocation& i, std::ostream& out,
    std::ostream& err) {
    using types::db_action;
    try {
        switch (i.db.action) {
        case db_action::init: return init(i, out);
        case db_action::migrate: return migrate(i, out);
        case db_action::status: return status(i, out);
        case db_action::hooks_install: return install_hooks(i.checkout, out,
            err);
        case db_action::prune: return prune(i, out);
        }
    } catch (const std::exception& e) {
        err << "compass: db: " << e.what() << "\n";
    }
    return exit_failure;
}

int run_sync(const types::invocation& i, std::ostream& out,
    std::ostream& err) {
    try {
        const auto s = settings_of(i.checkout);
        auto c = connect(s);
        db::io::require_current(c);
        sync_request r;
        r.checkout = i.checkout;
        r.threads = i.threads;
        r.environment = config::environment_name(i.checkout, s,
            i.environment);
        if (!i.revision.empty())
            r.commit = io::resolve_commit(i.checkout, i.revision);
        const auto report = sync_environment(c, r);
        write_sync(out, report, i.timings);
        return report.errors.empty() ? exit_success : exit_findings;
    } catch (const std::exception& e) {
        err << "compass: sync: " << e.what() << "\n";
    }
    return exit_failure;
}

int run_test(const types::invocation& i, std::ostream& out,
    std::ostream& err) {
    try {
        const auto s = settings_of(i.checkout);
        auto c = connect(s);
        db::io::require_current(c);
        const auto environment = config::environment_name(i.checkout, s,
            i.environment);
        const auto r = export_campaigns(c, i.checkout, environment,
            i.test.story);
        if (i.test.story && r.campaigns == 0 && r.skipped.empty()) {
            err << "compass: test export: no test campaign of story "
                << *i.test.story << " on " << environment << "\n";
            return exit_failure;
        }
        for (const auto& path : r.written)
            out << "compass: wrote " << path << "\n";
        out << std::format("compass: exported {} test campaigns of {}: {} "
            "files written, {} unchanged\n", r.campaigns, environment,
            r.written.size(), r.unchanged);
        if (r.not_png != 0)
            out << std::format("compass: {} screenshots are JPEG, GIF or "
                "WebP, not PNG, and were written as they are\n", r.not_png);
        for (const auto& story : r.skipped)
            err << "compass: warning: the campaign of story " << story
                << " was not exported: " << environment << " no longer "
                "holds the story's page\n";
        return r.skipped.empty() ? exit_success : exit_findings;
    } catch (const std::exception& e) {
        err << "compass: test export: " << e.what() << "\n";
    }
    return exit_failure;
}

void sync_after_run(const fs::path& checkout, unsigned threads, bool timings,
    std::ostream& err) {
    try {
        const auto s = settings_of(checkout);
        if (s.database.empty())
            return;
        auto c = connect(s);
        db::io::require_current(c);
        sync_request r;
        r.checkout = checkout;
        r.threads = threads;
        r.environment = config::environment_name(checkout, s, {});
        const auto report = sync_environment(c, r);
        if (timings)
            write_sync(err, report, true);
    } catch (const std::exception& e) {
        err << "compass: warning: the database was not synced: " << e.what()
            << "\n";
    }
}

void sync_after_edit(const fs::path& checkout, std::string_view path,
    bool timings, std::ostream& err) {
    try {
        if (!is_synced_document(checkout, path))
            return;
        const auto s = settings_of(checkout);
        if (s.database.empty())
            return;
        auto c = connect(s);
        db::io::require_current(c);
        const auto report = sync_file(c, checkout,
            config::environment_name(checkout, s, {}), path);
        if (timings)
            write_sync(err, report, false);
        else
            for (const auto& e : report.errors)
                err << "compass: warning: not synced: " << e << "\n";
    } catch (const std::exception& e) {
        err << "compass: warning: the database was not synced: " << e.what()
            << "\n";
    }
}

}
