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
#include "compass.crosscheck/codecs/subject_conformance.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

constexpr std::uint32_t outside_protocol = 0;
constexpr std::uint32_t no_declaration = 1;
constexpr std::uint32_t unreadable_model = 2;

constexpr std::array<ct::rule, 3> rules{{
    {"outside_protocol", "Every NATS subject a model declares or a writing "
        "site spells is inside the entity protocol."},
    {"no_declaration", "Some model declares a :subject:."},
    {"unreadable_model", "Every model is UTF-8, which the script's strict "
        "read needs."}}};

constexpr std::array<std::string_view, 7> verbs{"get", "get_many", "list",
    "put", "put_many", "delete", "delete_many"};
constexpr std::array<std::string_view, 3> event_actions{"created", "updated",
    "deleted"};

/**
 * @brief Whether a subject conforms, and the reason when it does not, as
 * the script's classify decides.
 */
std::pair<bool, std::string> classify(std::string_view subject) {
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

/**
 * @brief A subject and where it is: the file, its 1-based line and the
 * bytes of the file.
 */
struct site {
    std::string_view path;
    std::uint32_t line;
    std::string subject;
};

bool is_model(std::string_view path) {
    const auto parts = co::components_of(path);
    return parts.size() == 4 && parts[2] == "modeling" &&
        path.ends_with(".org");
}

void declared(const ct::corpus_view& v, std::vector<site>& out,
    ct::diagnostic_table& problems) {
    static const pt::regex line_pattern(R"(^\s*:subject:\s*(\S+)\s*$)");
    auto models = v.list_files("projects/", ".org");
    std::erase_if(models, [](auto p) { return !is_model(p); });
    std::ranges::sort(models, co::component_less);
    for (const auto path : models) {
        const auto bytes = v.read_file(path).value_or("");
        if (!pt::is_utf8(bytes)) {
            co::report(problems, path, 0, unreadable_model,
                "the model is not UTF-8, so its subjects cannot be read",
                "save the model as UTF-8");
            continue;
        }
        const auto text = pt::universal_newlines(bytes);
        std::uint32_t number = 0;
        for (const auto line : co::split_lines(text)) {
            ++number;
            if (const auto m = line_pattern.match(line))
                out.push_back({path, number, std::string(m->group(1))});
        }
    }
}

void written(const ct::corpus_view& v, std::vector<site>& out) {
    static const pt::regex constant(
        R"(\w*subject\w*\s*=\s*\n?\s*\"([^\"]+)\')");
    static const pt::regex seeded(
        R"(\'([a-z_]+\.[a-z0-9_]+\.[a-z0-9_.]+)\')");
    constexpr std::string_view dq =
        "projects/ores.sql/populate/dq/dq_artefact_types_populate.sql";

    std::set<std::tuple<std::string_view, std::uint32_t, std::string>> seen;
    const auto scan = [&](std::string_view path, const pt::regex& pattern,
        std::string_view needle) {
        const auto bytes = v.read_file(path);
        if (!bytes || bytes->find(needle) == std::string::npos)
            return;
        const auto text = pt::universal_newlines(pt::decode_replacing(*bytes));
        for (const auto& m : pattern.find_all(text)) {
            std::string subject(m.group(1));
            if (subject.ends_with('.') || subject.contains('>') ||
                subject.contains('*')) {
                while (subject.ends_with('.'))
                    subject.pop_back();
            }
            if (subject.empty())
                continue;
            const auto line = co::line_of(text, m.start());
            if (seen.emplace(path, line, subject).second)
                out.push_back({path, line, subject});
        }
    };
    for (const auto suffix : {".hpp", ".cpp"}) {
        auto paths = v.list_files("projects/", suffix);
        std::ranges::sort(paths, co::component_less);
        for (const auto path : paths)
            scan(path, constant, "subject");
    }
    scan(dq, seeded, "'");
}

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    std::vector<site> subjects;
    declared(v, subjects, out);
    if (subjects.empty()) {
        co::report(out, "projects", 0, no_declaration,
            "no :subject: declaration found; the scan is not reading the "
            "models", "the check reads projects/<component>/modeling/*.org");
        return;
    }
    written(v, subjects);
    for (const auto& s : subjects) {
        const auto [ok, reason] = classify(s.subject);
        if (ok)
            continue;
        const auto bytes = v.read_file(s.path).value_or("");
        co::report(out, s.path, co::offset_of_line(bytes, s.line),
            outside_protocol, s.subject + ": " + reason,
            "A subject is {component}.v1.{resource}.{verb} with a verb from "
            "the closed set of eight, {component}.v1.events.{collection}."
            "{action}, or {component}.v1.ops.{operation}; see "
            "doc/knowledge/architecture/nats_entity_protocol.org.");
    }
}

}

const ct::descriptor subject_conformance{
    .name = "subject_conformance",
    .version = 1,
    .summary = "Reports each NATS subject a model declares or a writing "
        "site spells outside the entity protocol.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}
