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
#include "compass.bench/ops/html_stage.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include "compass.html/ops/render.hpp"
#include "compass.ingest/org/parser.hpp"
#include "compass.org/ops/document_fields.hpp"
#include "compass.platform/filesystem/mapped_file.hpp"

namespace compass::bench::ops {

namespace {

namespace org = compass::org;

double elapsed_ms(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
}

std::string trimmed_text(std::string_view source,
    org::types::source_range r) {
    return std::string(org::ops::trimmed(source, r).of(source));
}

struct page {
    std::string path;
    std::unique_ptr<platform::filesystem::mapped_file> file;
    org::types::tables tables;
};

}

html_result run_html_stage(const std::filesystem::path& root,
    const std::filesystem::path& out, unsigned repetitions) {
    html_result r;
    std::unordered_map<std::string, std::string> document_of_id;
    std::unordered_map<std::string, std::string> document_at_path;
    std::unordered_map<std::string, bool> knowledge;
    std::vector<std::string> paths;

    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file() && entry.path().extension() == ".org")
            paths.push_back(std::filesystem::relative(entry.path(), root)
                .generic_string());
    }
    std::sort(paths.begin(), paths.end());

    std::vector<page> pages;
    org::types::tables t;
    for (const auto& path : paths) {
        const platform::filesystem::mapped_file file(root / path);
        const auto source = file.text();
        ingest::org::parse(source, t);
        const auto v = org::types::view(t);
        ++r.scanned;
        const auto id_row = org::ops::find_document_property(v, "ID");
        if (id_row.is_none())
            continue;
        const auto id = trimmed_text(source, v.properties.values[id_row]);
        for (std::uint32_t p = 0; p < v.properties.size(); ++p) {
            const org::types::property_id row{p};
            if (org::ops::name_text(v, v.properties.names[row]) == "ID")
                document_of_id.emplace(
                    trimmed_text(source, v.properties.values[row]), id);
        }
        document_at_path.emplace(path, id);
        const auto type_row = org::ops::find_keyword(v, "type");
        const auto type = type_row.is_none() ? std::string()
            : trimmed_text(source, v.keywords.values[type_row]);
        const bool shown = type == "knowledge" || type == "manual";
        knowledge.emplace(id, shown);
        if (shown) {
            page p;
            p.path = path;
            pages.push_back(std::move(p));
        }
    }

    const auto parse_start = std::chrono::steady_clock::now();
    for (auto& p : pages) {
        p.file = std::make_unique<platform::filesystem::mapped_file>(
            root / p.path);
        ingest::org::parse(p.file->text(), p.tables);
    }
    r.parse_ms = elapsed_ms(parse_start);
    r.rendered = pages.size();

    std::size_t lookups = 0;
    std::size_t missing = 0;
    std::size_t shown = 0;
    const auto lookup = [&](const auto& map, std::string_view key)
        -> std::optional<std::string> {
        ++lookups;
        const auto it = map.find(std::string(key));
        if (it != map.end())
            return it->second;
        ++missing;
        return std::nullopt;
    };
    html::types::link_resolver links;
    links.document_of = [&](std::string_view id) {
        return lookup(document_of_id, id);
    };
    links.document_at = [&](std::string_view path) {
        return lookup(document_at_path, path);
    };
    links.visible = [&](std::string_view id) {
        const auto it = knowledge.find(std::string(id));
        const bool v = it != knowledge.end() && it->second;
        shown += v ? 1 : 0;
        return v;
    };
    const html::types::render_config config;

    for (unsigned rep = 0; rep < repetitions; ++rep) {
        lookups = 0;
        missing = 0;
        shown = 0;
        std::size_t bytes = 0;
        std::map<std::string, std::size_t> unsupported;
        std::vector<html::types::rendered_document> docs;
        docs.reserve(pages.size());
        const auto start = std::chrono::steady_clock::now();
        for (const auto& p : pages)
            docs.push_back(html::ops::render(p.file->text(),
                org::types::view(p.tables), p.path, config, links));
        r.render_ms.push_back(elapsed_ms(start));
        for (const auto& d : docs) {
            bytes += d.body.size();
            for (const auto& kind : d.unsupported)
                ++unsupported[kind];
        }
        r.bytes = bytes;
        r.unsupported = std::move(unsupported);
        r.links = lookups;
        r.unknown = missing;
        r.hidden = lookups - shown - missing;
        if (rep + 1 < repetitions || out.empty())
            continue;
        for (std::size_t i = 0; i < pages.size(); ++i) {
            const auto file = out / (pages[i].path + ".html");
            std::filesystem::create_directories(file.parent_path());
            std::ofstream os(file, std::ios::binary);
            os << docs[i].body;
            if (!os)
                throw std::runtime_error("cannot write " +
                    file.generic_string());
        }
    }
    return r;
}

void write_html_report(std::ostream& os, const html_result& r) {
    os << "scanned " << r.scanned << " org files\n"
       << "rendered " << r.rendered << " knowledge and manual pages, "
       << r.bytes << " bytes of HTML\n"
       << "links to documents " << r.links << ", unknown " << r.unknown
       << ", outside the view " << r.hidden << "\n"
       << "parse " << r.parse_ms << " ms\n";
    for (const auto ms : r.render_ms)
        os << "render " << ms << " ms\n";
    if (r.unsupported.empty())
        os << "unsupported: none\n";
    for (const auto& [kind, count] : r.unsupported)
        os << "unsupported " << kind << " " << count << "\n";
}

}
