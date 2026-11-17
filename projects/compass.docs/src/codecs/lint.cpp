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
#include "compass.docs/codecs/lint.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codec/types/fact.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::docs::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

constexpr std::uint32_t bad_filetag = 0;
constexpr std::uint32_t unclosed_marker = 1;
constexpr std::uint32_t marker_without_writer = 2;
constexpr std::uint32_t malformed_link = 3;
constexpr std::uint32_t dangling_link = 4;
constexpr std::uint32_t durable_link = 5;

constexpr std::array<ct::rule, 6> rules{{
    {"bad_filetag", "Every tag of a page's first #+filetags: line is "
        "lowercase: a letter, then letters, digits, _, . or -."},
    {"unclosed_marker", "Every '# BEGIN generated X' has a matching "
        "'# END generated X' on its own line."},
    {"marker_without_writer", "A '# BEGIN generated X (script)' marker "
        "names a script that exists and writes the page."},
    {"malformed_link", "An id link whose target has a hyphen is a UUID."},
    {"dangling_link", "Every id link names the :ID: of some page."},
    {"durable_link", "A page under doc/llm, doc/meta, doc/knowledge or "
        "doc/recipes links no agile page."}}};

constexpr std::array<ct::option, 1> options{{
    {"strict_links", "false", "true reports durable_link, which the gate "
        "only advises on without --strict-links."}}};

constexpr std::array<std::string_view, 3> excluded_first{"build", "venv",
    ".git"};
constexpr std::array<std::string_view, 4> excluded_root{"build",
    ".packages", "vcpkg", "external"};
constexpr std::array<std::string_view, 4> excluded_anywhere{".git",
    ".claude", "node_modules", "__pycache__"};
constexpr std::array<std::string_view, 8> agile_types{"story", "task",
    "sprint", "design", "capture", "version", "plan", "retrospective"};
constexpr std::array<std::string_view, 4> durable_dirs{"doc/llm/",
    "doc/meta/", "doc/knowledge/", "doc/recipes/"};

template <std::size_t N>
bool holds(const std::array<std::string_view, N>& set, std::string_view s) {
    return std::ranges::find(set, s) != set.end();
}

bool is_linted(std::string_view path) {
    const auto parts = co::components_of(path);
    if (parts.empty() || parts.back().starts_with('.'))
        return false;
    if (holds(excluded_first, parts.front()) ||
        (parts.size() > 1 && holds(excluded_root, parts.front())))
        return false;
    for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
        if (holds(excluded_anywhere, parts[i]))
            return false;
    }
    return true;
}

bool is_durable(std::string_view path) {
    return std::ranges::any_of(durable_dirs, [&](auto d) {
        return path.starts_with(d);
    });
}

std::string upper(std::string_view s) {
    std::string out(s);
    for (auto& c : out) {
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
    }
    return out;
}

/**
 * @brief The first @p n characters of @p text, as Python's [:n] slices.
 */
std::string first_characters(std::string_view text, std::size_t n) {
    return std::string(text.substr(0, co::byte_of_character(text, n)));
}

std::string one_line(std::string_view text) {
    std::string out(text);
    std::ranges::replace(out, '\n', ' ');
    return out;
}

/**
 * @brief The path a marker names, relative to the checkout, with . and
 * empty components dropped and .. taken back; nothing when it climbs out
 * of the checkout or is absolute.
 */
std::optional<std::string> checkout_path(std::string_view named) {
    if (named.starts_with('/'))
        return std::nullopt;
    std::vector<std::string_view> kept;
    std::size_t start = 0;
    while (start <= named.size()) {
        auto end = named.find('/', start);
        if (end == std::string_view::npos)
            end = named.size();
        const auto part = named.substr(start, end - start);
        if (part == "..") {
            if (kept.empty())
                return std::nullopt;
            kept.pop_back();
        } else if (!part.empty() && part != ".") {
            kept.push_back(part);
        }
        start = end + 1;
    }
    std::string out;
    for (const auto part : kept)
        out += (out.empty() ? "" : "/") + std::string(part);
    return out;
}

std::vector<std::string_view> pages(const ct::corpus_view& v) {
    auto out = v.list_files("", ".org");
    std::erase_if(out, [](auto p) { return !is_linted(p); });
    return out;
}

void check_filetags(std::string_view path, std::string_view source,
    const std::vector<std::string_view>& lines, ct::file_scan& out) {
    static const pt::regex filetags(R"(#\+filetags:\s*:(.*):\s*$)");
    static const pt::regex tag(R"(^[a-z][a-z0-9_.-]*$)");
    for (std::uint32_t i = 0; i < lines.size(); ++i) {
        if (!lines[i].starts_with("#+filetags:"))
            continue;
        const auto m = filetags.match(co::strip(lines[i]));
        if (!m)
            return;
        const auto raw = m->group(1);
        std::size_t start = 0;
        while (start <= raw.size()) {
            auto end = raw.find(':', start);
            if (end == std::string_view::npos)
                end = raw.size();
            const auto t = raw.substr(start, end - start);
            if (!co::strip(t).empty() && !tag.match(t))
                co::report(out.findings, path, co::offset_of_line(source,
                    i + 1), bad_filetag, "unknown/malformed tag '" +
                    std::string(t) + "'", "Write the tag in lowercase: a "
                    "letter, then letters, digits, _, . or -.");
            start = end + 1;
        }
        return;
    }
}

void check_markers(std::string_view path, std::string_view source,
    const std::vector<std::string_view>& lines, ct::file_scan& out) {
    static const pt::regex begin(R"(^#\s*BEGIN generated (\w+)\b)");
    static const pt::regex end(R"(^#\s*END generated (\w+)\s*$)");
    std::vector<std::pair<std::string, std::uint32_t>> open;
    for (std::uint32_t i = 0; i < lines.size(); ++i) {
        const auto stripped = co::strip(lines[i]);
        if (const auto b = begin.match(stripped)) {
            open.emplace_back(b->group(1), i + 1);
        } else if (const auto e = end.match(stripped);
                   e && !open.empty() && open.back().first == e->group(1)) {
            open.pop_back();
        }
    }
    for (const auto& [kind, line] : open)
        co::report(out.findings, path, co::offset_of_line(source, line),
            unclosed_marker, "'# BEGIN generated " + kind + "' has no "
            "matching '# END generated " + kind + "' on its own line",
            "Put the END marker on a line of its own.");
}

void check_malformed(std::string_view path, std::string_view source,
    const std::vector<std::string_view>& lines, ct::file_scan& out) {
    static const pt::regex loose(R"(\[\[id:([0-9A-Fa-f-]+)\])");
    static const pt::regex uuid(R"(^[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-)"
        R"([0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}$)");
    for (std::uint32_t i = 0; i < lines.size(); ++i) {
        for (const auto& m : loose.find_all(lines[i])) {
            const auto target = m.group(1);
            if (!target.contains('-') || uuid.match(target))
                continue;
            co::report(out.findings, path, co::offset_of_line(source, i + 1),
                malformed_link, "[[id:" + std::string(target) + "]] (" +
                std::to_string(target.size()) + " chars) is not a UUID, so "
                "the page cannot export", "Link the page by its :ID:.");
        }
    }
}

/**
 * @brief Maps one page. Its own findings are the filetags, the unclosed
 * markers and the malformed links. Its fact is its type, the :ID: lines
 * it holds, its id links line by line with the line's offset, the
 * scripts its markers name and, for a durable page, its id links over the
 * whole text, which may span lines.
 */
void map_page(std::string_view path, std::string_view source,
    const ct::option_values&, ct::file_scan& out) {
    static const pt::regex type(R"(^#\+type:\s*(\S+))", {.multiline = true});
    static const pt::regex id(R"(^:ID:\s+(\S+))", {.multiline = true});
    static const pt::regex link(
        R"(\[\[id:([0-9A-Fa-f-]{36})\](?:\[([^\]]*)\])?\])");
    static const pt::regex source_marker(
        R"(^#\s*BEGIN generated\s+\S+\s+\(([^)]+)\)\s*$)",
        {.multiline = true});
    const auto text = pt::universal_newlines(pt::decode_replacing(source));
    const auto lines = co::split_lines(text);
    check_filetags(path, source, lines, out);
    check_markers(path, source, lines, out);
    check_malformed(path, source, lines, out);

    ct::fact_writer fact(out.fact);
    const auto t = type.search(text);
    fact.number(t ? 1 : 0);
    fact.text(t ? t->group(1) : std::string_view());
    const auto ids = id.find_all(text);
    fact.number(static_cast<std::uint32_t>(ids.size()));
    for (const auto& m : ids)
        fact.text(m.group(1));

    std::uint32_t count = 0;
    std::string links;
    ct::fact_writer link_fact(links);
    for (std::uint32_t i = 0; i < lines.size(); ++i) {
        for (const auto& m : link.find_all(lines[i])) {
            ++count;
            link_fact.number(co::offset_of_line(source, i + 1));
            link_fact.text(m.group(1));
            link_fact.text(first_characters(
                m.group_if(2).value_or("(no label)"), 50));
        }
    }
    fact.number(count);
    fact.text(links);

    const auto markers = source_marker.find_all(text);
    fact.number(static_cast<std::uint32_t>(markers.size()));
    for (const auto& m : markers) {
        const auto line = static_cast<std::uint32_t>(
            co::count(text.substr(0, m.start()), "\n") + 1);
        fact.number(co::offset_of_line(source, line));
        fact.text(m.group(1));
    }

    if (!is_durable(path)) {
        fact.number(0);
        return;
    }
    const auto all = link.find_all(text);
    fact.number(static_cast<std::uint32_t>(all.size()));
    for (const auto& m : all) {
        fact.text(m.group(1));
        fact.text(first_characters(one_line(
            m.group_if(2).value_or("(no label)")), 50));
    }
}

struct link_site {
    std::uint32_t offset;
    std::string_view target;
    std::string_view label;
};

struct marker_site {
    std::uint32_t offset;
    std::string_view script;
};

struct page {
    std::string_view path;
    std::vector<link_site> links;
    std::vector<marker_site> markers;
    std::vector<std::pair<std::string_view, std::string_view>> durable;
};

/**
 * @brief Whether the script a marker names writes the page: its source,
 * read with invalid bytes replaced, mentions the page's file name, or
 * the directory of a SKILL.org.
 */
void check_marker(const ct::corpus_view& v, std::string_view path,
    const marker_site& m, ct::diagnostic_table& out) {
    const auto named = std::string(co::strip(m.script));
    const auto script = checkout_path(named);
    const auto bytes = script ? v.read_file(*script) : std::nullopt;
    if (!bytes) {
        co::report(out, path, m.offset, marker_without_writer,
            "marker names " + std::string(m.script) + ", which does not "
            "exist", "Name the script that writes this page.");
        return;
    }
    const auto parts = co::components_of(path);
    const auto needle = parts.back() == "SKILL.org" && parts.size() > 1
        ? parts[parts.size() - 2] : parts.back();
    if (!pt::decode_replacing(*bytes).contains(needle))
        co::report(out, path, m.offset, marker_without_writer,
            "marker names " + std::string(m.script) + ", which never "
            "writes this file", "Name the script that writes this page.");
}

/**
 * @brief Resolves the links of every page against the IDs of all, in
 * pathlib's order of the paths, as the gate reads them.
 */
void reduce(std::span<const ct::scanned_file> scanned,
    const ct::corpus_view& v, ct::diagnostic_table& out) {
    std::vector<ct::scanned_file> by_path(scanned.begin(), scanned.end());
    std::ranges::sort(by_path, co::component_less, &ct::scanned_file::path);
    std::map<std::string, std::optional<std::string_view>, std::less<>>
        id_types;
    std::vector<page> all;
    all.reserve(by_path.size());
    for (const auto& f : by_path) {
        ct::fact_reader fact(f.fact);
        const bool typed = fact.number() != 0;
        const auto type = fact.text();
        const auto ids = fact.number();
        for (std::uint32_t i = 0; i < ids; ++i)
            id_types[upper(fact.text())] = typed
                ? std::optional(type) : std::optional<std::string_view>();
        auto& p = all.emplace_back();
        p.path = f.path;
        const auto links = fact.number();
        ct::fact_reader link_fact(fact.text());
        for (std::uint32_t i = 0; i < links; ++i) {
            const auto offset = link_fact.number();
            const auto target = link_fact.text();
            p.links.push_back({offset, target, link_fact.text()});
        }
        const auto markers = fact.number();
        for (std::uint32_t i = 0; i < markers; ++i) {
            const auto offset = fact.number();
            p.markers.push_back({offset, fact.text()});
        }
        const auto durable = fact.number();
        for (std::uint32_t i = 0; i < durable; ++i) {
            const auto target = fact.text();
            p.durable.emplace_back(target, fact.text());
        }
    }

    const bool strict = v.option("strict_links") == "true";
    for (const auto& p : all) {
        for (const auto& m : p.markers)
            check_marker(v, p.path, m, out);
        for (const auto& l : p.links) {
            if (id_types.contains(upper(l.target)))
                continue;
            co::report(out, p.path, l.offset, dangling_link, "[[id:" +
                std::string(l.target) + "]] -> " + std::string(l.label) +
                " resolves to no :ID: in the corpus", "Link a page that "
                "exists, or drop the link.");
        }
        if (!strict)
            continue;
        for (const auto& [target, label] : p.durable) {
            const auto it = id_types.find(upper(target));
            if (it == id_types.end() || !it->second ||
                !holds(agile_types, *it->second))
                continue;
            co::report(out, p.path, 0, durable_link, std::string(p.path) +
                " -> " + std::string(*it->second) + ": " +
                std::string(label), "Link the durable page the agile one "
                "produced, not the agile page.");
        }
    }
}

}

const ct::descriptor lint{
    .name = "lint",
    .version = 1,
    .summary = "Reports each malformed filetag, generated marker and id link "
        "of the org pages, as compass lint does.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::scan_function{pages, map_page, reduce}};

}
