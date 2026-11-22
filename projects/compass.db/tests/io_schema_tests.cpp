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

#include <string>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.db/ops/migrations.hpp"
#include "database.hpp"

namespace {

using namespace compass::db;
using Catch::Matchers::ContainsSubstring;

bool table_exists(io::connection& c, const std::string& name) {
    return c.exec("SELECT to_regclass($1) IS NOT NULL", {name}).value(0, 0) ==
        "t";
}

}

TEST_CASE("init creates the corpus, test and activity schemas",
    "[io][schema]") {
    io::connection c(tests::fresh_database());
    CHECK_FALSE(io::read_status(c).initialised());
    const auto s = io::init(c, "english");
    CHECK(s.version == ops::known_version());
    CHECK(s.current());
    CHECK(s.text_search == "english");
    for (const auto* table : {"corpus.document", "corpus.heading",
             "corpus.section", "corpus.link", "corpus.environment",
             "corpus.placement", "test.scenario", "test.step",
             "activity.campaign", "activity.run", "activity.step_result",
             "activity.note", "activity.attachment", "activity.attached"})
        CHECK(table_exists(c, table));
    CHECK_NOTHROW(io::require_current(c));
}

TEST_CASE("init and migrate run again change nothing", "[io][schema]") {
    io::connection c(tests::fresh_database());
    io::init(c, "english");
    CHECK(io::migrate(c).empty());
    CHECK(io::init(c, "english").version == ops::known_version());
}

TEST_CASE("a database with no schema is refused until init",
    "[io][schema]") {
    io::connection c(tests::fresh_database());
    CHECK_THROWS_WITH(io::require_current(c),
        ContainsSubstring("compass db init"));
}

TEST_CASE("a schema newer than this compass is refused", "[io][schema]") {
    io::connection c(tests::fresh_database());
    io::init(c, "english");
    c.exec("INSERT INTO compass.migration (version, name) VALUES ($1, "
        "'from the future')", {std::to_string(ops::known_version() + 1)});
    CHECK(io::read_status(c).newer());
    CHECK_THROWS_WITH(io::require_current(c),
        ContainsSubstring("newer than this compass"));
    CHECK_THROWS_WITH(io::migrate(c),
        ContainsSubstring("newer than this compass"));
    CHECK_THROWS_WITH(io::init(c, "english"),
        ContainsSubstring("newer than this compass"));
}

TEST_CASE("a schema older than this compass needs compass db migrate",
    "[io][schema]") {
    io::schema_status s;
    s.version = 1;
    s.known = 2;
    const auto why = io::refusal_of(s);
    REQUIRE(why);
    CHECK_THAT(*why, ContainsSubstring("compass db migrate"));
    s.version = 2;
    CHECK_FALSE(io::refusal_of(s));
}

TEST_CASE("the text search configuration is checked and kept",
    "[io][schema]") {
    io::connection c(tests::fresh_database());
    CHECK_THROWS(io::init(c, "no_such_configuration"));
    io::init(c, "simple");
    CHECK(io::read_status(c).text_search == "simple");
    CHECK_THROWS_WITH(io::init(c, "english"),
        ContainsSubstring("already computes its search"));
}
