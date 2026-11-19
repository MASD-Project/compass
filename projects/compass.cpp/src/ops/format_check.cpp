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
#include "compass.cpp/ops/format_check.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
#include <stdexcept>
#include "compass.cpp/ops/glob.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/process/executable.hpp"

namespace compass::cpp::ops {

namespace {

namespace ct = compass::codec::types;

/**
 * @brief The formatters a format check may name, as compass's codegen
 * runs them, without their program and style.
 */
const std::array<ct::formatter, 2> known_formatters{{
    {.name = "prettier", .program = {},
     .extensions = {".ts", ".tsx", ".js", ".jsx", ".mjs", ".cjs", ".json",
         ".css", ".scss", ".md", ".yaml", ".yml", ".html"},
     .arguments = {"--write", "--log-level", "warn", "--config", "{style}"},
     .passes = 1, .style = {}},
    {.name = "clang-format", .program = {},
     .extensions = {".cpp", ".hpp", ".h", ".cxx", ".cc", ".hxx", ".ipp",
         ".c"},
     .arguments = {"-i", "--style=file:{style}"}, .passes = 2, .style = {}}}};

std::string_view trimmed(std::string_view s) {
    while (!s.empty() && s.front() == ' ')
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\r'))
        s.remove_suffix(1);
    return s;
}

std::vector<std::string> words_of(std::string_view value) {
    std::vector<std::string> out;
    std::size_t at = 0;
    while (at < value.size()) {
        const auto start = value.find_first_not_of(' ', at);
        if (start == std::string_view::npos)
            break;
        const auto end = std::min(value.find(' ', start), value.size());
        out.emplace_back(value.substr(start, end - start));
        at = end;
    }
    return out;
}

}

std::expected<format_definition, std::string>
parse_format_definition(std::string_view text) {
    format_definition d;
    std::set<std::string, std::less<>> seen;
    std::uint32_t line = 0;
    const auto fail = [&](const std::string& what) {
        return std::unexpected("line " + std::to_string(line) + ": " + what);
    };
    for (const auto raw : split_lines_keeping_ends(text)) {
        ++line;
        const auto l = trimmed(raw.substr(0, raw.find('\n')));
        if (l.empty())
            continue;
        const auto space = l.find(' ');
        const auto key = l.substr(0, space);
        const auto value = space == std::string_view::npos ? std::string_view{}
            : trimmed(l.substr(space + 1));
        if (key == "RULE" || key == "EXAMPLE")
            return fail("a format check has one rule, unformatted, and no "
                "examples; remove " + std::string(key));
        if (!seen.emplace(key).second)
            return fail(std::string(key) + " is given twice");
        if (value.empty())
            return fail(std::string(key) + " has no value");
        if (key == "SOURCE") {
            d.source = std::string(value);
        } else if (key == "FILES" || key == "OUTSIDE") {
            if (auto ok = add_globs(value,
                    key == "FILES" ? d.files : d.outside); !ok)
                return fail(ok.error());
        } else if (key == "FORMATTER") {
            if (std::ranges::none_of(known_formatters,
                    [&](const auto& f) { return f.name == value; }))
                return fail("the formatter is '" + std::string(value) +
                    "'; compass knows prettier and clang-format");
            d.formatter = std::string(value);
        } else if (key == "PROGRAM") {
            d.programs = words_of(value);
        } else if (key == "STYLE") {
            d.style = std::string(value);
        } else {
            return fail("unknown key " + std::string(key));
        }
    }
    for (const auto* missing : {"FILES", "FORMATTER", "PROGRAM", "STYLE"}) {
        if (!seen.contains(std::string_view(missing)))
            return std::unexpected(std::string("the definition has no ") +
                missing);
    }
    return d;
}

ct::formatter formatter_of(const format_definition& d,
    const std::filesystem::path& checkout) {
    namespace pp = compass::platform::process;
    auto f = *std::ranges::find(known_formatters, d.formatter,
        &ct::formatter::name);
    for (const auto& candidate : d.programs) {
        const auto found = candidate.find('/') == std::string::npos
            ? pp::find_program(candidate)
            : pp::find_program((checkout / candidate).generic_string());
        if (found) {
            f.program = *found;
            f.style = checkout / d.style;
            return f;
        }
    }
    std::string names;
    for (const auto& candidate : d.programs)
        names += (names.empty() ? "" : ", ") + candidate;
    throw std::runtime_error(d.source + ": none of " + names +
        " is a program, for the formatter " + d.formatter);
}

}
