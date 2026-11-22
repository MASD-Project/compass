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
#include "compass.db/io/schema.hpp"

#include <charconv>
#include <format>
#include <utility>
#include "compass.db/ops/migrations.hpp"

namespace compass::db::io {

namespace {

/**
 * @brief The key of the advisory lock migrations take, so two migrates of
 * one database apply each migration once.
 */
constexpr std::string_view migration_lock = "4370616373737301";

std::uint32_t to_version(std::string_view text) {
    std::uint32_t v = 0;
    std::from_chars(text.data(), text.data() + text.size(), v);
    return v;
}

std::string setting(connection& c, std::string_view key) {
    const auto r = c.exec("SELECT value FROM compass.setting WHERE key = $1",
        {std::string(key)});
    return r.rows() == 0 ? std::string() : std::string(r.value(0, 0));
}

[[noreturn]] void refuse(std::string message) {
    throw error(std::move(message), "");
}

}

schema_status read_status(connection& c) {
    schema_status s;
    s.known = ops::known_version();
    const auto exists = c.exec(
        "SELECT to_regclass('compass.migration') IS NOT NULL");
    if (exists.value(0, 0) != "t")
        return s;
    const auto r = c.exec(
        "SELECT coalesce(max(version), 0) FROM compass.migration");
    s.version = to_version(r.value(0, 0));
    s.text_search = setting(c, "text_search");
    return s;
}

std::vector<std::uint32_t> migrate(connection& c) {
    std::vector<std::uint32_t> applied;
    {
        transaction t(c);
        c.exec(std::format("SELECT pg_advisory_xact_lock(x'{}'::bigint)",
            migration_lock));
        c.run_script(ops::bookkeeping_sql());
        t.commit();
    }
    for (const auto& m : ops::migrations()) {
        transaction t(c);
        c.exec(std::format("SELECT pg_advisory_xact_lock(x'{}'::bigint)",
            migration_lock));
        const auto status = read_status(c);
        if (status.newer())
            refuse(std::format("the database's schema is version {}, newer "
                "than this compass knows ({}); fix: upgrade compass",
                status.version, status.known));
        if (status.version >= m.version)
            continue;
        c.run_script(m.sql);
        c.exec("INSERT INTO compass.migration (version, name) VALUES ($1, $2)",
            {std::to_string(m.version), std::string(m.name)});
        t.commit();
        applied.push_back(m.version);
    }
    return applied;
}

schema_status init(connection& c, std::string_view text_search) {
    c.exec("SELECT $1::regconfig", {std::string(text_search)});
    const auto before = read_status(c);
    if (before.newer())
        refuse(std::format("the database's schema is version {}, newer than "
            "this compass knows ({}); fix: upgrade compass", before.version,
            before.known));
    if (!before.text_search.empty() && before.text_search != text_search)
        refuse(std::format("the database already computes its search with "
            "the text search configuration {}, not {}; fix: init a new "
            "database to change it", before.text_search, text_search));
    migrate(c);
    c.exec("INSERT INTO compass.setting (key, value) VALUES ('text_search', "
        "$1) ON CONFLICT (key) DO NOTHING", {std::string(text_search)});
    return read_status(c);
}

std::optional<std::string> refusal_of(const schema_status& s) {
    if (!s.initialised())
        return "the database has no compass schema; fix: run compass db "
               "init";
    if (s.newer())
        return std::format("the database's schema is version {}, newer than "
            "this compass knows ({}); fix: upgrade compass", s.version,
            s.known);
    if (!s.current())
        return std::format("the database's schema is version {}, older than "
            "this compass's ({}); fix: run compass db migrate", s.version,
            s.known);
    return std::nullopt;
}

void require_current(connection& c) {
    if (auto why = refusal_of(read_status(c)))
        refuse(std::move(*why));
}

}
