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
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.docs/codecs/catalogue.hpp"
#include "compass.docs/ops/link_graph.hpp"
#include "compass.docs/ops/org_text.hpp"
#include "compass.docs/ops/regions.hpp"
#include "compass.org/ops/document_fields.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::docs::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace dop = compass::docs::ops;
namespace pt = compass::platform::text;

enum rule : std::uint32_t {
    bad_filetag, unclosed_marker, marker_without_writer, malformed_link,
    dangling_link, durable_link, pattern_unknown_use, pattern_one_way,
    pattern_not_a_pattern, pattern_shared_id, missing_backlink
};

constexpr std::array<std::string_view, 1> named_patterns_sources{
    "470D4571-47A4-4AEF-9A93-2B4DE0279C1E"};

constexpr std::array<ct::rule, 11> rules{{
    {"bad_filetag", "Every tag of a page's first #+filetags: is "
        "lowercase: a letter, then letters, digits, _, . or -.",
        {}, ct::blame_method::line},
    {"unclosed_marker", "Every '# BEGIN generated X' comment has a "
        "matching '# END generated X' comment.", {}, ct::blame_method::line},
    {"marker_without_writer", "A '# BEGIN generated X (writer)' marker "
        "names a script that exists and writes the page, or 'compass "
        "generate: codec' for a codec compass.docs ships.",
        {}, ct::blame_method::line},
    {"malformed_link", "An id link whose target has a hyphen is a UUID.",
        {}, ct::blame_method::line},
    {"dangling_link", "Every id link names the :ID: of some page.",
        {}, ct::blame_method::line},
    {"durable_link", "A page under doc/llm, doc/meta, doc/knowledge or "
        "doc/recipes links no agile page.", {}, ct::blame_method::line},
    {"pattern_unknown_use", "Every page a service architecture pattern "
        "lists under In ORE Studio exists."},
    {"pattern_one_way", "A pattern and each of its uses link each other: "
        "the use links the pattern, and the pattern lists the use.",
        named_patterns_sources},
    {"pattern_not_a_pattern", "A page lists only service architecture "
        "patterns under Patterns used.", named_patterns_sources},
    {"pattern_shared_id", "No two pages of a pattern link share an ID."},
    {"missing_backlink", "Every page a structure note orders links the "
        "note back."}}};

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
constexpr std::string_view pattern_type = "service_architecture_pattern";
constexpr std::array<std::string_view, 3> pattern_exempt{"doc/plans/",
    "doc/knowledge/external/", "doc/agile/"};

template <std::size_t N>
bool holds(const std::array<std::string_view, N>& set, std::string_view s) {
    return std::ranges::find(set, s) != set.end();
}

template <std::size_t N>
bool under(const std::array<std::string_view, N>& dirs,
    std::string_view path) {
    return std::ranges::any_of(dirs, [&](auto d) {
        return path.starts_with(d);
    });
}

/**
 * @brief The pages the per-page rules and the id-link rules read: every
 * .org file of the checkout but those under build, venv, .packages, vcpkg
 * or external at the root, under a .git, .claude, node_modules or
 * __pycache__ directory, or whose name starts with a dot.
 */
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

/**
 * @brief The pages the pattern rules read: UTF-8 pages under doc/ or
 * projects/ outside a build or vcpkg directory.
 */
bool in_patterns(const dop::page& p) {
    if (!p.utf8 || p.ids.empty() || (!p.path.starts_with("doc/") &&
            !p.path.starts_with("projects/")))
        return false;
    const std::string rooted = "/" + std::string(p.path);
    return !rooted.contains("/build/") && !rooted.contains("/vcpkg/");
}

/**
 * @brief The pages the structure notes order: UTF-8 pages under doc/ or
 * projects/modeling/.
 */
bool in_clusters(const dop::page& p) {
    return p.utf8 && !p.ids.empty() && (p.path.starts_with("doc/") ||
        p.path.starts_with("projects/modeling/"));
}

bool is_uuid(std::string_view s) {
    static const pt::regex uuid(R"(^[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-)"
        R"([0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}$)");
    return uuid.match(s).has_value();
}

bool is_malformed(std::string_view target) {
    return target.contains('-') && !is_uuid(target);
}

/**
 * @brief The first 50 characters of a link's label on one line, or
 * "(no label)".
 */
std::string label_of(std::string_view label) {
    if (label.empty())
        return "(no label)";
    std::string out(label.substr(0, co::byte_of_character(label, 50)));
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
    std::erase_if(out, [](auto p) {
        return !is_linted(p) && !p.starts_with("doc/") &&
            !p.starts_with("projects/");
    });
    return out;
}

void check_filetags(const ct::document& d, ct::file_scan& out) {
    static const pt::regex tag(R"(^[a-z][a-z0-9_.-]*$)");
    const auto k = compass::org::ops::find_keyword(d.tables, "filetags");
    if (k.is_none())
        return;
    const auto value = compass::org::ops::trimmed(d.source,
        d.tables.keywords.values[k]).of(d.source);
    if (value.size() < 2 || !value.starts_with(':') || !value.ends_with(':'))
        return;
    const auto raw = value.substr(1, value.size() - 2);
    const auto line = dop::line_start(d.source, d.tables.elements.ranges[
        d.tables.keywords.elements[k]].begin);
    std::size_t start = 0;
    while (start <= raw.size()) {
        auto end = raw.find(':', start);
        if (end == std::string_view::npos)
            end = raw.size();
        const auto t = raw.substr(start, end - start);
        if (!co::strip(t).empty() && !tag.match(t))
            co::report(out.findings, d.path, line, bad_filetag,
                "unknown/malformed tag '" + std::string(t) + "'", "Write the "
                "tag in lowercase: a letter, then letters, digits, _, . or "
                "-.");
        start = end + 1;
    }
}

void check_markers(const ct::document& d, ct::file_scan& out) {
    static const pt::regex begin(R"(^#\s*BEGIN generated (\w+)\b)");
    static const pt::regex end(R"(^#\s*END generated (\w+)\s*$)");
    std::vector<std::pair<std::string, std::uint32_t>> open;
    for (const auto& c : dop::comments(d)) {
        for (const auto line : c.lines) {
            const auto stripped = co::strip(line);
            if (const auto b = begin.match(stripped)) {
                open.emplace_back(b->group(1), dop::line_start(d.source,
                    static_cast<std::uint32_t>(line.data() -
                        d.source.data())));
            } else if (const auto e = end.match(stripped); e &&
                       !open.empty() && open.back().first == e->group(1)) {
                open.pop_back();
            }
        }
    }
    for (const auto& [kind, line] : open)
        co::report(out.findings, d.path, line, unclosed_marker,
            "'# BEGIN generated " + kind + "' has no matching '# END "
            "generated " + kind + "' on its own line", "Put the END marker "
            "on a line of its own.");
}

void check_malformed(const ct::document& d, ct::file_scan& out) {
    for (const auto& l : dop::bracket_links(d, "id")) {
        if (!is_malformed(l.target))
            continue;
        co::report(out.findings, d.path, dop::line_start(d.source, l.begin),
            malformed_link, "[[id:" + std::string(l.target) + "]] (" +
            std::to_string(l.target.size()) + " chars) is not a UUID, so "
            "the page cannot export", "Link the page by its :ID:.");
    }
}

/**
 * @brief Maps one page: its fact is what the link graph knows of it, and
 * its findings, for a page the gate lints, its filetags, its unclosed
 * markers and its malformed links.
 */
void map_page(const ct::document& d, const ct::option_values&,
    ct::file_scan& out) {
    out.fact = dop::fact_of(d);
    if (!is_linted(d.path))
        return;
    check_filetags(d, out);
    check_markers(d, out);
    check_malformed(d, out);
}

/**
 * @brief Whether the writer a marker names exists. A marker naming compass
 * generate names a codec of this component, whose drift check holds the
 * region; any other names a script whose source, read with invalid bytes
 * replaced, mentions the page's file name, or the directory of a
 * SKILL.org.
 */
void check_marker(const ct::corpus_view& v, std::string_view path,
    const dop::marker& m, ct::diagnostic_table& out) {
    const auto named = std::string(co::strip(m.script));
    if (named.starts_with(dop::compass_writer)) {
        const auto codec = std::string_view(named).substr(
            dop::compass_writer.size());
        if (!std::ranges::contains(catalogue(), codec,
                &ct::descriptor::name))
            co::report(out, path, m.line, marker_without_writer,
                "marker names " + named + ", but compass has no codec " +
                std::string(codec), "Name the codec that writes this page.");
        return;
    }
    const auto script = checkout_path(named);
    const auto bytes = script ? v.read_file(*script) : std::nullopt;
    if (!bytes) {
        co::report(out, path, m.line, marker_without_writer,
            "marker names " + std::string(m.script) + ", which does not "
            "exist", "Name the script that writes this page.");
        return;
    }
    const auto parts = co::components_of(path);
    const auto needle = parts.back() == "SKILL.org" && parts.size() > 1
        ? parts[parts.size() - 2] : parts.back();
    if (!pt::decode_replacing(*bytes).contains(needle))
        co::report(out, path, m.line, marker_without_writer,
            "marker names " + std::string(m.script) + ", which never "
            "writes this file", "Name the script that writes this page.");
}

/**
 * @brief The lint rules: markers name their writers, id links resolve
 * against the IDs of every linted page, and, with strict_links, durable
 * pages link no agile page. Pages are read in pathlib's order, as the
 * gate reads them, and a later page's ID wins.
 */
void report_lint(const std::vector<dop::page>& all, const ct::corpus_view& v,
    ct::diagnostic_table& out) {
    std::vector<const dop::page*> linted;
    for (const auto& p : all) {
        if (is_linted(p.path))
            linted.push_back(&p);
    }
    std::ranges::sort(linted, co::component_less, &dop::page::path);
    std::unordered_map<std::string_view, std::string_view> types;
    for (const auto* p : linted) {
        for (const auto id : p->ids)
            types.insert_or_assign(id, p->type);
    }
    const bool strict = v.option("strict_links") == "true";
    for (const auto* p : linted) {
        for (const auto& m : p->markers)
            check_marker(v, p->path, m, out);
        for (const auto& l : p->links) {
            if (types.contains(dop::upper(l.target)))
                continue;
            co::report(out, p->path, l.line, dangling_link, "[[id:" +
                std::string(l.target) + "]] -> " + label_of(l.label) +
                " resolves to no :ID: in the corpus", "Link a page that "
                "exists, or drop the link.");
        }
        if (!strict || !under(durable_dirs, p->path))
            continue;
        for (const auto& l : p->links) {
            const auto it = types.find(dop::upper(l.target));
            if (it == types.end() || !holds(agile_types, it->second))
                continue;
            co::report(out, p->path, 0, durable_link, std::string(p->path) +
                " -> " + std::string(it->second) + ": " + label_of(l.label),
                "Link the durable page the agile one produced, not the "
                "agile page.");
        }
    }
}

/**
 * @brief The targets, in upper case, of the links of @p p in its first
 * level-1 section titled @p title, or of all its links when @p title is
 * empty.
 */
std::vector<std::string> targets(const dop::page& p,
    std::string_view title = {}) {
    std::uint32_t section = dop::no_section;
    if (!title.empty()) {
        const auto it = std::ranges::find(p.sections, title);
        if (it == p.sections.end())
            return {};
        section = static_cast<std::uint32_t>(it - p.sections.begin());
    }
    std::vector<std::string> out;
    for (const auto& l : p.links) {
        if (title.empty() || l.section == section)
            out.push_back(dop::upper(l.target));
    }
    return out;
}

/**
 * @brief The pattern rules: a service architecture pattern and each of
 * its uses link each other, a use lists only patterns, and the pages of
 * a pattern link have IDs of their own.
 */
void report_patterns(const std::vector<dop::page>& all,
    ct::diagnostic_table& out) {
    std::map<std::string_view, const dop::page*> docs;
    std::map<std::string_view, std::vector<std::string_view>> duplicates;
    for (const auto& p : all) {
        if (!in_patterns(p))
            continue;
        const auto key = p.ids.front();
        if (const auto it = docs.find(key); it != docs.end()) {
            auto& shared = duplicates[key];
            if (shared.empty())
                shared.push_back(it->second->path);
            shared.push_back(p.path);
        }
        docs[key] = &p;
    }
    std::set<std::string_view> patterns;
    for (const auto& [id, p] : docs) {
        if (p->type == pattern_type)
            patterns.insert(id);
    }
    std::set<std::string_view> involved = patterns;
    const auto contains = [](const std::vector<std::string>& ids,
        std::string_view id) {
        return std::ranges::find(ids, id) != ids.end();
    };
    for (const auto pid : patterns) {
        const auto* p = docs.at(pid);
        for (const auto& uid : targets(*p, "In ORE Studio")) {
            const auto use = docs.find(uid);
            if (use == docs.end()) {
                co::report(out, p->path, 0, pattern_unknown_use,
                    std::string(p->path) + " lists id " + uid + " under In "
                    "ORE Studio, which no page has", "Link a page that "
                    "exists, or drop the row.");
                continue;
            }
            involved.insert(use->first);
            if (patterns.contains(use->first) ||
                under(pattern_exempt, use->second->path))
                continue;
            if (!contains(targets(*use->second), pid))
                co::report(out, use->second->path, 0, pattern_one_way,
                    std::string(use->second->path) + " is a use listed by " +
                    std::string(p->path) + " but does not link the pattern "
                    "back", "List the pattern under the use's Patterns "
                    "used.");
        }
    }
    for (const auto& [did, d] : docs) {
        if (under(pattern_exempt, d->path) || patterns.contains(did))
            continue;
        const auto used = targets(*d, "Patterns used");
        if (!used.empty())
            involved.insert(did);
        for (const auto& pid : used) {
            if (!patterns.contains(pid)) {
                const auto target = docs.find(pid);
                co::report(out, d->path, 0, pattern_not_a_pattern,
                    std::string(d->path) + " lists " + (target == docs.end()
                        ? pid : std::string(target->second->path)) +
                    " under Patterns used, which is not a service "
                    "architecture pattern", "List only service architecture "
                    "patterns there.");
                continue;
            }
            const auto* p = docs.at(pid);
            if (!contains(targets(*p, "In ORE Studio"), did))
                co::report(out, d->path, 0, pattern_one_way,
                    std::string(d->path) + " uses " + std::string(p->path) +
                    ", which does not list it under In ORE Studio", "Add the "
                    "use to the pattern's In ORE Studio section.");
        }
    }
    for (const auto& [did, paths] : duplicates) {
        if (!involved.contains(did))
            continue;
        std::string listed;
        for (const auto p : paths)
            listed += (listed.empty() ? "" : ", ") + std::string(p);
        co::report(out, paths.front(), 0, pattern_shared_id, "id " +
            std::string(did) + " is shared by " + listed, "Give each page "
            "an ID of its own.");
    }
}

/**
 * @brief The structure note rule: each page a note orders, in its
 * sections before What this cluster does not cover and See also, links
 * the note back, unless it is a note itself. Notes are taken in ID order.
 */
void report_backlinks(const std::vector<dop::page>& all,
    ct::diagnostic_table& out) {
    std::map<std::string_view, const dop::page*> docs;
    for (const auto& p : all) {
        if (in_clusters(p))
            docs.insert_or_assign(p.ids.front(), &p);
    }
    std::set<std::string_view> hubs;
    for (const auto& [id, p] : docs) {
        if (p->structure_note)
            hubs.insert(id);
    }
    for (const auto hub_id : hubs) {
        const auto* hub = docs.at(hub_id);
        std::vector<std::pair<std::string_view, bool>> cluster;
        for (const auto& l : hub->links) {
            if (!l.ordering)
                continue;
            const auto target = docs.find(dop::upper(l.target));
            if (target == docs.end() || target->first == hub_id ||
                std::ranges::contains(cluster, target->first,
                    &std::pair<std::string_view, bool>::first))
                continue;
            cluster.emplace_back(target->first, !l.aside);
        }
        std::ranges::stable_partition(cluster,
            &std::pair<std::string_view, bool>::second);
        for (const auto& [target, ordered] : cluster) {
            if (hubs.contains(target))
                continue;
            const auto* page = docs.at(target);
            if (std::ranges::contains(targets(*page), hub_id))
                continue;
            const std::string_view why = ordered
                ? "the structure note that orders this cluster, and where "
                  "to read this page in it."
                : "the structure note for this cluster; this page sits "
                  "alongside its sequence rather than in it.";
            co::report(out, page->path, page->see_also, missing_backlink,
                std::string(page->path) + " is in the cluster of the "
                "structure note " + std::string(hub->path) + " but does "
                "not link it back", "Add \"- [[id:" + std::string(hub_id) +
                "][" + std::string(hub->title) + "]] \xe2\x80\x94 " +
                std::string(why) + "\" to the page's See also section.");
        }
    }
}

void reduce(std::span<const ct::scanned_file> scanned,
    const ct::corpus_view& v, ct::diagnostic_table& out) {
    std::vector<dop::page> all;
    all.reserve(scanned.size());
    for (const auto& f : scanned)
        all.push_back(dop::page_of(f.path, f.fact));
    report_lint(all, v, out);
    report_patterns(all, out);
    report_backlinks(all, out);
}

}

const ct::descriptor lint{
    .name = "lint",
    .version = 5,
    .summary = "Reports each malformed filetag, generated marker and id link "
        "of the org pages, as compass lint does, each pattern and use that "
        "do not link each other, and each page a structure note orders "
        "that does not link it back.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::scan_function{.files = pages, .reduce = reduce,
        .map_document = map_page}};

}
