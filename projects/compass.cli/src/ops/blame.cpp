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
#include "compass.cli/ops/blame.hpp"
#include "compass.cli/ops/provenance.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <iterator>
#include <system_error>

namespace compass::cli::ops {

namespace {

template <typename Number>
std::optional<Number> number_of(std::string_view s) {
    Number v{};
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || end != s.data() + s.size() || s.empty())
        return std::nullopt;
    return v;
}

bool is_commit_id(std::string_view s) {
    return s.size() == 40 && std::ranges::all_of(s, [](char c) {
        return std::isxdigit(static_cast<unsigned char>(c)) != 0;
    });
}

/**
 * @brief The lines of @p text, each with its line end.
 */
std::vector<std::string_view> lines_of(std::string_view text) {
    std::vector<std::string_view> out;
    while (!text.empty()) {
        const auto end = text.find('\n');
        const auto size = end == std::string_view::npos ? text.size()
                                                        : end + 1;
        out.push_back(text.substr(0, size));
        text.remove_prefix(size);
    }
    return out;
}

}

std::string_view method_name(codec::types::blame_method m) noexcept {
    using codec::types::blame_method;
    switch (m) {
    case blame_method::line: return "line";
    case blame_method::added: return "added";
    case blame_method::drift: return "drift";
    case blame_method::bisect: return "bisect";
    }
    return "bisect";
}

blamed_lines parse_blame_porcelain(std::span<const std::string> lines) {
    blamed_lines out;
    std::string current;
    for (const std::string_view line : lines) {
        if (line.size() > 41 && line[40] == ' ' &&
            is_commit_id(line.substr(0, 40))) {
            current = std::string(line.substr(0, 40));
            auto rest = line.substr(41);
            rest = rest.substr(rest.find(' ') + 1);
            const auto final_line = number_of<std::uint32_t>(
                rest.substr(0, rest.find(' ')));
            if (final_line)
                out.commits[*final_line] = current;
            continue;
        }
        constexpr std::string_view time = "committer-time ";
        if (line.starts_with(time) && !current.empty())
            if (const auto t = number_of<std::int64_t>(line.substr(
                    time.size())))
                out.times[current] = *t;
    }
    return out;
}

std::vector<types::trailer> closing_trailers(std::string_view message) {
    std::vector<types::trailer> out;
    for (;;) {
        auto found = trailers_of(message);
        if (found.empty())
            break;
        out.insert(out.end(), std::make_move_iterator(found.begin()),
            std::make_move_iterator(found.end()));
        while (!message.empty() && (message.back() == '\n' ||
                   message.back() == ' ' || message.back() == '\r'))
            message.remove_suffix(1);
        const auto gap = message.rfind("\n\n");
        if (gap == std::string_view::npos)
            break;
        message = message.substr(0, gap);
    }
    return out;
}

std::optional<std::uint64_t> pull_request_in(std::string_view subject) {
    constexpr std::string_view merge = "Merge pull request #";
    if (subject.starts_with(merge)) {
        const auto rest = subject.substr(merge.size());
        return number_of<std::uint64_t>(rest.substr(0, rest.find(' ')));
    }
    while (!subject.empty() && subject.back() == ' ')
        subject.remove_suffix(1);
    const auto open = subject.rfind("(#");
    if (open == std::string_view::npos || !subject.ends_with(')'))
        return std::nullopt;
    return number_of<std::uint64_t>(subject.substr(open + 2,
        subject.size() - open - 3));
}

std::pair<std::uint32_t, std::uint32_t> differing_lines(
    std::string_view file, std::string_view generated) {
    const auto a = lines_of(file);
    const auto b = lines_of(generated);
    std::size_t prefix = 0;
    while (prefix < a.size() && prefix < b.size() && a[prefix] == b[prefix])
        ++prefix;
    std::size_t suffix = 0;
    while (suffix < a.size() - prefix && suffix < b.size() - prefix &&
           a[a.size() - 1 - suffix] == b[b.size() - 1 - suffix])
        ++suffix;
    const auto first = prefix + 1;
    const auto last = a.size() - suffix;
    if (first <= last)
        return {static_cast<std::uint32_t>(first),
            static_cast<std::uint32_t>(last)};
    const auto at = static_cast<std::uint32_t>(std::clamp<std::size_t>(
        prefix, 1, std::max<std::size_t>(a.size(), 1)));
    return {at, at};
}

std::optional<finding_target> parse_target(std::string_view spec) {
    const auto rule_colon = spec.rfind(':');
    if (rule_colon == std::string_view::npos || rule_colon == 0)
        return std::nullopt;
    const auto line_colon = spec.rfind(':', rule_colon - 1);
    if (line_colon == std::string_view::npos || line_colon == 0)
        return std::nullopt;
    const auto rule = spec.substr(rule_colon + 1);
    const auto line = number_of<std::uint32_t>(spec.substr(line_colon + 1,
        rule_colon - line_colon - 1));
    const auto slash = rule.find('/');
    if (!line || *line == 0 || slash == std::string_view::npos ||
        slash == 0 || slash + 1 == rule.size())
        return std::nullopt;
    return finding_target{std::string(spec.substr(0, line_colon)), *line,
        std::string(rule)};
}

std::vector<notice> notices_of(const types::blame_report& r) {
    std::map<std::string, std::vector<const types::blamed_finding*>> named;
    std::vector<const types::blamed_finding*> unnamed;
    for (const auto& b : r.findings) {
        if (b.culprits.empty() || !b.culprits.front().on_main)
            continue;
        const auto& environment = b.culprits.front().environment;
        if (environment.empty())
            unnamed.push_back(&b);
        else
            named[environment].push_back(&b);
    }
    std::vector<notice> out;
    for (auto& [environment, findings] : named)
        out.push_back({environment, std::move(findings)});
    if (!unnamed.empty())
        out.push_back({{}, std::move(unnamed)});
    return out;
}

}
