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
#include "compass.cli/ops/provenance.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <format>
#include <map>
#include <stdexcept>
#include "compass.codec/ops/data_tables.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::cli::ops {

namespace {

namespace ct = compass::codec::types;

constexpr std::array<ct::rule, 8> rule_list{{
    {"missing_trailer", "Every commit of the branch, merges aside, carries "
        "each trailer the Trailers table of the check document names, unless "
        "an exemption lets its subject omit it."},
    {"malformed_trailer", "A trailer that names a document holds an "
        "uppercase UUID, and any other trailer's value matches its pattern; "
        "so does each value of the pull request's table."},
    {"unknown_document", "A trailer that names a document names one of the "
        "checkout: a document whose :ID: is the value."},
    {"wrong_document_type", "The document a trailer names has the #+type: "
        "the Trailers table gives it, a story for a story's ID."},
    {"pull_request_table", "The branch's pull request states, in its body's "
        "table, a row for each trailer the Trailers table labels, with the "
        "value in the column it names."},
    {"differs_from_pull_request", "A commit's trailer equals the value the "
        "pull request's table states; a warning, since a branch may touch a "
        "second task."},
    {"no_pull_request", "When require_pull_request is true, the branch has a "
        "pull request compass can read with gh."},
    {"unreadable", "The check document of commit_provenance gives a Trailers "
        "table that reads: one key per row, a document type or a pattern "
        "that compiles, and a column for each label."}}};

constexpr std::string_view missing_trailer = "missing_trailer";
constexpr std::string_view malformed_trailer = "malformed_trailer";
constexpr std::string_view unknown_document = "unknown_document";
constexpr std::string_view wrong_document_type = "wrong_document_type";
constexpr std::string_view pull_request_table = "pull_request_table";
constexpr std::string_view differs = "differs_from_pull_request";
constexpr std::string_view no_pull_request = "no_pull_request";

std::string_view trim(std::string_view s) {
    const auto blank = [](char c) {
        return c == ' ' || c == '\t' || c == '\r';
    };
    while (!s.empty() && blank(s.front()))
        s.remove_prefix(1);
    while (!s.empty() && blank(s.back()))
        s.remove_suffix(1);
    return s;
}

bool same_name(std::string_view a, std::string_view b) {
    return std::ranges::equal(a, b, [](char x, char y) {
        return std::tolower(static_cast<unsigned char>(x)) ==
            std::tolower(static_cast<unsigned char>(y));
    });
}

/**
 * @brief The lines of @p text, without their line ends.
 */
std::vector<std::string_view> lines_of(std::string_view text) {
    std::vector<std::string_view> out;
    while (!text.empty()) {
        const auto end = text.find('\n');
        out.push_back(text.substr(0, end));
        text = end == std::string_view::npos ? std::string_view{}
                                             : text.substr(end + 1);
    }
    return out;
}

/**
 * @brief The key of the trailer line @p line, or nothing when it is not
 * one: a token of letters, digits and dashes, then a colon.
 */
std::optional<std::string_view> trailer_key(std::string_view line) {
    const auto colon = line.find(':');
    if (colon == 0 || colon == std::string_view::npos)
        return std::nullopt;
    const auto key = line.substr(0, colon);
    if (!std::ranges::all_of(key, [](char c) {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '-';
        }))
        return std::nullopt;
    return key;
}

bool is_uuid(std::string_view s) {
    if (s.size() != 36)
        return false;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const bool dash = i == 8 || i == 13 || i == 18 || i == 23;
        const char c = s[i];
        if (dash ? c != '-'
                 : !(std::isdigit(static_cast<unsigned char>(c)) ||
                       (c >= 'A' && c <= 'F')))
            return false;
    }
    return true;
}

std::string rule_name(std::string_view rule) {
    return std::format("{}/{}", provenance_codec, rule);
}

/**
 * @brief Collects the findings of one check of provenance.
 */
struct reporter {
    std::vector<types::finding>& out;

    void add(std::string path, std::uint32_t line, std::string_view rule,
        std::string message, std::string fix,
        codec::types::severity level = codec::types::severity::error) {
        out.push_back({.path = std::move(path),
            .line = line == 0 ? 1 : line,
            .column = 1,
            .rule = rule_name(rule),
            .level = level,
            .message = std::move(message),
            .fix = std::move(fix),
            .drift = {}});
    }
};

/**
 * @brief A trailer rule with its pattern compiled.
 */
struct compiled_rule {
    const types::trailer_rule* rule;
    std::optional<platform::text::regex> pattern;
};

/**
 * @brief Reports @p value, which @p what holds at @p line of @p path, when
 * it is not what @p r asks: an uppercase UUID that names a document of
 * @p documents of its type, or a value its pattern matches.
 */
void check_value(const compiled_rule& r, std::string_view value,
    std::string_view what, const std::string& path, std::uint32_t line,
    const types::document_ids& documents, reporter& report) {
    const auto& rule = *r.rule;
    if (rule.type.empty()) {
        if (!r.pattern->fullmatch(value))
            report.add(path, line, malformed_trailer, std::format("{} is "
                "\"{}\", which does not match {}", what, value, rule.pattern),
                std::format("write a value the pattern {} matches",
                    rule.pattern));
        return;
    }
    if (!is_uuid(value)) {
        report.add(path, line, malformed_trailer, std::format("{} is \"{}\", "
            "which is not an uppercase UUID", what, value), std::format(
            "write the :ID: of the {} the work belongs to", rule.type));
        return;
    }
    const auto it = documents.find(std::string(value));
    if (it == documents.end()) {
        report.add(path, line, unknown_document, std::format("{} is {}, "
            "and no document of the checkout has that :ID:", what, value),
            std::format("write the :ID: of a {} of this checkout",
                rule.type));
        return;
    }
    if (it->second.type != rule.type)
        report.add(path, line, wrong_document_type, std::format("{} is {}, "
            "which names {}, a {}, not a {}", what, value, it->second.path,
            it->second.type.empty() ? "document of no type" : it->second.type,
            rule.type), std::format("write the :ID: of a {}", rule.type));
}

/**
 * @brief Whether a commit whose subject is @p subject may omit
 * @p trailer.
 */
bool may_omit(const std::vector<std::pair<platform::text::regex,
                  const types::exemption*>>& exemptions,
    std::string_view subject, std::string_view trailer) {
    return std::ranges::any_of(exemptions, [&](const auto& e) {
        return e.first.search(subject) &&
            std::ranges::any_of(e.second->omits, [&](const auto& o) {
                return same_name(o, trailer);
            });
    });
}

std::string short_id(std::string_view id) {
    return std::string(id.substr(0, 12));
}

}

std::span<const codec::types::rule> provenance_rule_list() noexcept {
    return rule_list;
}

std::vector<types::trailer> trailers_of(std::string_view message) {
    const auto lines = lines_of(message);
    auto end = lines.size();
    while (end > 0 && trim(lines[end - 1]).empty())
        --end;
    auto begin = end;
    while (begin > 0 && !trim(lines[begin - 1]).empty())
        --begin;
    const auto subject = std::ranges::find_if(lines,
        [](std::string_view l) { return !trim(l).empty(); }) - lines.begin();
    if (begin == end || static_cast<std::ptrdiff_t>(begin) <= subject)
        return {};
    std::vector<types::trailer> out;
    for (auto i = begin; i < end; ++i) {
        const auto line = lines[i];
        if (line.front() == ' ' || line.front() == '\t') {
            if (out.empty())
                return {};
            out.back().value += " " + std::string(trim(line));
            continue;
        }
        const auto key = trailer_key(line);
        if (!key)
            return {};
        out.push_back({std::string(*key),
            std::string(trim(line.substr(key->size() + 1))),
            static_cast<std::uint32_t>(i + 1)});
    }
    return out;
}

std::expected<types::provenance_rules, std::string> provenance_rules_of(
    std::string_view data, std::string_view require) {
    static constexpr std::array<std::string_view, 5> trailer_columns{
        "trailer", "document type", "pattern", "pull request label",
        "pull request column"};
    static constexpr std::array<std::string_view, 2> exemption_columns{
        "subject", "omits"};
    static constexpr std::array<codec::ops::table_need, 1> needed{{
        {"Trailers", trailer_columns}}};
    auto tables = codec::ops::tables_for(provenance_codec, data, needed);
    if (!tables)
        return std::unexpected(tables.error());
    types::provenance_rules out;
    out.source = tables->source;
    if (require == "true")
        out.require_pull_request = true;
    else if (require != "false")
        return std::unexpected(std::format("{}.require_pull_request is \"{}\"; "
            "write true or false", provenance_codec, require));

    const auto at = [&](std::uint32_t line) {
        return std::format("{}:{}: ", out.source, line);
    };
    const auto& t = *tables->find("Trailers");
    if (t.rows.empty())
        return std::unexpected(at(t.line) + "the Trailers table names no "
            "trailer; name one per row, or disable commit_provenance");
    std::array<std::size_t, trailer_columns.size()> c{};
    for (std::size_t i = 0; i < c.size(); ++i)
        c[i] = t.column(trailer_columns[i]);
    for (std::size_t r = 0; r < t.rows.size(); ++r) {
        const auto& row = t.rows[r];
        const auto where = at(t.row_lines[r]);
        types::trailer_rule rule{.trailer = row[c[0]], .type = row[c[1]],
            .pattern = row[c[2]], .label = row[c[3]], .column = 2,
            .line = t.row_lines[r]};
        if (!trailer_key(rule.trailer + ":"))
            return std::unexpected(where + "\"" + rule.trailer + "\" is no "
                "trailer key; write letters, digits and dashes, as Story-ID");
        if (std::ranges::any_of(out.trailers, [&](const auto& o) {
                return same_name(o.trailer, rule.trailer); }))
            return std::unexpected(where + rule.trailer + " is named twice; "
                "keep one row per trailer");
        if (rule.type.empty() == rule.pattern.empty())
            return std::unexpected(where + rule.trailer + " needs a document "
                "type or a pattern, and not both");
        if (!rule.pattern.empty()) {
            try {
                platform::text::regex compiled(rule.pattern);
            } catch (const std::invalid_argument& e) {
                return std::unexpected(where + "the pattern of " +
                    rule.trailer + " does not compile: " + e.what());
            }
        }
        if (!rule.label.empty()) {
            const std::string_view column = row[c[4]];
            std::size_t n = 0;
            const auto [end, ec] = std::from_chars(column.data(),
                column.data() + column.size(), n);
            if (ec != std::errc{} || end != column.data() + column.size() ||
                n < 2)
                return std::unexpected(where + "the pull request column of " +
                    rule.trailer + " is \"" + std::string(column) + "\"; write "
                    "the cell of the row that holds the value, 2 or more, the "
                    "label's own cell being 1");
            rule.column = n;
        }
        out.trailers.push_back(std::move(rule));
    }

    const auto* e = tables->find("Exemptions");
    if (e == nullptr)
        return out;
    for (const auto name : exemption_columns)
        if (e->column(name) == e->columns.size())
            return std::unexpected(at(e->line) + "the Exemptions table has "
                "no column " + std::string(name));
    const auto subject = e->column("subject");
    const auto omits = e->column("omits");
    for (std::size_t r = 0; r < e->rows.size(); ++r) {
        const auto& row = e->rows[r];
        const auto where = at(e->row_lines[r]);
        if (row.size() != e->columns.size())
            return std::unexpected(where + "the row's cells are not the "
                "table's columns");
        types::exemption x{.subject = row[subject], .omits = {}};
        try {
            platform::text::regex compiled(x.subject);
        } catch (const std::invalid_argument& err) {
            return std::unexpected(where + "the subject pattern does not "
                "compile: " + err.what());
        }
        for (auto rest = std::string_view(row[omits]);
             !(rest = trim(rest)).empty();) {
            const auto space = rest.find_first_of(" \t");
            const auto name = rest.substr(0, space);
            rest = space == std::string_view::npos ? std::string_view{}
                                                   : rest.substr(space);
            if (!std::ranges::any_of(out.trailers, [&](const auto& o) {
                    return same_name(o.trailer, name); }))
                return std::unexpected(where + std::string(name) + " is no "
                    "trailer of the Trailers table");
            x.omits.emplace_back(name);
        }
        if (x.omits.empty())
            return std::unexpected(where + "the exemption omits no trailer; "
                "name the trailers its commits may omit");
        out.exemptions.push_back(std::move(x));
    }
    return out;
}

std::vector<markdown_row> markdown_rows(std::string_view body) {
    std::vector<markdown_row> out;
    const auto lines = lines_of(body);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        auto line = trim(lines[i]);
        if (!line.starts_with('|'))
            continue;
        line.remove_prefix(1);
        if (line.ends_with('|'))
            line.remove_suffix(1);
        if (std::ranges::all_of(line, [](char c) {
                return c == '-' || c == ':' || c == '|' || c == ' ' ||
                    c == '+';
            }))
            continue;
        markdown_row row{.cells = {}, .line = static_cast<std::uint32_t>(i + 1)};
        for (auto rest = line;;) {
            const auto bar = rest.find('|');
            row.cells.emplace_back(trim(rest.substr(0, bar)));
            if (bar == std::string_view::npos)
                break;
            rest = rest.substr(bar + 1);
        }
        out.push_back(std::move(row));
    }
    return out;
}

types::provenance_result check_provenance(
    const types::provenance_rules& rules,
    std::span<const types::branch_commit> commits,
    const types::document_ids& documents,
    const std::optional<types::pull_request>& pr, std::string_view no_pr,
    std::string_view branch) {
    types::provenance_result r;
    r.checked = true;
    reporter report{r.findings};

    std::vector<compiled_rule> compiled;
    for (const auto& t : rules.trailers) {
        compiled.push_back({&t, std::nullopt});
        if (!t.pattern.empty())
            compiled.back().pattern.emplace(t.pattern);
    }
    std::vector<std::pair<platform::text::regex, const types::exemption*>>
        exemptions;
    for (const auto& e : rules.exemptions)
        exemptions.emplace_back(platform::text::regex(e.subject), &e);

    std::map<std::string_view, std::string> stated;
    if (pr) {
        r.pull_request = pr->number;
        const auto path = std::format("pull/{}", pr->number);
        const auto rows = markdown_rows(pr->body);
        for (const auto& c : compiled) {
            const auto& t = *c.rule;
            if (t.label.empty())
                continue;
            const auto row = std::ranges::find_if(rows, [&](const auto& x) {
                return !x.cells.empty() && same_name(x.cells.front(), t.label);
            });
            if (row == rows.end()) {
                report.add(path, 1, pull_request_table, std::format("the pull "
                    "request's table has no {} row", t.label), std::format(
                    "add | {} | ... | to the table of the pull request's "
                    "body, the value in cell {}", t.label, t.column));
                continue;
            }
            const auto value = t.column <= row->cells.size()
                ? std::string_view(row->cells[t.column - 1])
                : std::string_view{};
            if (value.empty()) {
                report.add(path, row->line, pull_request_table, std::format(
                    "the {} row of the pull request's table has no value in "
                    "cell {}", t.label, t.column), std::format("write the {} "
                    "value in cell {} of the row", t.trailer, t.column));
                continue;
            }
            check_value(c, value, std::format("the {} row of the pull "
                "request's table", t.label), path, row->line, documents,
                report);
            stated.emplace(t.trailer, value);
        }
    } else {
        r.note = std::string(no_pr);
        if (rules.require_pull_request)
            report.add(std::format("branch/{}", branch.empty() ? "HEAD"
                                                               : branch),
                1, no_pull_request, std::format("the branch has no pull "
                "request compass can read: {}", no_pr), "open the pull "
                "request, or make gh able to read it");
    }

    for (const auto& commit : commits) {
        if (commit.parents > 1) {
            ++r.merges;
            continue;
        }
        ++r.commits;
        const auto path = "commit/" + short_id(commit.id);
        for (const auto& c : compiled) {
            const auto& t = *c.rule;
            const auto it = std::ranges::find_if(commit.trailers,
                [&](const auto& x) { return same_name(x.key, t.trailer); });
            if (it == commit.trailers.end()) {
                if (!may_omit(exemptions, commit.subject, t.trailer))
                    report.add(path, 1, missing_trailer, std::format("the "
                        "commit \"{}\" has no {} trailer", commit.subject,
                        t.trailer), std::format("reword the commit to end "
                        "its message with the trailer line {}: <value>",
                        t.trailer));
                continue;
            }
            check_value(c, it->value, std::format("the {} trailer",
                t.trailer), path, it->line, documents, report);
            const auto pr_value = stated.find(t.trailer);
            if (pr_value != stated.end() && pr_value->second != it->value)
                report.add(path, it->line, differs, std::format("the {} "
                    "trailer is {}, and the pull request's {} row states {}",
                    t.trailer, it->value, t.label, pr_value->second),
                    std::format("if the commit belongs to the pull request's "
                    "{}, reword its {} trailer", t.label, t.trailer),
                    codec::types::severity::warning);
        }
    }
    return r;
}

}
