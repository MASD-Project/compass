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
#include "compass.env/ops/templates.hpp"

#include <map>
#include <string>
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace compass::env::ops;

const resolver known = [](std::string_view name) -> std::optional<std::string> {
    static const std::map<std::string, std::string, std::less<>> values{
        {"a", "1"}, {"empty", ""}, {"service.key", "K"}};
    const auto it = values.find(name);
    return it == values.end() ? std::nullopt
                              : std::optional<std::string>(it->second);
};

}

TEST_CASE("a template replaces names, keeps text and unescapes braces",
    "[templates]") {
    CHECK(expand("x{a}y{service.key}", known) == "x1yK");
    CHECK(expand("{{a}}", known) == "{a}");
    CHECK(expand("plain", known) == "plain");
}

TEST_CASE("a fallback stands in for an empty value only", "[templates]") {
    CHECK(expand("{empty:-5432}", known) == "5432");
    CHECK(expand("{a:-5432}", known) == "1");
}

TEST_CASE("an unknown name or a brace that opens no name fails",
    "[templates]") {
    CHECK_FALSE(expand("{nope}", known));
    CHECK_FALSE(expand("{ccache,sccache}", known));
    CHECK_FALSE(expand("{a", known));
}

TEST_CASE("the names of a template are listed in order", "[templates]") {
    CHECK(names_in("{b}-{a:-x}-{{c}}") == std::vector<std::string>{"b", "a"});
}
