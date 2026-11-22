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
#include "compass.docs/codecs/rule_identifiers.hpp"

#include <array>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "compass.codec/ops/pages.hpp"
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::docs::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

enum rule : std::uint32_t { unkeyed_section, no_identifier, foreign_code,
    duplicate, bad_citation, unreadable };

constexpr std::array<std::string_view, 1> ux_hub_sources{
    "00BA1689-B060-4BD8-BC46-67034D663B7F"};

constexpr std::array<ct::rule, 6> rules{{
    {"unkeyed_section", "No rule carries an identifier in a section the "
        "pages do not key.", {}, ct::blame_method::line},
    {"no_identifier", "Every rule of a keyed section starts with its "
        "identifier.", ux_hub_sources, ct::blame_method::line},
    {"foreign_code", "A rule's identifier carries its own page's code.",
        {}, ct::blame_method::line},
    {"duplicate", "No identifier is on two rules.", ux_hub_sources,
        ct::blame_method::line},
    {"bad_citation", "Every checklist line cites two-digit identifiers a "
        "page carries, or a page that carries none yet.",
        ux_hub_sources, ct::blame_method::line},
    {"unreadable", "The options name each keyed page and the checklist by "
        "its :ID:, and a document has each ID."}}};

using sections = std::vector<std::pair<std::string_view, std::string_view>>;

/**
 * @brief A keyed page: its code, which is also the option that names it
 * by :ID:, and the code of each level-two section whose bullets are rules.
 */
struct page {
    std::string_view code;
    sections keyed;
};

const std::array<page, 2>& pages() {
    static const std::array<page, 2> out{{
        {"RS", {
            {"Where this standard applies", "SCOPE"},
            {"What a record kind declares", "DECL"},
            {"The layers", "LAYER"}, {"Addresses", "ADDR"},
            {"The list", "LIST"}, {"The record page", "PAGE"},
            {"Writes", "WRITE"}, {"History", "HIST"},
            {"Related records", "RELATED"}, {"Import and Export", "ORE"},
            {"Flags and images", "FLAG"}, {"Icons and cells", "ICON"},
            {"Feedback, access and safety", "FEEDBACK"},
            {"Naming", "NAME"}, {"Exceptions", "EXCEPT"},
            {"Building a record screen", "BUILD"}}},
        {"ES", {
            {"User journeys and screens", "JOURNEY"},
            {"The shape of an entity screen", "SHAPE"},
            {"The list", "LIST"}, {"The page", "PAGE"},
            {"Changing a record", "EDIT"},
            {"References are links", "REF"},
            {"Workflows on the entity screen", "WORKFLOW"},
            {"People are links", "PEOPLE"},
            {"Photos, flags and images", "PHOTO"},
            {"Paging: the pager is the request", "PAGING"},
            {"History and the timeline", "HIST"},
            {"Several kinds of record in one entity", "ASPECT"},
            {"A trade, a structure and a package", "TRADE"},
            {"The entity protocol", "PROTOCOL"},
            {"Relationship views", "RELATION"},
            {"Who sees what", "SEES"},
            {"Proposals and four-eyes", "PROPOSE"},
            {"The shell around a screen", "SHELL"},
            {"Feedback, states and words", "WORDS"},
            {"Updates and refresh", "REFRESH"},
            {"What a screen or a form needs, and what the log keeps",
                "NEEDS"},
            {"Verifying a rule", "VERIFY"}}}}};
    return out;
}

constexpr std::array<std::string_view, 5> unkeyed_pages{"UXL", "UDP", "CFG",
    "FSS", "OLS"};

std::vector<std::string_view> lines_of(std::string_view text) {
    std::vector<std::string_view> out;
    for (std::size_t start = 0;;) {
        const auto nl = text.find('\n', start);
        out.push_back(text.substr(start, nl == std::string_view::npos
                ? std::string_view::npos : nl - start));
        if (nl == std::string_view::npos)
            return out;
        start = nl + 1;
    }
}

/**
 * @brief A bullet of a level-two section: its title, its section code if
 * the page keys it, and its 0-based line.
 */
struct bullet {
    std::string title;
    std::string_view code;
    std::size_t line;
};

std::vector<bullet> rules_of(const page& p, std::string_view text,
    const std::vector<std::string_view>& lines) {
    static const pt::regex heading(R"(^(\*+) (.+?)\s*$)", {.multiline = true});
    static const pt::regex bullet_line(
        R"(^(?:- (?!\[ \])|\*(?:RS|ES)-[A-Z]+-(?:\d{2}|\?\?)\* )(.*)$)");
    std::vector<std::size_t> offsets;
    std::size_t pos = 0;
    for (const auto line : lines) {
        offsets.push_back(pos);
        pos += line.size() + 1;
    }
    struct top {
        std::size_t start;
        std::size_t level;
        std::string title;
    };
    std::vector<top> tops;
    for (const auto& m : heading.find_all(text)) {
        if (m.group(1).size() <= 2)
            tops.push_back({m.start(), m.group(1).size(),
                std::string(m.group(2))});
    }
    std::vector<bullet> out;
    for (std::size_t t = 0; t < tops.size(); ++t) {
        if (tops[t].level != 2)
            continue;
        const auto start = tops[t].start;
        const auto end = t + 1 < tops.size() ? tops[t + 1].start : text.size();
        std::string_view code;
        for (const auto& [title, c] : p.keyed) {
            if (title == tops[t].title)
                code = c;
        }
        bool in_block = false;
        for (std::size_t i = 0; i < lines.size(); ++i) {
            if (offsets[i] < start || offsets[i] >= end)
                continue;
            if (lines[i].starts_with("#+begin_"))
                in_block = true;
            else if (lines[i].starts_with("#+end_"))
                in_block = false;
            if (in_block)
                continue;
            if (bullet_line.match(lines[i]))
                out.push_back({tops[t].title, code, i});
        }
    }
    return out;
}

constexpr std::array<ct::option, 3> options{{
    {"RS", "", "The :ID: of the record screen standard, whose rules carry "
        "RS- identifiers."},
    {"ES", "", "The :ID: of the entity screen specification, whose rules "
        "carry ES- identifiers."},
    {"checklist", "", "The :ID: of the screen review checklist, whose lines "
        "cite the identifiers."}}};

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    static const pt::regex start(R"(^(?:- )?\*((?:RS|ES)-[A-Z]+-\d{2})\* )");
    static const pt::regex token(R"(\b((?:RS|ES)-[A-Z]+-\d+)\b)");
    static const pt::regex two_digits(R"((?:RS|ES)-[A-Z]+-\d{2})");
    const auto report = [&](rule r, std::string_view path,
        std::string_view bytes, std::size_t line, const std::string& message) {
        std::uint32_t offset = 0;
        for (std::size_t l = 0; l < line; ++l) {
            const auto nl = bytes.find('\n', offset);
            offset = nl == std::string_view::npos
                ? static_cast<std::uint32_t>(bytes.size())
                : static_cast<std::uint32_t>(nl + 1);
        }
        co::report(out, path, offset, r, message, "Run "
            "build/scripts/check_rule_identifiers.py --assign for a rule "
            "without an identifier; cite an identifier on the checklist line "
            "it names.");
    };
    std::vector<std::pair<const page*, ct::document>> keyed;
    bool configured = true;
    const auto resolve = [&](std::string_view key) {
        auto found = compass::codec::ops::page_of(v, key);
        if (!found) {
            configured = false;
            co::report(out, ".compass.org", 0, unreadable,
                "rule_identifiers: " + found.error(), "Set the option in "
                "the codec's row of .compass.org to the :ID: of the page.");
            return std::optional<ct::document>{};
        }
        return std::optional<ct::document>{*found};
    };
    for (const auto& p : pages())
        if (auto d = resolve(p.code))
            keyed.emplace_back(&p, *d);
    const auto listing = resolve("checklist");
    if (!configured)
        return;

    std::map<std::string, std::string> seen;
    for (const auto& [pp, d] : keyed) {
        const auto& p = *pp;
        const auto path = d.path;
        const auto bytes = d.source;
        const auto text = pt::universal_newlines(pt::decode_replacing(bytes));
        const auto lines = lines_of(text);
        for (const auto& b : rules_of(p, text, lines)) {
            const auto line = lines[b.line];
            const auto m = start.match(line);
            if (b.code.empty()) {
                if (m)
                    report(unkeyed_section, path, bytes, b.line,
                        "a rule under '" + b.title + "', a section this "
                        "check does not key");
                continue;
            }
            if (!m) {
                report(no_identifier, path, bytes, b.line, "a rule without "
                    "an identifier at its start under '" + b.title + "'");
                continue;
            }
            const std::string ident(m->group(1));
            if (!ident.starts_with(std::string(p.code) + "-"))
                report(foreign_code, path, bytes, b.line, ident +
                    " carries another page's code");
            if (const auto it = seen.find(ident); it != seen.end())
                report(duplicate, path, bytes, b.line, ident + " is also "
                    "on a rule of " + it->second);
            seen[ident] = path;
        }
    }
    const auto checklist = listing->path;
    const auto bytes = listing->source;
    const auto text = pt::universal_newlines(pt::decode_replacing(bytes));
    const auto lines = lines_of(text);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const auto line = lines[i];
        if (!line.starts_with("- [ ]"))
            continue;
        std::vector<std::string> cited;
        for (const auto& m : token.find_all(line))
            cited.emplace_back(m.group(1));
        for (const auto& t : cited) {
            if (!two_digits.fullmatch(t))
                report(bad_citation, checklist, bytes, i, t + " is not an "
                    "identifier; two digits are required");
        }
        bool unkeyed = false;
        for (const auto u : unkeyed_pages) {
            const std::string name(u);
            unkeyed = unkeyed || line.contains("(" + name + " ") ||
                line.contains("; " + name + " ") ||
                line.contains(", " + name + " ");
        }
        if (cited.empty() && !unkeyed)
            report(bad_citation, checklist, bytes, i, "a checklist line that "
                "cites no identifier");
        for (const auto& ident : cited) {
            if (!seen.contains(ident))
                report(bad_citation, checklist, bytes, i, "cites " + ident +
                    ", which no page carries");
        }
    }
}

}

const ct::descriptor rule_identifiers{
    .name = "rule_identifiers",
    .version = 2,
    .summary = "Reports each UX rule without its one identifier, and each "
        "checklist line whose citations do not resolve.",
    .reads = ct::inputs::every_document(),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}
