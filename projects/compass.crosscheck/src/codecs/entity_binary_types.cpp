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
#include "compass.crosscheck/codecs/entity_binary_types.hpp"

#include <array>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/glob.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.crosscheck/ops/words.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

constexpr std::uint32_t wire_mismatch = 0;
constexpr std::uint32_t unclassified = 1;
constexpr std::uint32_t no_table = 2;
constexpr std::uint32_t too_few_compared = 3;

constexpr std::array<ct::rule, 4> rules{{
    {"wire_mismatch", "A generated row binds each column in the binary "
        "format its column's type defines, to a column its table creates "
        "and a member its struct declares."},
    {"unclassified", "Every bound member and column has a type the check "
        "can place on the wire, or both are text."},
    {"no_table", "Every generated row's table is created by some create "
        "script, or is a view."},
    {"too_few_compared", "At least min_compared binary columns are "
        "compared, so the check is reading the tree."}}};

constexpr std::array<ct::option, 1> options{{
    {"min_compared", "500", "The fewest binary columns a run may compare."}}};

/**
 * @brief A wire format: the type class, and the width of an integer.
 */
using wire = std::pair<std::string_view, int>;

const std::map<std::string_view, wire, std::less<>>& cpp_wires() {
    static const std::map<std::string_view, wire, std::less<>> out{
        {"bool", {"boolean", 0}}, {"std::string", {"text", 0}},
        {"std::string_view", {"text", 0}},
        {"boost::uuids::uuid", {"uuid", 0}},
        {"utility::uuid::tenant_id", {"uuid", 0}},
        {"std::chrono::system_clock::time_point", {"timestamptz", 0}},
        {"db_timestamp", {"timestamptz", 0}},
        {"std::chrono::year_month_day", {"date", 0}},
        {"ores::utility::decimal::decimal", {"numeric", 0}},
        {"decimal", {"numeric", 0}},
        {"double", {"double precision", 0}}, {"float", {"real", 0}},
        {"int", {"integer", 32}}, {"std::int32_t", {"integer", 32}},
        {"std::uint32_t", {"integer", 32}}, {"std::int64_t", {"integer", 64}},
        {"std::uint64_t", {"integer", 64}}, {"std::size_t", {"integer", 64}},
        {"long", {"integer", 64}}, {"long long", {"integer", 64}},
        {"std::int16_t", {"integer", 16}}, {"std::uint16_t", {"integer", 16}},
        {"short", {"integer", 16}}};
    return out;
}

const std::map<std::string_view, wire, std::less<>>& sql_wires() {
    static const std::map<std::string_view, wire, std::less<>> out{
        {"text", {"text", 0}}, {"character varying", {"text", 0}},
        {"varchar", {"text", 0}}, {"uuid", {"uuid", 0}},
        {"boolean", {"boolean", 0}}, {"bool", {"boolean", 0}},
        {"date", {"date", 0}},
        {"timestamp with time zone", {"timestamptz", 0}},
        {"timestamp without time zone", {"timestamp", 0}},
        {"timestamp", {"timestamp", 0}},
        {"double precision", {"double precision", 0}},
        {"real", {"real", 0}}, {"inet", {"inet", 0}},
        {"jsonb", {"jsonb", 0}}, {"integer", {"integer", 32}},
        {"int", {"integer", 32}}, {"int4", {"integer", 32}},
        {"bigint", {"integer", 64}}, {"int8", {"integer", 64}},
        {"smallint", {"integer", 16}}, {"int2", {"integer", 16}}};
    return out;
}

std::string_view unwrap(std::string_view t, std::string_view open) {
    if (t.starts_with(open) && t.ends_with('>'))
        return co::strip(t.substr(open.size(), t.size() - open.size() - 1));
    return t;
}

std::optional<wire> wire_of_cpp(std::string_view cpp_type) {
    auto t = unwrap(co::strip(cpp_type), "std::optional<");
    if (t.starts_with("const "))
        t.remove_prefix(6);
    t = unwrap(t, "std::vector<");
    const auto it = cpp_wires().find(t);
    if (it == cpp_wires().end())
        return std::nullopt;
    return it->second;
}

std::string rstrip_commas(std::string_view s) {
    while (!s.empty() && (s.back() == ',' || s.back() == ' '))
        s.remove_suffix(1);
    return std::string(s);
}

std::optional<wire> wire_of_sql(std::string_view sql_type) {
    static const pt::regex sized(
        R"(^(?:character varying|varchar|character|char)\s*\(\s*\d+\s*\)$)");
    auto t = rstrip_commas(ops::lowered(co::strip(ops::collapse_spaces(
        sql_type))));
    if (t.ends_with("[]"))
        t = std::string(co::strip(std::string_view(t).substr(0,
            t.size() - 2)));
    if (t.starts_with("numeric"))
        return wire{"numeric", 0};
    if (sized.match(t))
        return wire{"text", 0};
    const auto it = sql_wires().find(t);
    if (it == sql_wires().end())
        return std::nullopt;
    return it->second;
}

std::string describe(const wire& w) {
    return std::string(w.first) + (w.second == 0 ? ""
        : " " + std::to_string(w.second));
}

std::string read(const ct::corpus_view& v, std::string_view path) {
    return pt::universal_newlines(pt::decode_replacing(
        v.read_file(path).value_or("")));
}

void created_columns(const ct::corpus_view& v,
    std::map<std::string, std::map<std::string, std::string>>& tables,
    std::set<std::string>& views) {
    static const pt::regex create_table(
        R"re(create +table +(?:if +not +exists +)?"?([a-z0-9_]+)"? *\((.*?)\) *;)re",
        {.dotall = true, .ignorecase = true});
    static const pt::regex create_view(
        R"re(create +(?:or +replace +)?view +"?([a-z0-9_]+)"?)re",
        {.ignorecase = true});
    static const pt::regex create_column(R"re(^ *,? *"([a-z0-9_]+)" +(.+)$)re",
        {.ignorecase = true});
    static const pt::regex constraint(
        R"( +(?:not +null|null|default|primary +key|references|check|unique|generated|collate|constraint)\b)",
        {.ignorecase = true});
    for (const auto path : co::glob(v,
             "projects/ores.sql/create/*/*_create.sql")) {
        const auto text = read(v, path);
        for (const auto& m : create_view.find_all(text))
            views.emplace(m.group(1));
        for (const auto& m : create_table.find_all(text)) {
            auto& columns = tables[std::string(m.group(1))];
            for (const auto line : co::split_lines(m.group(2))) {
                const auto c = create_column.match(line);
                if (!c)
                    continue;
                auto declared = c->group(2);
                if (const auto k = constraint.search(declared))
                    declared = declared.substr(0, k->start());
                columns.emplace(std::string(c->group(1)), rstrip_commas(
                    co::strip(ops::collapse_spaces(declared))));
            }
        }
    }
}

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    static const pt::regex tablename(
        R"re(constexpr static const char\* tablename = "(\w+)")re");
    static const pt::regex member(
        R"(^ {4}([A-Za-z_][A-Za-z0-9_:<>, ]*?) +([a-z_][A-Za-z0-9_]*)(?:\{\}| *= *[^;]*)?;$)",
        {.multiline = true});
    static const pt::regex binding(
        R"re(query_builder::(bind|bind_text|key|key_text)\("([a-z0-9_]+)" *, *&e::([a-z_][a-z0-9_]*) *\))re");

    std::map<std::string, std::map<std::string, std::string>> tables;
    std::set<std::string> views;
    created_columns(v, tables, views);

    std::size_t compared = 0;
    for (const auto path : co::glob(v,
             "projects/**/repository/*_entity.hpp")) {
        const auto text = read(v, path);
        if (!text.contains("AUTO-GENERATED FILE") ||
            !text.contains("static constexpr auto columns()"))
            continue;
        const auto found = tablename.search(text);
        if (!found)
            continue;
        const std::string table(found->group(1));
        if (views.contains(table))
            continue;
        const auto created = tables.find(table);
        if (created == tables.end()) {
            co::report(out, path, 0, no_table, "no create script creates " +
                table, "Generate the table's create script, or name the "
                "table the script creates.");
            continue;
        }
        const auto& columns = created->second;
        std::map<std::string, std::string> members;
        for (const auto& m : member.find_all(text)) {
            const auto type = co::strip(m.group(1));
            if (type.starts_with("constexpr") || type.starts_with("static") ||
                type.starts_with("using") || type.starts_with("return") ||
                type.starts_with("struct"))
                continue;
            members.emplace(std::string(m.group(2)), std::string(type));
        }
        const auto flat = ops::collapse_spaces(text);
        for (const auto& b : binding.find_all(flat)) {
            const auto kind = b.group(1);
            const std::string column(b.group(2));
            const std::string name(b.group(3));
            if (kind != "bind" && kind != "key")
                continue;
            const auto declared = columns.find(column);
            if (declared == columns.end()) {
                co::report(out, path, 0, wire_mismatch, "the row binds '" +
                    column + "', which " + table + " does not create",
                    "Regenerate the row and the table from one model.");
                continue;
            }
            const auto cpp = members.find(name);
            if (cpp == members.end()) {
                co::report(out, path, 0, wire_mismatch, "the row binds '" +
                    name + "' but the member is not declared in the struct",
                    "Regenerate the row from its model.");
                continue;
            }
            const auto cpp_wire = wire_of_cpp(cpp->second);
            const auto sql_wire = wire_of_sql(declared->second);
            if (!cpp_wire) {
                co::report(out, path, 0, unclassified, "'" + column +
                    "' holds " + cpp->second + ", which this check cannot "
                    "place on the wire", "Teach the check the type's "
                    "format.");
                continue;
            }
            if (!sql_wire) {
                if (*cpp_wire == wire{"text", 0})
                    continue;
                co::report(out, path, 0, unclassified, "'" + column +
                    "' holds " + cpp->second + " but " + table +
                    " declares it " + declared->second + ", a type this "
                    "check cannot place on the wire", "Teach the check the "
                    "type's format.");
                continue;
            }
            ++compared;
            if (*cpp_wire != *sql_wire)
                co::report(out, path, 0, wire_mismatch, "'" + column +
                    "' holds " + cpp->second + " (" + describe(*cpp_wire) +
                    ") but " + table + " creates it " + declared->second +
                    " (" + describe(*sql_wire) + ")", "Make the model's C++ "
                    "and SQL types agree, then regenerate.");
        }
    }
    if (compared < std::stoul(std::string(v.option("min_compared"))))
        co::report(out, "projects", 0, too_few_compared, "compared only " +
            std::to_string(compared) + " binary columns, below the floor: "
            "the check is no longer reading the tree", "Teach the check the "
            "format, or lower min_compared for a subtree.");
}

}

const ct::descriptor entity_binary_types{
    .name = "entity_binary_types",
    .version = 1,
    .summary = "Reports each generated row that binds a column in a binary "
        "format its table's type does not define.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}
