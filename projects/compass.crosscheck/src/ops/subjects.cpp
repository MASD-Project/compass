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
#include "compass.crosscheck/ops/subjects.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <vector>
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::ops {

namespace {

namespace pt = compass::platform::text;

constexpr std::array<std::string_view, 7> verbs{"get", "get_many", "list",
    "put", "put_many", "delete", "delete_many"};
constexpr std::array<std::string_view, 3> event_actions{"created", "updated",
    "deleted"};

}

/**
 * @brief Whether a subject conforms, and the reason when it does not, as
 * the script's classify decides.
 */
std::pair<bool, std::string> classify_subject(std::string_view subject) {
    static const pt::regex snake(R"(^[a-z][a-z0-9_]*$)");
    std::vector<std::string_view> segments;
    for (std::size_t start = 0;;) {
        const auto dot = subject.find('.', start);
        segments.push_back(subject.substr(start, dot == std::string_view::npos
                ? std::string_view::npos : dot - start));
        if (dot == std::string_view::npos)
            break;
        start = dot + 1;
    }
    const auto is_snake = [&](std::string_view s) {
        return snake.match(s).has_value();
    };
    const auto quoted = [](std::string_view s) {
        return "'" + std::string(s) + "'";
    };
    const auto in = [](const auto& set, std::string_view s) {
        return std::ranges::find(set, s) != set.end();
    };

    if (segments.size() == 5 && segments[2] == "events") {
        const std::array<std::pair<std::string_view, std::string_view>, 3>
            named{{{"component", segments[0]}, {"version", segments[1]},
                {"resource", segments[3]}}};
        for (const auto& [name, segment] : named) {
            if (!is_snake(segment))
                return {false, std::format("the {} segment {} is not lower "
                    "snake_case", name, quoted(segment))};
        }
        if (segments[1] != "v1")
            return {false, std::format("the version segment is {}, not 'v1'",
                quoted(segments[1]))};
        if (!in(event_actions, segments[4]))
            return {false, std::format("the event action {} is not one of "
                "created, updated, deleted", quoted(segments[4]))};
        return {true, {}};
    }
    if (segments.size() != 4)
        return {false, std::format("{} segments, the grammar has four, or "
            "five for an event", segments.size())};
    const auto component = segments[0];
    const auto version = segments[1];
    const auto resource = segments[2];
    const auto suffix = segments[3];
    if (component == "{component}")
        return {true, {}};
    const std::array<std::pair<std::string_view, std::string_view>, 4>
        named{{{"component", component}, {"version", version},
            {"resource", resource}, {"verb", suffix}}};
    for (const auto& [name, segment] : named) {
        if (!is_snake(segment))
            return {false, std::format("the {} segment {} is not lower "
                "snake_case", name, quoted(segment))};
    }
    if (version != "v1")
        return {false, std::format("the version segment is {}, not 'v1'",
            quoted(version))};
    if (resource == "ops")
        return {true, {}};
    if (in(verbs, suffix) || suffix.starts_with("list_by_"))
        return {true, {}};
    if (resource.ends_with("_events") && in(event_actions, suffix))
        return {false, "the four-segment event spelling; an event names the "
            "events namespace, as in component.v1.events.collection.action"};
    return {false, std::format("the verb {} is not one of the closed set of "
        "eight and is not in the reserved ops namespace", quoted(suffix))};
}

}
