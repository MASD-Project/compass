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
#include "compass.cpp/codecs/exported_types.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codec/types/fact.hpp"
#include "compass.cpp/ops/exported_types.hpp"
#include "compass.cpp/ops/paths.hpp"

namespace compass::cpp::codecs {

namespace {

namespace ct = compass::codec::types;

constexpr std::array<ct::rule, 1> rules{{
    {"uncompiled_exported_type",
        "No library exports a type that none of its translation units "
        "compiles."}}};

/**
 * @brief The fact of a file: its quoted includes, the export macro it
 * defines when it is a library's export header, and the types it exports
 * with any macro.
 */
struct file_fact {
    std::vector<std::string_view> includes;
    std::optional<std::string_view> macro;
    std::vector<ops::exported_type> types;
};

file_fact read_fact(std::string_view fact) {
    ct::fact_reader r(fact);
    file_fact out;
    for (auto n = r.number(); n > 0; --n)
        out.includes.push_back(r.text());
    if (r.number() != 0)
        out.macro = r.text();
    while (!r.done()) {
        ops::exported_type t;
        t.macro = r.text();
        t.name = r.text();
        t.offset = r.number();
        out.types.push_back(t);
    }
    return out;
}

bool is_export_header(std::string_view path);

void map(std::string_view path, std::string_view bytes,
    const ct::option_values&, ct::file_scan& out) {
    ct::fact_writer w(out.fact);
    const auto includes = ops::quoted_includes(bytes);
    w.number(static_cast<std::uint32_t>(includes.size()));
    for (const auto i : includes)
        w.text(i);
    const auto macro = is_export_header(path) ? ops::export_macro(bytes)
        : std::nullopt;
    w.number(macro ? 1 : 0);
    if (macro)
        w.text(*macro);
    if (path.ends_with(".hpp")) {
        for (const auto& t : ops::exported_types(bytes))
            w.text(t.macro).text(t.name).number(t.offset);
    }
}

/**
 * @brief The files the #include "..." lines of each file reach, resolved
 * once per file whichever library's walk reaches it, from the facts of the
 * files scanned and by reading any other.
 */
class include_graph final {
public:
    include_graph(const ct::corpus_view& v, const ops::file_set& files,
        std::vector<std::string> roots,
        const std::unordered_map<std::string_view, file_fact>& facts)
        : view_(v), files_(files), roots_(std::move(roots)), facts_(facts) {}

    const std::vector<std::string>& targets(const std::string& path) {
        if (const auto it = targets_.find(path); it != targets_.end())
            return it->second;
        auto& out = targets_[path];
        std::string text;
        std::vector<std::string_view> names;
        if (const auto it = facts_.find(path); it != facts_.end()) {
            names = it->second.includes;
        } else {
            text = view_.read_file(path).value_or("");
            names = ops::quoted_includes(text);
        }
        const auto dir = ops::parent_of(path);
        for (const auto name : names) {
            auto found = files_.find(dir, name);
            for (std::size_t r = 0; !found && r < roots_.size(); ++r)
                found = files_.find(roots_[r], name);
            if (found)
                out.push_back(std::move(*found));
        }
        return out;
    }

private:
    const ct::corpus_view& view_;
    const ops::file_set& files_;
    std::vector<std::string> roots_;
    const std::unordered_map<std::string_view, file_fact>& facts_;
    std::unordered_map<std::string, std::vector<std::string>> targets_;
};

/**
 * @brief Every projects/<component>/<part>/include directory, in pathlib
 * order.
 */
std::vector<std::string> include_roots(
    const std::vector<std::string_view>& paths) {
    std::vector<std::string> out;
    for (const auto p : paths) {
        const auto parts = ops::components_of(p);
        if (parts.size() >= 5 && parts[0] == "projects" &&
            parts[3] == "include")
            out.push_back(std::string(p.substr(0, parts[3].data() +
                parts[3].size() - p.data())));
    }
    std::ranges::sort(out, ops::component_less);
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

bool is_export_header(std::string_view path) {
    const auto parts = ops::components_of(path);
    return parts.size() == 6 && parts[0] == "projects" &&
        parts[3] == "include" && parts[5] == "export.hpp";
}

std::vector<std::string_view> under(const std::vector<std::string_view>& all,
    std::string_view prefix, std::string_view suffix) {
    std::vector<std::string_view> out;
    for (auto it = std::ranges::lower_bound(all, prefix);
         it != all.end() && it->starts_with(prefix); ++it) {
        if (it->ends_with(suffix))
            out.push_back(*it);
    }
    return out;
}

constexpr std::array<std::string_view, 9> scanned_suffixes{".cpp", ".hpp",
    ".h", ".ipp", ".inc", ".cc", ".cxx", ".hxx", ".tpp"};

std::vector<std::string_view> scanned(const ct::corpus_view& v) {
    std::vector<std::string_view> out;
    for (const auto suffix : scanned_suffixes) {
        const auto paths = v.list_files("projects/", suffix);
        out.insert(out.end(), paths.begin(), paths.end());
    }
    return out;
}

void reduce(std::span<const ct::scanned_file> scanned_files,
    const ct::corpus_view& v, ct::diagnostic_table& out) {
    std::unordered_map<std::string_view, file_fact> facts;
    for (const auto& f : scanned_files)
        facts.emplace(f.path, read_fact(f.fact));
    const auto all = v.list_files("");
    const ops::file_set files(all);
    include_graph graph(v, files, include_roots(all), facts);

    std::vector<std::string_view> export_headers;
    for (const auto p : under(all, "projects/", "/export.hpp")) {
        if (is_export_header(p))
            export_headers.push_back(p);
    }
    std::ranges::sort(export_headers, ops::component_less);

    const file_fact none;
    const auto fact_of = [&](std::string_view path) -> const file_fact& {
        const auto it = facts.find(path);
        return it == facts.end() ? none : it->second;
    };
    for (const auto export_header : export_headers) {
        const auto macro = fact_of(export_header).macro;
        if (!macro)
            continue;
        const auto library = ops::parent_of(ops::parent_of(
            ops::parent_of(export_header)));
        const auto sources = std::string(library) + "/src";
        if (!files.is_dir(sources))
            continue;

        std::unordered_set<std::string> reached;
        std::vector<std::string> queue;
        for (const auto p : under(all, sources + "/", ".cpp"))
            queue.emplace_back(p);
        while (!queue.empty()) {
            auto current = std::move(queue.back());
            queue.pop_back();
            if (!reached.insert(current).second)
                continue;
            for (const auto& target : graph.targets(current))
                queue.push_back(target);
        }

        auto headers = under(all, std::string(library) + "/include/",
            ".hpp");
        std::ranges::sort(headers, ops::component_less);
        for (const auto header : headers) {
            auto types = fact_of(header).types;
            std::erase_if(types, [&](const ops::exported_type& t) {
                return t.macro != *macro;
            });
            if (types.empty() || reached.contains(std::string(header)))
                continue;
            std::string names;
            for (const auto& t : types)
                names += (names.empty() ? "" : ", ") + std::string(t.name);
            const auto at = types.front().offset;
            out.add({.document = compass::org::types::document_id::none(),
                .range = {at, at},
                .rule = ct::rule_id{0},
                .level = ct::severity::error,
                .message = "exports " + names + " with " +
                    std::string(*macro) + ", and no translation unit of "
                    "its library compiles the header, so on Windows every "
                    "consumer imports a symbol nobody defines",
                .fix = "drop the export macro from a header-only type, or "
                    "add a .cpp to the library that includes the header",
                .file = header});
        }
    }
}

}

const ct::descriptor exported_types{
    .name = "exported_types",
    .version = 2,
    .summary = "Reports each header whose library exports a type that no "
        "translation unit of the library compiles.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::scan_function{scanned, map, reduce}};

}
