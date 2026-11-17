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
#include "compass.docs/codecs/structure_notes.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::docs::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

struct page {
    std::string path;
    std::string text;
};

std::string upper(std::string_view s) {
    std::string out(s);
    std::ranges::transform(out, out.begin(), [](char c) {
        return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
    });
    return out;
}

std::string frontmatter(std::string_view text, std::string_view field) {
    const pt::regex line("^#\\+" + std::string(field) + R"(:\s*(.+?)\s*$)",
        {.multiline = true});
    const auto m = line.search(text);
    return m ? std::string(m->group(1)) : std::string();
}

/**
 * @brief The text before the sections that point outside the cluster.
 */
std::string_view ordering_section(std::string_view hub) {
    auto end = hub.size();
    for (const auto heading : {"\n* What this cluster does not cover",
             "\n* See also"}) {
        if (const auto i = hub.find(heading); i != std::string_view::npos)
            end = std::min(end, i);
    }
    return hub.substr(0, end);
}

/**
 * @brief The text before the first heading, then each heading with its
 * body up to the next heading at any level.
 */
std::vector<std::string_view> sections(std::string_view text) {
    static const pt::regex heading(R"(^\*+ .*$)", {.multiline = true});
    std::vector<std::size_t> bounds;
    for (const auto& m : heading.find_all(text))
        bounds.push_back(m.start());
    if (bounds.empty())
        return {text};
    std::vector<std::string_view> out{text.substr(0, bounds[0])};
    for (std::size_t i = 0; i < bounds.size(); ++i) {
        const auto end = i + 1 < bounds.size() ? bounds[i + 1] : text.size();
        out.push_back(text.substr(bounds[i], end - bounds[i]));
    }
    return out;
}

std::optional<std::string> add_backlink(std::string_view text,
    const std::string& hub_id, std::string_view title, bool ordered) {
    std::string lower(hub_id);
    std::ranges::transform(lower, lower.begin(), [](char c) {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    });
    if (text.find("id:" + hub_id) != std::string_view::npos ||
        text.find("id:" + lower) != std::string_view::npos)
        return std::nullopt;
    const std::string_view why = ordered
        ? "the structure note that orders this cluster, and where to read "
          "this page in it."
        : "the structure note for this cluster; this page sits alongside its "
          "sequence rather than in it.";
    const auto line = "- [[id:" + hub_id + "][" + std::string(title) +
        "]] \xe2\x80\x94 " + std::string(why);
    static const pt::regex see_also(R"(^\* See also\s*$)",
        {.multiline = true});
    if (const auto m = see_also.search(text)) {
        const auto rest = text.substr(m->end());
        const auto head = std::string(text.substr(0, m->end())) + "\n\n" +
            line;
        if (!co::strip(rest).empty())
            return head + std::string(rest);
        auto trimmed = rest;
        while (trimmed.ends_with('\n'))
            trimmed.remove_suffix(1);
        return head + std::string(trimmed) + "\n";
    }
    auto trimmed = text;
    while (trimmed.ends_with('\n'))
        trimmed.remove_suffix(1);
    return std::string(trimmed) + "\n\n* See also\n\n" + line + "\n";
}

bool in_search(std::string_view path) {
    return path.starts_with("doc/") || path.starts_with("projects/modeling/");
}

void run(const ct::corpus_view& v, ct::file_output& out) {
    static const pt::regex id(R"(^:ID:\s*(\S+)\s*$)", {.multiline = true});
    static const pt::regex link(
        R"(\[\[id:([0-9A-Fa-f-]{36})\]\[([^\]]*)\]\])");
    std::map<std::string, page> docs;
    for (std::size_t i = 0; i < v.size(); ++i) {
        const auto d = v[i];
        if (!in_search(d.path) || !d.path.ends_with(".org") ||
            !pt::is_utf8(d.source))
            continue;
        if (const auto m = id.search(d.source))
            docs.insert_or_assign(upper(m->group(1)),
                page{std::string(d.path), std::string(d.source)});
    }
    std::vector<std::string> hubs;
    for (const auto& [identifier, p] : docs) {
        if (frontmatter(p.text, "filetags").find("structure_note") !=
            std::string::npos)
            hubs.push_back(identifier);
    }
    std::vector<std::string> written;
    for (const auto& hub_id : hubs) {
        const auto hub_text = docs.at(hub_id).text;
        const auto title = frontmatter(hub_text, "title");
        std::vector<std::string> ordered;
        std::vector<std::string> aside;
        for (const auto body : sections(ordering_section(hub_text))) {
            auto& bucket = body.find("# aside") != std::string_view::npos
                ? aside : ordered;
            for (const auto& m : link.find_all(body)) {
                const auto target = upper(m.group(1));
                if (target == hub_id || !docs.contains(target) ||
                    std::ranges::contains(ordered, target) ||
                    std::ranges::contains(aside, target))
                    continue;
                bucket.push_back(target);
            }
        }
        const auto add = [&](const std::string& target, bool in_order) {
            if (std::ranges::contains(hubs, target))
                return;
            auto& p = docs.at(target);
            if (const auto updated = add_backlink(p.text, hub_id, title,
                    in_order))
                p.text = *updated;
            if (!std::ranges::contains(written, target))
                written.push_back(target);
        };
        for (const auto& target : ordered)
            add(target, true);
        for (const auto& target : aside)
            add(target, false);
    }
    for (const auto& target : written)
        out.files.add(docs.at(target).path, docs.at(target).text);
}

}

const ct::descriptor structure_notes{
    .name = "structure_notes",
    .version = 1,
    .summary = "Writes every page a structure note orders with a link back "
        "to the note.",
    .reads = ct::inputs::every_document(),
    .options = {},
    .rules = {},
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::file_output>{run}};

}
