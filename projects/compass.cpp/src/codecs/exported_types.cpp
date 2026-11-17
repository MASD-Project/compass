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
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codec/types/fact.hpp"
#include "compass.cpp/ops/exported_types.hpp"
#include "compass.cpp/ops/lexer.hpp"
#include "compass.cpp/ops/paths.hpp"

namespace compass::cpp::codecs {

namespace {

namespace ct = compass::codec::types;

constexpr std::array<ct::rule, 2> rules{{
    {"uncompiled_exported_type",
        "No library exports a type that none of its translation units "
        "compiles."},
    {"visibility_macro_on_enum",
        "No symbol visibility or DLL storage macro decorates an enum."}}};

constexpr ct::rule_id uncompiled{0};
constexpr ct::rule_id decorated_enum{1};

/**
 * @brief The fact of a file: its quoted includes, the export macro it
 * defines when it is a library's export header, the types it exports
 * with any macro, the visibility macros it defines, and each enum an
 * identifier stands before.
 */
struct file_fact {
    std::vector<std::string_view> includes;
    std::optional<std::string_view> macro;
    std::vector<ops::exported_type> types;
    std::vector<std::string_view> visibility_macros;
    std::vector<ops::enum_site> enums;
};

file_fact read_fact(std::string_view fact) {
    ct::fact_reader r(fact);
    file_fact out;
    for (auto n = r.number(); n > 0; --n)
        out.includes.push_back(r.text());
    if (r.number() != 0)
        out.macro = r.text();
    for (auto n = r.number(); n > 0; --n) {
        ops::exported_type t;
        t.macro = r.text();
        t.name = r.text();
        t.offset = r.number();
        out.types.push_back(t);
    }
    for (auto n = r.number(); n > 0; --n)
        out.visibility_macros.push_back(r.text());
    while (!r.done()) {
        ops::enum_site e;
        e.offset = r.number();
        e.macro = r.text();
        e.source = r.text();
        out.enums.push_back(e);
    }
    return out;
}

bool is_export_header(std::string_view path) {
    const auto parts = ops::components_of(path);
    return parts.size() == 6 && parts[0] == "projects" &&
        parts[3] == "include" && parts[5] == "export.hpp";
}

void map(std::string_view path, std::string_view bytes,
    const ct::option_values&, ct::file_scan& out) {
    const auto lexed = ops::lex(bytes);
    const auto model = ops::model_of(bytes, lexed);
    ct::fact_writer w(out.fact);
    const auto includes = ops::quoted_includes(model);
    w.number(static_cast<std::uint32_t>(includes.size()));
    for (const auto i : includes)
        w.text(i);
    const auto macro = is_export_header(path)
        ? ops::export_macro(lexed, model) : std::nullopt;
    w.number(macro ? 1 : 0);
    if (macro)
        w.text(*macro);
    const auto types = path.ends_with(".hpp")
        ? ops::exported_types(bytes, model)
        : std::vector<ops::exported_type>{};
    w.number(static_cast<std::uint32_t>(types.size()));
    for (const auto& t : types)
        w.text(t.macro).text(t.name).number(t.offset);
    const auto macros = ops::visibility_macros(model);
    w.number(static_cast<std::uint32_t>(macros.size()));
    for (const auto m : macros)
        w.text(m);
    for (const auto& e : ops::macro_enum_sites(lexed, model))
        w.number(e.offset).text(e.macro).text(e.source);
}

/**
 * @brief The files the #include "..." lines of each scanned file reach,
 * resolved once per file whichever library's walk reaches it.
 */
class include_graph final {
public:
    include_graph(const ops::file_set& files, std::vector<std::string> roots,
        const std::unordered_map<std::string_view, file_fact>& facts)
        : files_(files), roots_(std::move(roots)), facts_(facts) {}

    const std::vector<std::string>& targets(const std::string& path) {
        if (const auto it = targets_.find(path); it != targets_.end())
            return it->second;
        auto& out = targets_[path];
        const auto it = facts_.find(path);
        if (it == facts_.end())
            return out;
        const auto dir = ops::parent_of(path);
        for (const auto name : it->second.includes) {
            auto found = files_.find(dir, name);
            if (!found)
                found = under_roots(name);
            if (found)
                out.push_back(std::move(*found));
        }
        return out;
    }

private:
    /**
     * @brief The file @p name names under the first include root that has
     * it, looked up once per name.
     */
    const std::optional<std::string>& under_roots(std::string_view name) {
        const std::string key(name);
        if (const auto it = rooted_.find(key); it != rooted_.end())
            return it->second;
        std::optional<std::string> found;
        for (std::size_t r = 0; !found && r < roots_.size(); ++r)
            found = files_.find(roots_[r], name);
        return rooted_.emplace(key, std::move(found)).first->second;
    }

    const ops::file_set& files_;
    std::vector<std::string> roots_;
    const std::unordered_map<std::string_view, file_fact>& facts_;
    std::unordered_map<std::string, std::optional<std::string>> rooted_;
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

/**
 * @brief Reports each header of a library that exports a type with the
 * library's macro and that no .cpp under the library's src/ reaches.
 * @p all is every scanned path, sorted.
 */
void report_uncompiled(const std::vector<std::string_view>& all,
    const std::unordered_map<std::string_view, file_fact>& facts,
    ct::diagnostic_table& out) {
    const ops::file_set files(all);
    include_graph graph(files, include_roots(all), facts);

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

        auto headers = under(all, std::string(library) + "/include/",
            ".hpp");
        std::ranges::sort(headers, ops::component_less);
        using exports = std::vector<ops::exported_type>;
        std::vector<std::pair<std::string_view, exports>> exporting;
        std::unordered_set<std::string> pending;
        for (const auto header : headers) {
            auto types = fact_of(header).types;
            std::erase_if(types, [&](const ops::exported_type& t) {
                return t.macro != *macro;
            });
            if (types.empty())
                continue;
            exporting.emplace_back(header, std::move(types));
            pending.emplace(header);
        }

        // The walk stops once every exporting header is reached, as the
        // rest of it cannot change a finding.
        std::unordered_set<std::string> reached;
        std::vector<std::string> queue;
        for (const auto p : under(all, sources + "/", ".cpp"))
            queue.emplace_back(p);
        while (!queue.empty() && !pending.empty()) {
            auto current = std::move(queue.back());
            queue.pop_back();
            if (!reached.insert(current).second)
                continue;
            pending.erase(current);
            for (const auto& target : graph.targets(current))
                queue.push_back(target);
        }

        for (const auto& [header, types] : exporting) {
            if (!pending.contains(std::string(header)))
                continue;
            std::string names;
            for (const auto& t : types)
                names += (names.empty() ? "" : ", ") + std::string(t.name);
            const auto at = types.front().offset;
            out.add({.document = compass::org::types::document_id::none(),
                .range = {at, at},
                .rule = uncompiled,
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

/**
 * @brief Reports each enum whose identifier before its name is a
 * visibility macro: one of Boost's, or one a scanned file defines.
 */
void report_decorated_enums(std::span<const ct::scanned_file> scanned_files,
    const std::unordered_map<std::string_view, file_fact>& facts,
    ct::diagnostic_table& out) {
    std::set<std::string_view> macros{"BOOST_SYMBOL_EXPORT",
        "BOOST_SYMBOL_IMPORT"};
    for (const auto& [path, fact] : facts)
        macros.insert(fact.visibility_macros.begin(),
            fact.visibility_macros.end());
    for (const auto& f : scanned_files) {
        for (const auto& e : facts.at(f.path).enums) {
            if (!macros.contains(e.macro))
                continue;
            out.add({.document = compass::org::types::document_id::none(),
                .range = {e.offset, e.offset},
                .rule = decorated_enum,
                .level = ct::severity::error,
                .message = "the visibility macro " + std::string(e.macro) +
                    " decorates an enum, and the attribute does not apply "
                    "to one: " + std::string(e.source),
                .fix = "drop the macro; an enum needs no export attribute "
                    "to cross a binary boundary",
                .file = f.path});
        }
    }
}

void reduce(std::span<const ct::scanned_file> scanned_files,
    const ct::corpus_view&, ct::diagnostic_table& out) {
    std::unordered_map<std::string_view, file_fact> facts;
    std::vector<std::string_view> all;
    all.reserve(scanned_files.size());
    for (const auto& f : scanned_files) {
        facts.emplace(f.path, read_fact(f.fact));
        all.push_back(f.path);
    }
    std::ranges::sort(all);
    report_uncompiled(all, facts, out);
    report_decorated_enums(scanned_files, facts, out);
}

}

const ct::descriptor exported_types{
    .name = "exported_types",
    .version = 3,
    .summary = "Reports each header whose library exports a type that no "
        "translation unit of the library compiles, and each enum a "
        "visibility macro decorates.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::scan_function{scanned, map, reduce}};

}
