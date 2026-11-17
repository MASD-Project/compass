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
#include "compass.crosscheck/codecs/marketdata_identity_columns.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
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

enum rule : std::uint32_t { table_columns, projector_switch, views,
    index_mapping, unreadable_source };

constexpr std::array<ct::rule, 5> rules{{
    {"table_columns", "The identity table holds the context columns, the "
        "identity fields and the index columns, and nothing else."},
    {"projector_switch", "The projector's switch names every field, places "
        "each identity field in its own column and no coordinate."},
    {"views", "Each asset class with series has one view exposing the "
        "context, its identity fields and its instrument types."},
    {"index_mapping", "The projector places exactly the index names the "
        "grammar declares, each in a column the table has."},
    {"unreadable_source", "The schema, projector, SQL and index grammar "
        "have the shapes the check reads."}}};

constexpr std::string_view schema_path = "projects/ores.marketdata/api/"
    "include/ores.marketdata.api/datum/schema.hpp";
constexpr std::string_view projector_path = "projects/ores.marketdata/core/"
    "src/repository/market_series_identity_projector.cpp";
constexpr std::string_view create_path = "projects/ores.sql/create/"
    "marketdata/marketdata_market_series_identity_create.sql";
constexpr std::string_view views_path = "projects/ores.sql/create/"
    "marketdata/marketdata_series_identity_views_create.sql";
constexpr std::string_view grammar_path = "projects/ores.marketdata/api/"
    "include/ores.marketdata.api/datum/market_index.hpp";
constexpr std::string_view table = "ores_marketdata_market_series_identity_tbl";

constexpr std::array<std::string_view, 7> view_columns{"series_id",
    "tenant_id", "party_id", "instrument_type", "quote_type", "asset_class",
    "identity_kind"};
constexpr std::array<std::string_view, 7> context_columns{"series_id",
    "tenant_id", "party_id", "identity_kind", "asset_class",
    "instrument_type", "quote_type"};

using names = std::set<std::string>;

/**
 * @brief Raised when a source no longer has the shape the check reads, as
 * the script exits; the run reports it and stops.
 */
struct unreadable {
    std::string_view path;
    std::string message;
};

std::string read(const ct::corpus_view& v, std::string_view path) {
    return pt::universal_newlines(pt::decode_replacing(
        v.read_file(path).value_or("")));
}

std::size_t index_of(std::string_view text, std::string_view what,
    std::string_view path, std::size_t from = 0) {
    const auto at = text.find(what, from);
    if (at == std::string_view::npos)
        throw unreadable{path, "does not hold '" + std::string(what) +
            "', which the check reads through"};
    return at;
}

names all_of(const pt::regex& pattern, std::string_view text,
    std::size_t group = 1) {
    names out;
    for (const auto& m : pattern.find_all(text))
        out.emplace(m.group(group));
    return out;
}

const pt::regex& identity_ref() {
    static const pt::regex out(R"(\bid(?:_or_none)?\(f::(\w+)\))");
    return out;
}

struct schema {
    names identity;
    names coordinate;
    names every;
    std::map<std::string, std::pair<names, names>> classes;
};

schema read_schema(const ct::corpus_view& v) {
    static const pt::regex coordinate_ref(R"(\bat(?:_or_none)?\(f::(\w+)\))");
    static const pt::regex enum_member(R"(^\s*(\w+),\s*$)",
        {.multiline = true});
    static const pt::regex array(
        R"(inline constexpr std::array (\w+)\{(.*?)\};)", {.dotall = true});
    static const pt::regex row(
        R"(\{detail::t::(\w+),\s*detail::(\w+),\s*asset_class::(\w+),\s*field::(\w+)\})");
    const auto text = read(v, schema_path);
    const auto rows = std::string_view(text).substr(index_of(text,
        "using f = field;", schema_path));
    const auto enum_start = index_of(text, "enum class field", schema_path);
    const auto enum_end = index_of(text, "};", schema_path, enum_start);
    schema out;
    out.every = all_of(enum_member, std::string_view(text).substr(enum_start,
        enum_end - enum_start));
    out.identity = all_of(identity_ref(), rows);
    for (const auto& c : all_of(coordinate_ref, rows)) {
        if (!out.identity.contains(c))
            out.coordinate.insert(c);
    }
    if (out.every.empty() || out.identity.empty())
        throw unreadable{schema_path, "yields no fields"};
    std::map<std::string, names> arrays;
    for (const auto& m : array.find_all(rows))
        arrays[std::string(m.group(1))] = all_of(identity_ref(), m.group(2));
    for (const auto& m : row.find_all(rows)) {
        auto& [fields, types] = out.classes[std::string(m.group(3))];
        if (const auto it = arrays.find(std::string(m.group(2)));
            it != arrays.end())
            fields.insert(it->second.begin(), it->second.end());
        types.emplace(m.group(1));
    }
    if (out.classes.empty())
        throw unreadable{schema_path, "yields no asset classes"};
    return out;
}

std::map<std::string, std::pair<std::vector<std::string>, names>>
read_views(const ct::corpus_view& v) {
    static const pt::regex view(
        R"(create or replace view (\w+)\s+with \(security_invoker = true\) as\s+select\s+(.*?)\s+from ores_marketdata_market_series_identity_tbl\s+where identity_kind = 'series'\s+and instrument_type in \(\s*(.*?)\s*\);)",
        {.dotall = true});
    static const pt::regex view_name(R"(^ores_marketdata_series_identity_(\w+)_vw$)");
    static const pt::regex quoted(R"('(\w+)')");
    const auto text = read(v, views_path);
    std::map<std::string, std::pair<std::vector<std::string>, names>> out;
    for (const auto& m : view.find_all(text)) {
        const auto n = view_name.match(m.group(1));
        if (!n)
            throw unreadable{views_path, "defines '" +
                std::string(m.group(1)) + "', which does not follow "
                "ores_marketdata_series_identity_<class>_vw"};
        std::vector<std::string> columns;
        for (auto item : ops::split(m.group(2), ',')) {
            item = co::strip(item);
            if (item.empty())
                continue;
            const auto lowered = ops::lowered(item);
            if (const auto as = lowered.find(" as ");
                as != std::string::npos)
                item = item.substr(as + 4);
            auto column = co::strip(item);
            while (column.starts_with('"'))
                column.remove_prefix(1);
            while (column.ends_with('"'))
                column.remove_suffix(1);
            columns.emplace_back(column);
        }
        out[std::string(n->group(1))] = {std::move(columns),
            all_of(quoted, m.group(3))};
    }
    if (out.empty())
        throw unreadable{views_path, "yields no views"};
    return out;
}

names read_table_columns(const ct::corpus_view& v) {
    static const pt::regex column(R"re(\s*"([a-z_0-9]+)"\s)re");
    const auto text = read(v, create_path);
    const auto quoted = "\"" + std::string(table) + "\" (";
    const auto at = text.find(quoted);
    if (at == std::string::npos)
        throw unreadable{create_path, "does not create " +
            std::string(table)};
    const auto body = std::string_view(text).substr(text.find('(', at) + 1);
    std::int64_t depth = 1;
    names out;
    for (const auto line : co::split_lines(body)) {
        depth += std::ranges::count(line, '(') - std::ranges::count(line, ')');
        if (depth <= 0)
            break;
        const auto stripped = co::strip(line);
        const auto m = column.match(line);
        if (m && !stripped.starts_with("primary key") &&
            !stripped.starts_with("check") &&
            !stripped.starts_with("exclude") &&
            !stripped.starts_with("unique"))
            out.emplace(m->group(1));
    }
    if (out.empty())
        throw unreadable{create_path, "yields no columns"};
    return out;
}

std::map<std::string, std::string> read_switch(std::string_view text) {
    static const pt::regex arm(
        R"(case field::(\w+):\n\s*(?:row\.(\w+) = text;)?\s*\n?\s*break;)");
    const auto start = index_of(text, "switch (f) {", projector_path);
    const auto end = index_of(text, "\n    }\n", projector_path, start);
    std::map<std::string, std::string> out;
    for (const auto& m : arm.find_all(text.substr(start, end - start)))
        out[std::string(m.group(1))] = std::string(m.group_if(2).value_or(""));
    return out;
}

std::map<std::string, std::string> read_index_mapping(std::string_view text) {
    static const pt::regex placement(
        R"re(name == "(\w+)"\)\s*\n\s*row\.(\w+) = text;)re");
    const auto start = index_of(text, "void assign_index(", projector_path);
    const auto end = index_of(text, "\n}\n", projector_path, start);
    std::map<std::string, std::string> out;
    for (const auto& m : placement.find_all(text.substr(start, end - start)))
        out[std::string(m.group(1))] = std::string(m.group(2));
    if (out.empty())
        throw unreadable{projector_path, "no longer names the index mapping "
            "in the shape the check reads"};
    return out;
}

names read_index_names(const ct::corpus_view& v) {
    static const pt::regex row(
        R"re(\{index_family::(\w+),\s*asset_class::(\w+),\s*"(\w+)",\s*detail::(\w+)\})re");
    static const pt::regex array(
        R"(inline constexpr std::array<index_field_spec, \d+> (\w+)\{(.*?)\};)",
        {.dotall = true});
    static const pt::regex field_name(R"re(\{"(\w+)",\s*(?:true|false)\})re");
    const auto text = read(v, grammar_path);
    std::map<std::string, names> arrays;
    for (const auto& m : array.find_all(text))
        arrays[std::string(m.group(1))] = all_of(field_name, m.group(2));
    names out;
    for (const auto& m : row.find_all(text)) {
        out.emplace(m.group(3));
        if (const auto it = arrays.find(std::string(m.group(4)));
            it != arrays.end())
            out.insert(it->second.begin(), it->second.end());
    }
    if (out.empty())
        throw unreadable{grammar_path, "yields no index names"};
    return out;
}

template <std::size_t N>
names as_set(const std::array<std::string_view, N>& a) {
    return names(a.begin(), a.end());
}

names minus(const names& a, const names& b) {
    names out;
    std::ranges::set_difference(a, b, std::inserter(out, out.end()));
    return out;
}

void check(const ct::corpus_view& v, ct::diagnostic_table& out) {
    const auto report = [&](rule r, std::string_view path,
        const std::string& message) {
        co::report(out, path, 0, r, message, "Bring the schema, the "
            "projector and the SQL of the series identity back in step.");
    };
    const auto s = read_schema(v);
    const auto index_names = read_index_names(v);
    const auto projector = read(v, projector_path);
    const auto index_placed = read_index_mapping(projector);
    names index_columns;
    for (const auto& [name, column] : index_placed)
        index_columns.insert(column);

    const auto columns = read_table_columns(v);
    auto expected = as_set(context_columns);
    expected.insert(s.identity.begin(), s.identity.end());
    expected.insert(index_columns.begin(), index_columns.end());
    for (const auto& m : minus(expected, columns))
        report(table_columns, create_path, std::string(table) +
            " has no column '" + m + "'.");
    for (const auto& e : minus(columns, expected))
        report(table_columns, create_path, std::string(table) +
            " has column '" + e + "', which is neither a context column, nor "
            "an identity field, nor an index field.");

    const auto placed = read_switch(projector);
    for (const auto& f : s.every) {
        const auto it = placed.find(f);
        if (it == placed.end()) {
            report(projector_switch, projector_path, "the projector's switch "
                "does not name field '" + f + "'.");
            continue;
        }
        const auto& written = it->second;
        if (s.identity.contains(f) && written != f)
            report(projector_switch, projector_path, "the projector places "
                "field '" + f + "' in column '" + (written.empty()
                    ? "nothing" : written) + "'; it is an identity field and "
                "belongs in '" + f + "'.");
        if (s.coordinate.contains(f) && !written.empty())
            report(projector_switch, projector_path, "the projector places "
                "coordinate field '" + f + "' in column '" + written + "'; a "
                "series holds no coordinate.");
    }

    const auto found = read_views(v);
    for (const auto& [asset, c] : s.classes) {
        if (!found.contains(asset))
            report(views, views_path, "no view exposes the " + asset +
                " identity.");
    }
    for (const auto& [asset, c] : found) {
        if (!s.classes.contains(asset))
            report(views, views_path, "a view exposes the " + asset +
                " identity, which the schema gives no series.");
    }
    const auto context = as_set(view_columns);
    for (const auto& [asset, cls] : s.classes) {
        const auto view = found.find(asset);
        if (view == found.end())
            continue;
        const auto& [class_fields, class_types] = cls;
        const auto& [listed, view_types] = view->second;
        const names selected(listed.begin(), listed.end());
        if (listed.size() != selected.size())
            report(views, views_path, "the " + asset + " view selects a "
                "column more than once.");
        for (const auto& m : minus(context, selected))
            report(views, views_path, "the " + asset + " view does not "
                "expose '" + m + "'.");
        for (const auto& e : minus(minus(selected, context), class_fields))
            report(views, views_path, "the " + asset + " view exposes column "
                "'" + e + "', which is not one of its identity fields.");
        for (const auto& m : minus(class_fields, selected))
            report(views, views_path, "the " + asset + " view does not "
                "expose identity field '" + m + "'.");
        for (const auto& e : minus(view_types, class_types))
            report(views, views_path, "the " + asset + " view names "
                "instrument type '" + e + "', which does not belong to " +
                asset + ".");
        for (const auto& m : minus(class_types, view_types))
            report(views, views_path, "the " + asset + " view does not name "
                "instrument type '" + m + "', so its series are missing from "
                "it.");
    }

    names placed_names;
    for (const auto& [name, column] : index_placed)
        placed_names.insert(name);
    for (const auto& m : minus(index_names, placed_names))
        report(index_mapping, projector_path, "the projector places no "
            "column for index name '" + m + "', which an index family "
            "declares.");
    for (const auto& e : minus(placed_names, index_names))
        report(index_mapping, projector_path, "the projector places index "
            "name '" + e + "', which no index family declares.");
    for (const auto& [name, column] : index_placed) {
        if (!columns.contains(column))
            report(index_mapping, projector_path, "the projector places index "
                "name '" + name + "' in column '" + column + "', which " +
                std::string(table) + " does not have.");
    }
}

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    ct::diagnostic_table found;
    try {
        check(v, found);
    } catch (const unreadable& u) {
        co::report(out, u.path, 0, unreadable_source, u.message,
            "The check reads the source through this shape; teach it the "
            "new one.");
        return;
    }
    out = std::move(found);
}

}

const ct::descriptor marketdata_identity_columns{
    .name = "marketdata_identity_columns",
    .version = 1,
    .summary = "Reports each way the series identity projection disagrees "
        "with the codec schema and the index grammar.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}
