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
#include "compass.cpp/codecs/forward_declarable_includes.hpp"

#include <algorithm>
#include <array>
#include <expected>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/declarations.hpp"
#include "compass.cpp/ops/lexer.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/source_model.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::cpp::codecs {

namespace {

namespace ct = compass::codec::types;
namespace pt = compass::platform::text;

constexpr std::array<ct::rule, 4> rules{{
    {"includes_definition_it_only_names", "A header that only names a "
        "listed type includes its declaration, not its definition."},
    {"bad_type_list", "The list of forward-declarable types is readable "
        "and not empty."},
    {"stale_type_list", "The types option lists what the tree's own list "
        "file lists."},
    {"type_not_defined", "A listed type's defining header, where the tree "
        "has it, defines the type."}}};

constexpr std::array<ct::option, 2> declared_options{{
    {"types",
        "ores::database::context;class;ores.database/domain/context.hpp;"
        "ores.database/domain/context_fwd.hpp,"
        "ores::database::pq::connection;class;ores.database/pq/connection.hpp;"
        "ores.database/pq/connection_fwd.hpp,"
        "ores::database::pq::pool;class;ores.database/pq/pool.hpp;-,"
        "ores::database::pq::result;class;ores.database/pq/result.hpp;-,"
        "ores::database::pq::session_context;struct;"
        "ores.database/pq/session_context.hpp;"
        "ores.database/pq/session_context_fwd.hpp,"
        "ores::database::pq::transaction;class;"
        "ores.database/pq/transaction.hpp;-",
        "The types a header may name without their definition, "
        "comma-separated, each name;class key;defining header;forwarding "
        "header or -: ORE Studio's forward_declarable_types.list."},
    {"list_file", "build/scripts/forward_declarable_types.list",
        "The tree's own list of the types, one per line as four fields "
        "separated by spaces, compared with the types option when the "
        "tree has it; empty compares nothing."}}};

constexpr std::array<std::string_view, 3> header_suffixes{".hpp", ".h",
    ".ipp"};

constexpr std::array<std::string_view, 5> skipped{"venv", ".venv",
    "node_modules", "build", "_deps"};

constexpr std::string_view generated_marker =
    "AUTO-GENERATED FILE - DO NOT EDIT MANUALLY";

struct declarable {
    std::string name;
    std::string keyword;
    std::string defining_header;
    std::optional<std::string> forwarding_header;

    [[nodiscard]] std::string_view short_name() const {
        return std::string_view(name).substr(name.rfind("::") + 2);
    }

    [[nodiscard]] std::string_view name_space() const {
        return std::string_view(name).substr(0, name.rfind("::"));
    }

    /**
     * @brief pq::, database::pq::, ores::database::pq::: each suffix of the
     * namespace, outermost last.
     */
    [[nodiscard]] std::vector<std::string> qualifiers() const {
        std::vector<std::string> out;
        const auto ns = name_space();
        out.push_back(std::string(ns) + "::");
        for (std::size_t at = ns.find("::"); at != std::string_view::npos;
             at = ns.find("::", at + 2))
            out.push_back(std::string(ns.substr(at + 2)) + "::");
        return out;
    }
};

/**
 * @brief The regex of @p pattern, compiled once per thread. The patterns
 * are built from the listed types, so the reduce step asks for the same
 * ones for every header that includes a type's defining header.
 */
const pt::regex& compiled(const std::string& pattern) {
    thread_local std::unordered_map<std::string, pt::regex> cache;
    const auto it = cache.find(pattern);
    if (it != cache.end())
        return it->second;
    return cache.emplace(pattern, pt::regex(pattern)).first->second;
}

bool opens_namespace(const declarable& d, std::string_view text) {
    const auto ns = std::string(d.name_space());
    if (compiled(R"(\busing\s+namespace\s+)" + pt::escape(ns) + R"(\s*;)")
            .search(text))
        return true;
    if (compiled(R"(\bnamespace\s+)" + pt::escape(ns) + R"(\b[^;]*\{)")
            .search(text))
        return true;
    const auto last = ns.rfind("::");
    if (last == std::string::npos)
        return false;
    return compiled(R"(\bnamespace\s+)" + pt::escape(ns.substr(0, last)) +
               R"(\b[^;]*\{)").search(text) &&
        compiled(R"(\bnamespace\s+)" + pt::escape(ns.substr(last + 2)) +
            R"(\b)").search(text);
}

bool binds_short_name(const declarable& d, std::string_view text) {
    const auto s = pt::escape(d.short_name());
    return compiled(R"(\busing\s+)" + s + R"(\s*=\s*(?:\w+::)*)" + s +
        R"(\s*;)").search(text).has_value();
}

std::optional<std::string> enclosing_template(std::string_view text,
    std::size_t position) {
    static const pt::regex name_before(R"(([A-Za-z_]\w*)\s*$)");
    int depth = 0;
    for (auto index = static_cast<long>(position) - 1; index >= 0; --index) {
        const char c = text[static_cast<std::size_t>(index)];
        if (c == '>') {
            ++depth;
        } else if (c == '<') {
            if (depth == 0) {
                const auto m = name_before.search(
                    text.substr(0, static_cast<std::size_t>(index)));
                if (!m)
                    return std::nullopt;
                return std::string(m->group(1));
            }
            --depth;
        } else if ((c == ';' || c == '{' || c == '}') && depth == 0) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

enum class verdict { names, needs };

verdict classify(std::string_view text, std::size_t start,
    std::size_t end) {
    static const pt::regex key_before(
        R"((?:^|[^\w:])(?:class|struct|enum\s+class)\s*$)");
    static const pt::regex alias_before(R"(\b(?:using|typedef)\b[^;{}]*$)");
    const auto before = text.substr(0, start);
    auto following = text.substr(end);
    following.remove_prefix(std::min(following.size(),
        ops::skip_spaces(following, 0)));
    if (key_before.search(before))
        return verdict::names;
    if (following.starts_with('*') || following.starts_with('&'))
        return verdict::names;
    if (alias_before.search(before))
        return verdict::names;
    if (following.starts_with('>') || following.starts_with(',') ||
        following.starts_with(')')) {
        const auto t = enclosing_template(text, start);
        if (t == "shared_ptr" || t == "weak_ptr")
            return verdict::names;
    }
    return verdict::needs;
}

std::set<std::string> bound_names(const declarable& d,
    std::string_view text) {
    std::set<std::string> names;
    auto qualifiers = d.qualifiers();
    qualifiers.emplace_back();
    for (const auto& q : qualifiers) {
        const auto spelling = pt::escape(q) + pt::escape(d.short_name());
        const auto& parameter = compiled(
            R"((?<![\w:])(?:const\s+)?)" + spelling +
            R"(\s*(?:const\s*)?[&*]+\s*(?:const\s+)?([A-Za-z_]\w*))");
        for (const auto& m : parameter.find_all(text))
            names.emplace(m.group(1));
        const auto& member = compiled(
            R"(\b(?:shared_ptr|weak_ptr|unique_ptr)\s*<\s*)" + spelling +
            R"(\s*>\s*([A-Za-z_]\w*))");
        for (const auto& m : member.find_all(text))
            names.emplace(m.group(1));
    }
    return names;
}

bool dereferences(std::string_view text, const std::set<std::string>& names) {
    return std::ranges::any_of(names, [&](const auto& n) {
        return compiled(R"(\b)" + pt::escape(n) + R"(\s*(?:\.|->))")
            .search(text).has_value();
    });
}

std::optional<verdict> scan(const declarable& d, std::string_view text) {
    std::set<verdict> verdicts;
    const auto short_name = pt::escape(d.short_name());
    for (const auto& q : d.qualifiers()) {
        const auto& use = compiled(R"((?<![\w:]))" + pt::escape(q) +
            short_name + R"((?![\w]))");
        for (const auto& m : use.find_all(text))
            verdicts.insert(classify(text, m.start(), m.end()));
    }
    if (opens_namespace(d, text) || binds_short_name(d, text)) {
        const auto& use = compiled(R"((?<![\w:]))" + short_name +
            R"((?![\w]))");
        for (const auto& m : use.find_all(text))
            verdicts.insert(classify(text, m.start(), m.end()));
    }
    if (verdicts.contains(verdict::needs))
        return verdict::needs;
    if (dereferences(text, bound_names(d, text)))
        return verdict::needs;
    if (verdicts.empty())
        return std::nullopt;
    return verdict::names;
}

bool is_header(std::string_view path) {
    const auto parts = ops::components_of(path);
    for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
        if (std::ranges::find(skipped, parts[i]) != skipped.end())
            return false;
    }
    return std::ranges::find(header_suffixes, ops::suffix_of(path)) !=
        header_suffixes.end();
}

std::string relative_include(std::string_view path) {
    const auto parts = ops::components_of(path);
    for (auto i = parts.size(); i-- > 0;) {
        if (parts[i] != "include")
            continue;
        std::string out;
        for (auto k = i + 1; k < parts.size(); ++k) {
            if (!out.empty())
                out += '/';
            out += parts[k];
        }
        return out;
    }
    return std::string(parts.back());
}

std::vector<std::string_view> scanned(const ct::corpus_view& v) {
    std::vector<std::string_view> headers;
    for (const auto suffix : header_suffixes) {
        for (const auto p : v.list_files("projects/", suffix)) {
            if (is_header(p))
                headers.push_back(p);
        }
    }
    std::ranges::sort(headers, ops::component_less);
    return headers;
}

/**
 * @brief The type of one row's four fields, or why it is not one.
 */
std::expected<declarable, std::string> declarable_of(
    const std::vector<std::string>& fields) {
    if (fields.size() != 4)
        return std::unexpected("has " + std::to_string(fields.size()) +
            " fields, not four");
    if (fields[1] != "class" && fields[1] != "struct")
        return std::unexpected("gives " + fields[0] + " the class key " +
            fields[1] + ", which must be class or struct");
    return declarable{fields[0], fields[1], fields[2],
        fields[3] == "-" ? std::nullopt
            : std::optional<std::string>(fields[3])};
}

/**
 * @brief The row of @p d as the types option spells it.
 */
std::string row_of(const declarable& d) {
    return d.name + ";" + d.keyword + ";" + d.defining_header + ";" +
        d.forwarding_header.value_or("-");
}

/**
 * @brief The types of the types option, or why they cannot be read: each
 * row name;key;defining header;forwarding header or -, rows separated by
 * commas.
 */
std::expected<std::vector<declarable>, std::string> parse_types(
    std::string_view value) {
    std::vector<declarable> out;
    for (std::size_t start = 0; start < value.size();) {
        auto end = value.find(',', start);
        if (end == std::string_view::npos)
            end = value.size();
        const auto row = value.substr(start, end - start);
        start = end + 1;
        if (row.empty())
            continue;
        std::vector<std::string> fields;
        for (std::size_t f = 0; f <= row.size();) {
            auto e = row.find(';', f);
            if (e == std::string_view::npos)
                e = row.size();
            fields.emplace_back(row.substr(f, e - f));
            f = e + 1;
        }
        auto d = declarable_of(fields);
        if (!d)
            return std::unexpected("the row " + std::string(row) + " " +
                d.error());
        out.push_back(std::move(*d));
    }
    if (out.empty())
        return std::unexpected("no type is listed, so the guard is off");
    return out;
}

/**
 * @brief The types of a list file, or why they cannot be read, as ORE
 * Studio's script reads forward_declarable_types.list: one per line as
 * four fields separated by whitespace, blank lines and lines starting
 * with # skipped.
 */
std::expected<std::vector<declarable>, std::string> parse_list(
    std::string_view text) {
    static const pt::regex field(R"(\S+)");
    std::vector<declarable> out;
    std::size_t number = 0;
    for (std::size_t start = 0; start < text.size();) {
        auto end = text.find('\n', start);
        if (end == std::string_view::npos)
            end = text.size();
        const auto line = text.substr(start, end - start);
        start = end + 1;
        ++number;
        std::vector<std::string> fields;
        for (const auto& m : field.find_all(line))
            fields.emplace_back(m.group(0));
        if (fields.empty() || fields[0].starts_with('#'))
            continue;
        auto d = declarable_of(fields);
        if (!d)
            return std::unexpected("line " + std::to_string(number) + " " +
                d.error());
        out.push_back(std::move(*d));
    }
    if (out.empty())
        return std::unexpected("no type is listed");
    return out;
}

/**
 * @brief Each type whose row differs between @p file and @p option,
 * named with both rows; empty when they list the same rows.
 */
std::vector<std::string> differences(const std::vector<declarable>& file,
    const std::vector<declarable>& option) {
    std::map<std::string, std::pair<std::string, std::string>> rows;
    for (const auto& d : file)
        rows[d.name].first = row_of(d);
    for (const auto& d : option)
        rows[d.name].second = row_of(d);
    std::vector<std::string> out;
    for (const auto& [name, both] : rows) {
        const auto& [in_file, in_option] = both;
        if (in_file == in_option)
            continue;
        if (in_option.empty())
            out.push_back(in_file + " is only in the file");
        else if (in_file.empty())
            out.push_back(in_option + " is only in the option");
        else
            out.push_back("the file has " + in_file + " where the option "
                "has " + in_option);
    }
    return out;
}

/**
 * @brief Whether the bytes of the header @p bytes define @p d, as the
 * model reads the class and struct definitions at namespace scope.
 */
bool defines(std::string_view bytes, const declarable& d) {
    const auto lexed = ops::lex(bytes);
    return ops::defined_types(ops::model_of(bytes, lexed,
        ops::model_parts::declarations)).contains(d.name);
}

/**
 * @brief The listed types of the types option, parsed once per value.
 */
const std::vector<declarable>& listed(std::string_view value) {
    thread_local std::string parsed_value;
    thread_local std::vector<declarable> parsed;
    if (parsed_value != value || parsed.empty()) {
        parsed_value = value;
        parsed = parse_types(value).value_or(std::vector<declarable>{});
    }
    return parsed;
}

/**
 * @brief Decides the header alone: for each listed type whose defining
 * header it includes, other than that header itself, whether every use
 * only names the type, read from its lexed code with the include lines
 * blanked. Reported on the line of the include.
 */
void map(std::string_view path, std::string_view bytes,
    const ct::option_values& options, ct::file_scan& out) {
    const auto& types = listed(options["types"]);
    if (std::ranges::none_of(types, [&](const declarable& d) {
            return bytes.find(d.defining_header) != std::string_view::npos; }))
        return;
    const auto lexed = ops::lex(bytes);
    const auto model = ops::model_of(bytes, lexed,
        ops::model_parts::directives);
    const auto own = relative_include(path);
    std::vector<std::pair<const declarable*, std::uint32_t>> candidates;
    for (const auto& i : model.includes) {
        for (const auto& d : types) {
            if (d.defining_header == i.target && d.defining_header != own &&
                std::ranges::none_of(candidates,
                    [&](const auto& c) { return c.first == &d; }))
                candidates.emplace_back(&d, i.offset);
        }
    }
    if (candidates.empty())
        return;

    auto code = lexed.code;
    for (const auto d : lexed.directives) {
        if (ops::parts_of(lexed, d).keyword != "include")
            continue;
        for (auto k = d.begin; k < d.end; ++k) {
            if (code[k] != '\n')
                code[k] = ' ';
        }
    }
    const auto body = pt::universal_newlines(pt::decode_ignoring(code));
    std::optional<std::string> generated_by;
    if (bytes.find(generated_marker) != std::string_view::npos) {
        static const pt::regex template_re(
            R"(^\s*\*\s*Template:\s*(\S+)\s*$)", {.multiline = true});
        const auto text = pt::universal_newlines(pt::decode_ignoring(bytes));
        const auto head = std::string_view(text).substr(0,
            std::min(text.size(), ops::byte_of_character(text, 2000)));
        const auto m = template_re.search(head);
        generated_by = m ? std::string(m->group(1)) : "unknown";
    }
    for (const auto& [d, offset] : candidates) {
        if (scan(*d, body) != verdict::names)
            continue;
        const auto key = d->name + ":" + (generated_by
            ? "template:" + *generated_by : std::string(path));
        ops::report(out.findings, path, ops::offset_of_line(bytes,
                ops::line_of(bytes, offset)), 0,
            "the header includes " + d->defining_header + " but only "
            "names " + d->name + " (" + key + ")",
            d->forwarding_header
                ? "include " + *d->forwarding_header + " instead"
                : "declare " + d->keyword + " " + d->name + ";");
    }
}

/**
 * @brief Reports a types option that cannot be read, a list file that
 * lists other types than the option, and a listed type its defining
 * header, where the tree has it, does not define. The list file and the
 * defining headers are read through the view, so an edit to one calls the
 * reduce again. The headers are decided in map.
 */
void reduce(std::span<const ct::scanned_file> files, const ct::corpus_view& v,
    ct::diagnostic_table& out) {
    const auto types = parse_types(v.option("types"));
    if (!types) {
        ops::report(out, ".compass.org", 0, 1, "the forward-declarable "
            "types cannot be read: " + types.error(), "write each type as "
            "name;class or struct;defining header;forwarding header or -, "
            "comma-separated");
        return;
    }

    const auto list_file = v.option("list_file");
    if (!list_file.empty() && v.file_exists(list_file)) {
        const auto bytes = v.read_file(list_file);
        const auto file = bytes ? parse_list(*bytes)
            : std::expected<std::vector<declarable>, std::string>(
                std::unexpect, "the file cannot be read");
        if (!file) {
            ops::report(out, list_file, 0, 2, "the forward-declarable types "
                "of " + std::string(list_file) + " cannot be read, so the "
                "types option cannot be compared with them: " +
                file.error(), "write each type on its own line as name, "
                "class or struct, defining header and forwarding header or "
                "-, separated by spaces, or set list_file= to compare "
                "nothing");
        } else if (const auto diff = differences(*file, *types);
                   !diff.empty()) {
            std::string value;
            for (const auto& d : *file)
                value += (value.empty() ? "" : ",") + row_of(d);
            ops::report(out, list_file, 0, 2, "the types option of "
                "forward_declarable_includes differs from " +
                std::string(list_file) + ": " + ops::listed(diff),
                "set types=" + value + " on the forward_declarable_includes "
                "row of .compass.org");
        }
    }

    for (const auto& d : *types) {
        std::vector<std::string_view> headers;
        for (const auto& f : files) {
            if (relative_include(f.path) == d.defining_header)
                headers.push_back(f.path);
        }
        if (headers.empty() || std::ranges::any_of(headers, [&](auto h) {
                return defines(v.read_file(h).value_or(""), d); }))
            continue;
        ops::report(out, headers.front(), 0, 3, "the types option names " +
            d.defining_header + " as the defining header of " + d.name +
            ", but it does not define " + d.name, "set the row of " +
            d.name + " in the types option to the header that defines it");
    }
}

}

const ct::descriptor forward_declarable_includes{
    .name = "forward_declarable_includes",
    .version = 5,
    .summary = "Reports each header that includes the definition of a "
        "listed type it only names.",
    .reads = ct::inputs::of_types({}),
    .options = declared_options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::scan_function{scanned, map, reduce}};

}
