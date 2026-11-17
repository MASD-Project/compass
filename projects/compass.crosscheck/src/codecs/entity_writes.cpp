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
#include "compass.crosscheck/codecs/entity_writes.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <set>
#include <span>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codec/types/fact.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

constexpr std::uint32_t sql_writer = 0;
constexpr std::uint32_t cpp_writer = 1;

constexpr std::array<std::string_view, 1> table_writes_sources{
    "5724BDE2-7FA4-4667-9129-2D9DD7783AB2"};

constexpr std::array<ct::rule, 2> rules{{
    {"sql_writer", "No SQL function but the purge writes an entity table "
        "with INSERT, UPDATE or DELETE.", table_writes_sources},
    {"cpp_writer", "No hand-written C++ file but the repository of each "
        "table it names writes an entity table in raw SQL.",
        table_writes_sources}}};

const pt::regex& dml() {
    static const pt::regex out(
        R"(\b(insert\s+into|update|delete\s+from)\s+(ores_[a-z0-9_]*_tbl)\b)",
        {.ignorecase = true});
    return out;
}

std::string lowered(std::string_view s) {
    std::string out(s);
    for (auto& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

/**
 * @brief Whether the file at @p path is the repository of every table of
 * @p tables: named <name>_repository, each table ores_<component>_<name>
 * or its plural, then _tbl.
 */
bool is_own_repository(std::string_view path,
    const std::set<std::string>& tables) {
    const auto stem = co::stem_of(path);
    constexpr std::string_view suffix = "_repository";
    if (!stem.ends_with(suffix) || stem.size() == suffix.size())
        return false;
    const pt::regex own("ores_[a-z0-9]+_" + pt::escape(stem.substr(0,
        stem.size() - suffix.size())) + "s?_tbl");
    return std::ranges::all_of(tables, [&](const auto& t) {
        return own.fullmatch(t).has_value();
    });
}

/**
 * @brief A writer the script keys by kind and name or file: where to
 * report it and what to say.
 */
struct writer {
    std::uint32_t rule;
    std::string_view file;
    std::uint32_t offset;
    std::string message;
};

void sql_writers(std::string_view path, std::string_view bytes,
    std::map<std::string, writer>& out) {
    static const pt::regex function(
        R"(create\s+(?:or\s+replace\s+)?function\s+([a-z0-9_]+)\s*\()",
        {.ignorecase = true});
    const auto text = pt::universal_newlines(pt::decode_ignoring(bytes));
    for (const auto& m : function.find_all(text)) {
        const auto opened = text.find("$$", m.end());
        if (opened == std::string::npos)
            continue;
        const auto closed = text.find("$$", opened + 2);
        if (closed == std::string::npos)
            continue;
        const std::string name(m.group(1));
        if (name.starts_with("ores_iam_purge_"))
            continue;
        const auto body = std::string_view(text).substr(opened + 2,
            closed - opened - 2);
        if (!dml().search(body))
            continue;
        out.insert_or_assign("sql_function\n" + name, writer{sql_writer,
            path, co::offset_of_line(bytes, co::line_of(text, m.start())),
            "SQL function " + name + " writes an entity table outside "
            "its repository"});
    }
}

bool is_hand_written(std::string_view path) {
    const auto parts = co::components_of(path);
    if (parts.size() < 3 || !parts[1].starts_with("ores.") ||
        parts[1] == "ores.testing")
        return false;
    return std::ranges::none_of(parts, [](auto p) {
        return p == "tests" || p == "build";
    });
}

void cpp_writers(std::string_view path, std::string_view bytes,
    std::map<std::string, writer>& out) {
    const auto text = pt::universal_newlines(pt::decode_ignoring(bytes));
    std::set<std::string> tables;
    std::size_t first = std::string::npos;
    for (const auto& m : dml().find_all(text)) {
        tables.insert(lowered(m.group(2)));
        first = std::min(first, m.start());
    }
    if (tables.empty() || is_own_repository(path, tables))
        return;
    out.insert_or_assign("cpp\n" + std::string(path), writer{
        cpp_writer, path,
        co::offset_of_line(bytes, co::line_of(text, first)),
        "writes an entity table in raw SQL, and is not the "
        "repository of every table it writes"});
}

constexpr std::string_view sql_root = "projects/ores.sql/create/";

/**
 * @brief The SQL files, then the hand-written .cpp and .hpp files, each
 * group in component order, as the writers are keyed in that order.
 */
std::vector<std::string_view> scanned(const ct::corpus_view& v) {
    auto out = v.list_files(sql_root, ".sql");
    std::ranges::sort(out, co::component_less);
    for (const auto suffix : {".cpp", ".hpp"}) {
        auto paths = v.list_files("projects/ores.", suffix);
        std::erase_if(paths, [](auto p) { return !is_hand_written(p); });
        std::ranges::sort(paths, co::component_less);
        out.insert(out.end(), paths.begin(), paths.end());
    }
    return out;
}

/**
 * @brief The fact of a file: each writer it holds, keyed as the reduce
 * keys it, with its rule, offset and message.
 */
void map(std::string_view path, std::string_view bytes,
    const ct::option_values&, ct::file_scan& out) {
    std::map<std::string, writer> found;
    if (path.starts_with(sql_root) && path.ends_with(".sql"))
        sql_writers(path, bytes, found);
    else
        cpp_writers(path, bytes, found);
    ct::fact_writer fact(out.fact);
    for (const auto& [key, w] : found)
        fact.text(key).number(w.rule).number(w.offset).text(w.message);
}

void reduce(std::span<const ct::scanned_file> files, const ct::corpus_view&,
    ct::diagnostic_table& out) {
    std::map<std::string, writer> found;
    for (const auto& f : files) {
        ct::fact_reader fact(f.fact);
        while (!fact.done()) {
            std::string key(fact.text());
            const auto rule = fact.number();
            const auto offset = fact.number();
            found.insert_or_assign(std::move(key), writer{rule, f.path,
                offset, std::string(fact.text())});
        }
    }
    for (const auto& [key, w] : found)
        co::report(out, w.file, w.offset, w.rule, w.message,
            "Write the table through its repository, or excuse the writer "
            "with a compass:except comment that says why.");
}

}

const ct::descriptor entity_writes{
    .name = "entity_writes",
    .version = 2,
    .summary = "Reports each SQL function and hand-written C++ file that "
        "writes an entity table outside its repository.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::scan_function{scanned, map, reduce}};

}
