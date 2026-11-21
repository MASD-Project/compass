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
#include "compass.model/ops/builder.hpp"
#include "compass.model/ops/load.hpp"

namespace compass::cpp::codecs {

namespace {

namespace ct = compass::codec::types;
namespace mo = compass::model::ops;
namespace mt = compass::model::types;

constexpr std::array<ct::rule, 3> rules{{
    {"uncompiled_exported_type",
        "No library exports a type that none of its translation units "
        "compiles."},
    {"visibility_macro_on_enum",
        "No symbol visibility or DLL storage macro decorates an enum."},
    {"unexported_hidden_function",
        "A part built with hidden visibility exports every function its "
        "public headers declare at namespace scope."}}};

constexpr ct::rule_id uncompiled{0};
constexpr ct::rule_id decorated_enum{1};
constexpr ct::rule_id unexported_function{2};

/**
 * @brief The fact of a file: its quoted includes, the export macro it
 * defines when it is a part's export header, the types it exports with
 * any macro, the visibility macros it defines, the functions it declares
 * that a part with hidden symbols must export, when it is a header under
 * an include directory, and each enum an identifier stands before.
 */
struct file_fact {
    std::vector<std::string_view> includes;
    std::optional<std::string_view> macro;
    std::vector<ops::exported_type> types;
    std::vector<std::string_view> visibility_macros;
    std::vector<ops::exportable_function> functions;
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
    for (auto n = r.number(); n > 0; --n) {
        ops::exportable_function f;
        f.offset = r.number();
        f.name = r.text();
        for (auto k = r.number(); k > 0; --k)
            f.decorations.push_back(r.text());
        out.functions.push_back(std::move(f));
    }
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

/**
 * @brief The directory of the part whose export header @p path is, for
 * projects/<component>/<part>/include/<name>/export.hpp and for a flat
 * component's projects/<component>/include/<name>/export.hpp; nothing
 * for any other path.
 */
std::optional<std::string_view> part_of_export_header(std::string_view path) {
    const auto parts = ops::components_of(path);
    if ((parts.size() != 5 && parts.size() != 6) || parts[0] != "projects" ||
        parts[parts.size() - 3] != "include" || parts.back() != "export.hpp")
        return std::nullopt;
    return ops::parent_of(ops::parent_of(ops::parent_of(path)));
}

/**
 * @brief True for a header under the include directory of a part.
 */
bool is_public_header(std::string_view path) {
    if (!path.ends_with(".hpp"))
        return false;
    const auto parts = ops::components_of(path);
    return parts.size() > 4 && parts[0] == "projects" &&
        (parts[2] == "include" || parts[3] == "include");
}

void map(std::string_view path, std::string_view bytes,
    const ct::option_values&, ct::file_scan& out) {
    const auto lexed = ops::lex(bytes);
    const bool is_public = is_public_header(path);
    const auto model = ops::model_of(bytes, lexed, is_public
        ? ops::model_parts::heads | ops::model_parts::declarations
        : ops::model_parts::heads);
    ct::fact_writer w(out.fact);
    const auto includes = ops::quoted_includes(model);
    w.number(static_cast<std::uint32_t>(includes.size()));
    for (const auto i : includes)
        w.text(i);
    const auto macro = part_of_export_header(path)
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
    const auto functions = is_public
        ? ops::exportable_functions(lexed, model)
        : std::vector<ops::exportable_function>{};
    w.number(static_cast<std::uint32_t>(functions.size()));
    for (const auto& f : functions) {
        w.number(f.offset).text(f.name);
        w.number(static_cast<std::uint32_t>(f.decorations.size()));
        for (const auto d : f.decorations)
            w.text(d);
    }
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

/**
 * @brief True when the component model of the overview at @p path says
 * its part is built with hidden visibility; false when there is no such
 * overview or it does not load.
 */
bool hides_symbols(const ct::corpus_view& v, const std::string& path) {
    const auto d = v.find_path(path);
    if (!d)
        return false;
    const mo::document_finder find = [&v](std::string_view p)
        -> std::optional<mo::document> {
        const auto found = v.find_path(p);
        if (!found)
            return std::nullopt;
        return mo::document{found->path, found->source, &found->tables};
    };
    const mo::document_lister list = [&v](std::string_view prefix) {
        return v.list_files(prefix, ".org");
    };
    mt::model_tables tables;
    mo::builder b(tables);
    mo::load_cache cache;
    std::string error;
    const auto id = mo::load_model(b, {d->path, d->source, &d->tables},
        mt::model_type::component, find, list, cache, &error);
    if (!error.empty())
        return false;
    const auto component = b.find(tables.models.roots[id], "component");
    if (component.is_none())
        return false;
    const auto hidden = b.find(component, "hidden_visibility");
    return !hidden.is_none() &&
        tables.values.kinds[hidden] == mt::value_kind::boolean &&
        tables.values.scalars[hidden] != 0;
}

/**
 * @brief Reports each function a public header of a part built with
 * hidden visibility declares at namespace scope without the part's export
 * macro. @p all is every scanned path, sorted.
 */
void report_unexported_functions(const std::vector<std::string_view>& all,
    const std::unordered_map<std::string_view, file_fact>& facts,
    const ct::corpus_view& v, ct::diagnostic_table& out) {
    std::vector<std::string_view> export_headers;
    for (const auto p : under(all, "projects/", "/export.hpp")) {
        if (part_of_export_header(p))
            export_headers.push_back(p);
    }
    std::ranges::sort(export_headers, ops::component_less);
    for (const auto export_header : export_headers) {
        const auto macro = facts.at(export_header).macro;
        if (!macro)
            continue;
        const std::string part(*part_of_export_header(export_header));
        if (!hides_symbols(v, part + "/modeling/component_overview.org"))
            continue;
        auto headers = under(all, part + "/include/", ".hpp");
        std::ranges::sort(headers, ops::component_less);
        for (const auto header : headers) {
            for (const auto& f : facts.at(header).functions) {
                if (std::ranges::contains(f.decorations, *macro))
                    continue;
                out.add({.document =
                        compass::org::types::document_id::none(),
                    .range = {f.offset, f.offset},
                    .rule = unexported_function,
                    .level = ct::severity::error,
                    .message = "declares " + std::string(f.name) +
                        " without " + std::string(*macro) + ", and " +
                        part + " is built with hidden visibility, so no "
                        "binary outside the library can link it",
                    .fix = "mark the declaration " + std::string(*macro) +
                        ", or define the function inline in the header",
                    .file = header});
            }
        }
    }
}

void reduce(std::span<const ct::scanned_file> scanned_files,
    const ct::corpus_view& v, ct::diagnostic_table& out) {
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
    report_unexported_functions(all, facts, v, out);
}

}

const ct::descriptor exported_types{
    .name = "exported_types",
    .version = 4,
    .summary = "Reports each header whose library exports a type that no "
        "translation unit of the library compiles, each enum a "
        "visibility macro decorates, and each function a part built with "
        "hidden visibility declares without its export macro.",
    .reads = ct::inputs::every_document(),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::scan_function{scanned, map, reduce}};

}
