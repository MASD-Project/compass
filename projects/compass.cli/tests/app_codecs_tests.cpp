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
#include <array>
#include <cstddef>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.cli/app/codecs.hpp"
#include "compass.codec/ops/exceptions.hpp"

namespace {

using namespace compass;
namespace ct = codec::types;

bool is_uuid(std::string_view s) {
    constexpr std::string_view shape = "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX";
    if (s.size() != shape.size())
        return false;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        const bool hex = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
        if (shape[i] == '-' ? c != '-' : !hex)
            return false;
    }
    return true;
}

/**
 * @brief Each defect of @p r, named with @p codec, or nothing.
 */
std::vector<std::string> defects_of(std::string_view codec,
    const ct::rule& r) {
    const auto id = std::string(codec) + "/" + std::string(r.name);
    std::vector<std::string> out;
    if (r.summary.empty() || !r.summary.ends_with('.'))
        out.push_back(id + ": the statement is not one sentence");
    if (r.summary.contains('\n'))
        out.push_back(id + ": the statement spans lines");
    std::set<std::string_view> seen;
    for (const auto source : r.enforces) {
        if (!is_uuid(source))
            out.push_back(id + ": " + std::string(source) +
                " is not an uppercase UUID");
        if (!seen.insert(source).second)
            out.push_back(id + ": cites " + std::string(source) + " twice");
    }
    return out;
}

}

TEST_CASE("every rule compass knows has a one-line statement and cites "
    "memories by UUID", "[app][codecs]") {
    std::vector<std::string> defects;
    for (const auto& d : cli::app::codecs()) {
        std::set<std::string_view> names;
        for (const auto& r : d.rules) {
            if (!names.insert(r.name).second)
                defects.push_back(std::string(d.name) + "/" +
                    std::string(r.name) + ": declared twice");
            for (auto& m : defects_of(d.name, r))
                defects.push_back(std::move(m));
        }
    }
    for (const auto& r : codec::ops::exception_rules)
        for (auto& m : defects_of(codec::ops::exception_codec, r))
            defects.push_back(std::move(m));
    for (auto& m : defects_of("<files codec>", codec::ops::drift_rule))
        defects.push_back(std::move(m));
    CHECK(defects == std::vector<std::string>{});
}

TEST_CASE("the check of a rule's metadata refuses a malformed citation",
    "[app][codecs]") {
    constexpr std::array<std::string_view, 3> sources{
        "5b528519-8bce-4cb8-a48e-b525fd71f873",
        "5B528519-8BCE-4CB8-A48E-B525FD71F87",
        "5B528519-8BCE-4CB8-A48E-B525FD71F873"};
    const ct::rule lowercase{"r", "A statement.", {sources.data(), 1}};
    const ct::rule short_id{"r", "A statement.", {sources.data() + 1, 1}};
    const ct::rule good{"r", "A statement.", {sources.data() + 2, 1}};
    const std::array twice{sources[2], sources[2]};
    const ct::rule repeated{"r", "A statement.", twice};
    const ct::rule unfinished{"r", "A statement", {}};

    CHECK(defects_of("c", lowercase).size() == 1);
    CHECK(defects_of("c", short_id).size() == 1);
    CHECK(defects_of("c", good).empty());
    CHECK(defects_of("c", repeated) ==
        std::vector<std::string>{"c/r: cites " + std::string(sources[2]) +
            " twice"});
    CHECK(defects_of("c", unfinished) ==
        std::vector<std::string>{"c/r: the statement is not one sentence"});
}
