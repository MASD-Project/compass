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
#include "compass.codec/ops/exceptions.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <filesystem>
#include <format>
#include <iterator>
#include <optional>
#include <ranges>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <variant>

namespace compass::codec::ops {

namespace {

namespace ct = compass::codec::types;

bool is_exception_rule(std::string_view rule) noexcept {
    return rule.starts_with(exception_codec) &&
        rule.substr(exception_codec.size()).starts_with('/');
}

constexpr std::string_view blanks = " \t";
constexpr std::string_view written_as = "write it as compass:except(<codec>/"
    "<rule>) expires=<YYYY-MM-DD> approver=<name> reason=<why>";

std::string_view trim_right(std::string_view s, std::string_view chars) {
    const auto end = s.find_last_not_of(chars);
    return end == std::string_view::npos ? std::string_view{}
                                         : s.substr(0, end + 1);
}

std::string_view trim(std::string_view s) {
    const auto begin = s.find_first_not_of(blanks);
    return begin == std::string_view::npos
        ? std::string_view{} : trim_right(s.substr(begin), blanks);
}

/**
 * @brief Where a marker stands on its line: not in a comment, alone on
 * its line after the comment opener, or after code.
 */
enum class placement : std::uint8_t { not_comment, alone, trailing };

placement placement_of(std::string_view path, std::string_view before) {
    const auto opened = trim_right(before, blanks);
    if (path.ends_with(".org")) {
        const auto text = trim(before);
        return text == "#" && opened.size() < before.size()
            ? placement::alone : placement::not_comment;
    }
    constexpr std::array<std::string_view, 6> openers{
        "//", "/*", "--", "#", ";", "<!--"};
    const auto opener = std::ranges::find_if(openers,
        [&](std::string_view o) { return opened.ends_with(o); });
    if (opener == openers.end())
        return placement::not_comment;
    const auto code = opened.substr(0, opened.size() - opener->size());
    return code.find_first_not_of(" \t/*#-;!<") == std::string_view::npos
        ? placement::alone : placement::trailing;
}

bool starts_with_folded(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && std::ranges::equal(
        s.substr(0, prefix.size()), prefix, {},
        [](char c) {
            return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
        });
}

/**
 * @brief Whether the lines of an org file lie inside a block, read
 * forward: a line from #+begin_ up to its #+end_, in any case, belongs to
 * the block, not to the org document.
 */
class org_blocks {
public:
    explicit org_blocks(std::string_view text) : text_(text) {}

    [[nodiscard]] bool inside(std::size_t line_begin) {
        while (read_ < line_begin) {
            auto end = text_.find('\n', read_);
            if (end == std::string_view::npos)
                end = text_.size();
            const auto line = trim(text_.substr(read_, end - read_));
            if (starts_with_folded(line, "#+begin_"))
                inside_ = true;
            else if (starts_with_folded(line, "#+end_"))
                inside_ = false;
            read_ = end + 1;
        }
        return inside_;
    }

private:
    std::string_view text_;
    std::size_t read_ = 0;
    bool inside_ = false;
};

bool is_rule_name(std::string_view rule) {
    const auto slash = rule.find('/');
    if (slash == 0 || slash == std::string_view::npos ||
        slash + 1 == rule.size())
        return false;
    return std::ranges::all_of(rule, [](char c) {
        return c == '/' || c == '_' || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9');
    }) && std::ranges::count(rule, '/') == 1;
}

/**
 * @brief The exception that @p text, what follows the marker up to the
 * end of its line, writes, or why it is malformed.
 */
std::variant<ct::exception, std::string> parse_one(std::string_view text) {
    ct::exception e;
    if (text.starts_with("-file")) {
        e.scope = ct::exception_scope::file;
        text.remove_prefix(5);
    }
    if (!text.starts_with('('))
        return std::string("the exception names no rule in parentheses");
    const auto close = text.find(')');
    if (close == std::string_view::npos)
        return std::string("the rule of the exception has no closing "
                           "parenthesis");
    e.rule = trim(text.substr(1, close - 1));
    if (!is_rule_name(e.rule))
        return std::format("{} is not a rule name of the form codec/rule",
            e.rule.empty() ? "an empty name" : e.rule);
    text.remove_prefix(close + 1);
    bool has_expires = false;
    while (true) {
        text = text.substr(std::min(text.size(),
            text.find_first_not_of(blanks)));
        if (text.empty())
            break;
        if (text.starts_with("reason=")) {
            auto reason = trim(text.substr(7));
            for (const auto closer : {"*/", "-->"})
                if (reason.ends_with(closer))
                    reason = trim(reason.substr(0,
                        reason.size() - std::string_view(closer).size()));
            e.reason = reason;
            break;
        }
        const auto token = text.substr(0, text.find_first_of(blanks));
        text.remove_prefix(token.size());
        const auto equals = token.find('=');
        if (equals == std::string_view::npos)
            return std::format("{} is not a field of the form key=value",
                token);
        const auto key = token.substr(0, equals);
        const auto value = token.substr(equals + 1);
        if (key == "expires") {
            if (has_expires)
                return std::string("the exception gives expires twice");
            has_expires = true;
            if (!value.empty()) {
                e.expires = parse_date(value);
                if (!e.expires)
                    return std::format("expires={} is not a date of the "
                                       "form YYYY-MM-DD", value);
            }
        } else if (key == "approver") {
            if (!e.approver.empty())
                return std::string("the exception gives approver twice");
            e.approver = value;
        } else {
            return std::format("{} is not a field of an exception; the "
                               "fields are expires, approver and reason",
                key);
        }
    }
    return e;
}

std::string date_text(std::chrono::year_month_day d) {
    return std::format("{:04}-{:02}-{:02}", static_cast<int>(d.year()),
        static_cast<unsigned>(d.month()), static_cast<unsigned>(d.day()));
}

}

std::optional<std::chrono::year_month_day> parse_date(
    std::string_view s) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-')
        return std::nullopt;
    const auto number = [&](std::size_t at, std::size_t n) {
        unsigned v = 0;
        const auto [end, ec] =
            std::from_chars(s.data() + at, s.data() + at + n, v);
        return ec == std::errc{} && end == s.data() + at + n
            ? std::optional<unsigned>(v) : std::nullopt;
    };
    const auto y = number(0, 4);
    const auto m = number(5, 2);
    const auto d = number(8, 2);
    if (!y || !m || !d)
        return std::nullopt;
    const std::chrono::year_month_day date{
        std::chrono::year(static_cast<int>(*y)), std::chrono::month(*m),
        std::chrono::day(*d)};
    return date.ok() ? std::optional(date) : std::nullopt;
}

ct::exception_set parse_exceptions(std::string_view path,
    std::string_view text) {
    ct::exception_set out;
    auto at = text.find(exception_marker);
    if (at == std::string_view::npos)
        return out;

    std::uint32_t line = 1;
    std::size_t counted = 0;
    std::vector<std::uint32_t> alone;
    std::vector<std::size_t> to_cover;
    const bool org = path.ends_with(".org");
    org_blocks blocks(text);
    for (; at != std::string_view::npos;
         at = text.find(exception_marker, at + 1)) {
        line += static_cast<std::uint32_t>(
            std::count(text.begin() + counted, text.begin() + at, '\n'));
        counted = at;
        const auto begin = text.rfind('\n', at);
        const auto line_begin = begin == std::string_view::npos ? 0 : begin + 1;
        auto line_end = text.find('\n', at);
        if (line_end == std::string_view::npos)
            line_end = text.size();
        if (org && blocks.inside(line_begin))
            continue;
        const auto where = placement_of(path,
            text.substr(line_begin, at - line_begin));
        if (where == placement::not_comment)
            continue;
        if (where == placement::alone)
            alone.push_back(line);
        const auto after = at + exception_marker.size();
        auto parsed = parse_one(trim_right(
            text.substr(after, line_end - after), " \t\r"));
        if (auto* why = std::get_if<std::string>(&parsed)) {
            out.malformed.push_back({std::string(path), line,
                std::move(*why)});
            continue;
        }
        auto& e = std::get<ct::exception>(parsed);
        e.written_in = path;
        e.line = line;
        e.path = path;
        if (e.scope == ct::exception_scope::line) {
            e.covers = line;
            if (where == placement::alone)
                to_cover.push_back(out.exceptions.size());
        }
        out.exceptions.push_back(std::move(e));
    }
    for (const auto i : to_cover) {
        auto& covers = out.exceptions[i].covers;
        do
            ++covers;
        while (std::ranges::binary_search(alone, covers));
    }
    return out;
}

void add_exception_row(ct::exception_set& out, std::string_view written_in,
    std::uint32_t line, const exception_row& row) {
    const auto malformed = [&](std::string why) {
        out.malformed.push_back({std::string(written_in), line,
            std::move(why)});
    };
    const auto written_path = trim(row.path);
    if (written_path.empty())
        return malformed("the row names no path");
    const std::filesystem::path normal =
        std::filesystem::path(written_path).lexically_normal();
    const auto path = normal.generic_string();
    if (normal.has_root_path() || path == ".." || path.starts_with("../"))
        return malformed(std::format("{} is not a path inside the checkout",
            written_path));

    ct::exception e;
    e.written_in = written_in;
    e.line = line;
    e.path = path;
    if (const auto covers = trim(row.line); covers.empty()) {
        e.scope = ct::exception_scope::file;
    } else {
        const auto [end, ec] = std::from_chars(covers.data(),
            covers.data() + covers.size(), e.covers);
        if (ec != std::errc{} || end != covers.data() + covers.size() ||
            e.covers == 0)
            return malformed(std::format("line {} is not a line number; "
                "leave it empty for the whole file", covers));
    }
    e.rule = trim(row.rule);
    if (!is_rule_name(e.rule))
        return malformed(std::format("{} is not a rule name of the form "
            "codec/rule", e.rule.empty() ? "an empty name" : e.rule));
    if (const auto expires = trim(row.expires); !expires.empty()) {
        e.expires = parse_date(expires);
        if (!e.expires)
            return malformed(std::format("expires {} is not a date of the "
                "form YYYY-MM-DD", expires));
    }
    e.approver = trim(row.approver);
    e.reason = trim(row.reason);
    out.exceptions.push_back(std::move(e));
}

void append(ct::exception_set& to, ct::exception_set&& from) {
    std::ranges::move(from.exceptions, std::back_inserter(to.exceptions));
    std::ranges::move(from.malformed, std::back_inserter(to.malformed));
}

rule_status status_of(const ct::registry& codecs, std::string_view rule) {
    const auto slash = rule.find('/');
    const auto codec = rule.substr(0, slash);
    const auto name = slash == std::string_view::npos
        ? std::string_view{} : rule.substr(slash + 1);
    for (std::size_t c = 0; c < codecs.size(); ++c) {
        const auto& d = codecs.codecs[c];
        if (d.name != codec)
            continue;
        const bool declared = (output_of(d) == ct::output_kind::files &&
                name == drift_rule.name) ||
            std::ranges::any_of(d.rules,
                [&](const ct::rule& r) { return r.name == name; });
        if (!declared)
            return rule_status::unknown;
        return codecs.enabled[c] ? rule_status::enabled
                                 : rule_status::disabled;
    }
    return rule_status::unknown;
}

excusal excuse(std::span<const finding_site> sites,
    const ct::exception_set& set, const ct::registry& codecs,
    std::chrono::year_month_day today) {
    excusal out;
    out.excused.assign(sites.size(), 0);

    const auto problem = [&](std::string_view path, std::uint32_t line,
                             exception_rule kind, std::string message,
                             std::string fix) {
        out.findings.push_back({std::string(path), line,
            std::format("{}/{}", exception_codec, rule_of(kind).name),
            std::move(message), std::move(fix)});
    };

    for (const auto& m : set.malformed)
        problem(m.written_in, m.line, exception_rule::malformed, m.why,
            std::string(written_as));

    const auto& all = set.exceptions;
    std::vector<std::uint8_t> valid(all.size(), 0);
    std::vector<std::uint8_t> used(all.size(), 0);
    std::unordered_map<std::string_view, std::vector<std::size_t>> by_path;
    for (std::size_t i = 0; i < all.size(); ++i) {
        const auto& e = all[i];
        const auto status = is_exception_rule(e.rule)
            ? rule_status::unknown : status_of(codecs, e.rule);
        if (is_exception_rule(e.rule)) {
            problem(e.written_in, e.line, exception_rule::malformed,
                std::format("{} is a finding about exceptions, which no "
                    "exception excuses", e.rule),
                "fix the exception it reports");
        } else if (status == rule_status::unknown) {
            problem(e.written_in, e.line, exception_rule::malformed,
                std::format("the exception names {}, a rule no codec "
                    "declares", e.rule),
                "name the rule as the finding does, codec/rule");
        } else if (e.reason.empty() || e.approver.empty() || !e.expires) {
            std::string missing;
            for (const auto& [absent, field] : {
                     std::pair{e.reason.empty(), "a reason"},
                     std::pair{e.approver.empty(), "an approver"},
                     std::pair{!e.expires.has_value(), "an expiry"}})
                if (absent)
                    missing += (missing.empty() ? "" : ", ") +
                        std::string(field);
            problem(e.written_in, e.line, exception_rule::incomplete,
                std::format("the exception for {} lacks {}", e.rule,
                    missing),
                std::string(written_as));
        } else if (std::chrono::sys_days(*e.expires) <
                   std::chrono::sys_days(today)) {
            // A comment beside the line it covers is reported there, so
            // the expiry shows next to the finding it no longer excuses.
            const bool beside = e.scope == ct::exception_scope::line &&
                e.written_in == e.path;
            problem(e.written_in, beside ? e.covers : e.line,
                exception_rule::expired, std::format(
                "the exception for {} approved by {} expired on {}",
                e.rule, e.approver, date_text(*e.expires)),
                std::format("fix the finding, or renew the exception "
                            "on line {} with a new expiry and approver",
                    e.line));
        } else {
            valid[i] = status == rule_status::enabled;
        }
        if (valid[i])
            by_path[e.path].push_back(i);
    }

    for (std::size_t s = 0; s < sites.size(); ++s) {
        const auto& site = sites[s];
        const auto found = by_path.find(site.path);
        if (found == by_path.end())
            continue;
        for (const auto i : found->second) {
            const auto& e = all[i];
            if (e.rule == site.rule && (e.scope == ct::exception_scope::file
                    || e.covers == site.line)) {
                out.excused[s] = 1;
                used[i] = 1;
            }
        }
    }

    for (std::size_t i = 0; i < all.size(); ++i)
        if (valid[i] && !used[i])
            problem(all[i].written_in, all[i].line, exception_rule::stale,
                std::format("the exception for {} covers no finding",
                    all[i].rule), "remove the exception");

    std::ranges::stable_sort(out.findings, {},
        [](const exception_finding& f) { return std::tie(f.path, f.line); });
    return out;
}
}
