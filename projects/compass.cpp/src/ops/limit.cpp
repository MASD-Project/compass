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
#include "compass.cpp/ops/limit.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <map>
#include <optional>
#include <set>
#include <system_error>
#include <utility>
#include "compass.cpp/ops/glob.hpp"
#include "compass.cpp/ops/lexer.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.cpp/ops/type_widths.hpp"

namespace compass::cpp::ops {

namespace {

namespace ct = compass::codec::types;

constexpr std::string_view cpp_fields = "cpp_fields";

std::vector<std::string_view> words_of(std::string_view value) {
    std::vector<std::string_view> out;
    std::size_t at = 0;
    while (at < value.size()) {
        const auto start = value.find_first_not_of(' ', at);
        if (start == std::string_view::npos)
            break;
        const auto end = std::min(value.find(' ', start), value.size());
        out.push_back(value.substr(start, end - start));
        at = end;
    }
    return out;
}

std::optional<std::uint32_t> count_of(std::string_view s) {
    std::uint32_t n = 0;
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), n);
    if (ec != std::errc{} || end != s.data() + s.size())
        return std::nullopt;
    return n;
}

/**
 * @brief A type over the ceiling, as a fact line states it.
 */
struct measured {
    std::string_view name;
    std::uint32_t offset = 0;
    std::string_view key;
    std::uint32_t count = 0;
};

std::vector<measured> measured_of(std::string_view fact) {
    std::vector<measured> out;
    for (const auto line : split_lines(fact)) {
        std::array<std::string_view, 4> cells;
        std::size_t at = 0;
        for (auto& c : cells) {
            const auto tab = std::min(line.find('\t', at), line.size());
            c = line.substr(at, tab - at);
            at = tab + 1;
        }
        out.push_back({cells[0], count_of(cells[1]).value_or(0), cells[2],
            count_of(cells[3]).value_or(0)});
    }
    return out;
}

/**
 * @brief The offset of the table row of @p source whose first cells are
 * @p path and @p type, or 0.
 */
std::uint32_t row_of(std::string_view source, std::string_view path,
    std::string_view type) {
    std::uint32_t offset = 0;
    for (const auto line : split_lines_keeping_ends(source)) {
        const auto row = strip(line);
        if (row.starts_with('|')) {
            std::vector<std::string_view> cells;
            std::size_t at = 1;
            while (at < row.size()) {
                const auto bar = std::min(row.find('|', at), row.size());
                auto cell = strip(row.substr(at, bar - at));
                if (cell.size() > 1 && (cell.front() == '=' ||
                        cell.front() == '~') && cell.back() == cell.front())
                    cell = cell.substr(1, cell.size() - 2);
                cells.push_back(cell);
                at = bar + 1;
            }
            if (cells.size() >= 2 && cells[0] == path && cells[1] == type)
                return offset;
        }
        offset += static_cast<std::uint32_t>(line.size());
    }
    return 0;
}

}

std::expected<limit_definition, std::string>
parse_limit_definition(std::string_view text) {
    limit_definition d;
    std::set<std::string, std::less<>> seen;
    std::set<std::pair<std::string, std::string>> recorded;
    std::uint32_t line = 0;
    const auto fail = [&](const std::string& what) {
        return std::unexpected("line " + std::to_string(line) + ": " + what);
    };
    for (const auto raw : split_lines(text)) {
        ++line;
        const auto l = strip(raw);
        if (l.empty())
            continue;
        const auto space = l.find(' ');
        const auto key = l.substr(0, space);
        const auto value = space == std::string_view::npos ? std::string_view{}
            : strip(l.substr(space + 1));
        if (key == "RULE" || key == "EXAMPLE")
            return fail("a limit check has the rules over_ceiling and "
                "below_record, and no examples; remove " + std::string(key));
        if (key != "RECORDED" && !seen.emplace(key).second)
            return fail(std::string(key) + " is given twice");
        if (value.empty())
            return fail(std::string(key) + " has no value");
        if (key == "SOURCE") {
            d.source = std::string(value);
        } else if (key == "FILES" || key == "OUTSIDE") {
            if (auto ok = add_globs(value,
                    key == "FILES" ? d.files : d.outside); !ok)
                return fail(ok.error());
        } else if (key == "FACT") {
            if (value != cpp_fields)
                return fail(std::format("the fact is '{}'; compass counts "
                    "{}", value, cpp_fields));
            d.fact = std::string(value);
        } else if (key == "CEILING") {
            const auto n = count_of(value);
            if (!n)
                return fail(std::format("the ceiling '{}' is not a count",
                    value));
            d.ceiling = *n;
        } else if (key == "RECORDED") {
            const auto words = words_of(value);
            const auto n = words.size() == 3 ? count_of(words[2])
                : std::nullopt;
            if (!n)
                return fail(std::format("'{}' is not a path, a type and a "
                    "count", value));
            if (!recorded.emplace(words[0], words[1]).second)
                return fail(std::format("{} of {} is recorded twice",
                    words[1], words[0]));
            d.recorded.push_back({std::string(words[0]),
                std::string(words[1]), *n});
        } else {
            return fail("unknown key " + std::string(key));
        }
    }
    for (const auto* missing : {"FILES", "FACT", "CEILING"}) {
        if (!seen.contains(std::string_view(missing)))
            return std::unexpected(std::string("the definition has no ") +
                missing);
    }
    for (const auto& r : d.recorded) {
        if (r.ceiling <= d.ceiling)
            return std::unexpected(std::format("{} of {} is recorded at {}, "
                "not above the ceiling of {}", r.type, r.path, r.ceiling,
                d.ceiling));
    }
    return d;
}

std::string limit_fact(const limit_definition& d, std::string_view bytes) {
    std::string out;
    const auto l = lex(bytes);
    for (const auto& w : type_widths(l)) {
        if (w.fields > d.ceiling)
            out += std::format("{}\t{}\t{}\t{}\n", w.name, w.offset, w.key,
                w.fields);
    }
    return out;
}

void limit_findings(const limit_definition& d,
    std::span<const ct::scanned_file> files, std::string_view source,
    ct::diagnostic_table& out) {
    std::map<std::pair<std::string_view, std::string_view>, std::uint32_t>
        ceilings;
    for (const auto& r : d.recorded)
        ceilings[{r.path, r.type}] = r.ceiling;
    std::map<std::pair<std::string_view, std::string_view>, std::uint32_t>
        widest;
    for (const auto& f : files) {
        for (const auto& m : measured_of(f.fact)) {
            auto& w = widest[{f.path, m.name}];
            w = std::max(w, m.count);
            const auto recorded = ceilings.find({f.path, m.name});
            const auto ceiling = recorded == ceilings.end() ? d.ceiling
                : recorded->second;
            if (m.count <= ceiling)
                continue;
            const auto against = recorded == ceilings.end()
                ? std::format("the ceiling is {}", d.ceiling)
                : std::format("its recorded ceiling is {}", ceiling);
            report(out, f.path, m.offset, 0, std::format("{} {} has {} "
                "fields; {}", m.key, m.name, m.count, against),
                "group fields that belong together into types of their "
                "own, so the type holds fewer");
        }
    }
    for (const auto& r : d.recorded) {
        const auto found = widest.find({r.path, r.type});
        const auto count = found == widest.end() ? 0 : found->second;
        if (count >= r.ceiling)
            continue;
        const auto now = found == widest.end()
            ? std::format("is not over the ceiling of {}, or not in {}",
                d.ceiling, r.path)
            : std::format("now has {} fields", count);
        report(out, d.source, row_of(source, r.path, r.type), 1,
            std::format("{} is recorded at {} fields and {}", r.type,
                r.ceiling, now), found == widest.end()
                ? "remove its row" : std::format("lower its row to {}",
                    count));
    }
}

}
