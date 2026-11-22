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
#include <string>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.crosscheck/ops/generated_shape.hpp"
#include "compass.crosscheck/ops/words.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

enum rule : std::uint32_t { wire_mismatch, unclassified };

constexpr std::array<ct::rule, 2> rules{{
    {"wire_mismatch", "A generated row binds each column in the binary "
        "format its column's type defines, to a member its struct "
        "declares."},
    {"unclassified", "Every bound member and column has a type the check "
        "can place on the wire, or both are text."}}};

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
        {"ores::platform::time::time_of_day", {"time", 0}},
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
        {"time", {"time", 0}}, {"time without time zone", {"time", 0}},
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

void report(ct::diagnostic_table& out, const ct::document& d,
    const ops::bound_column& c, rule r, std::string_view message,
    std::string_view fix) {
    const auto at = c.heading.empty() ? 0
        : static_cast<std::uint32_t>(c.heading.data() - d.source.data());
    out.add({d.row, {at, at}, ct::rule_id{r}, ct::severity::error, message,
        fix});
}

/**
 * @brief Each column the repository row of the model @p d binds in binary,
 * against the type the model's create script gives it. A row on a table
 * the model does not create, such as a view, is not compared.
 */
void run(const ct::document& d, const ct::corpus_view& v,
    ct::diagnostic_table& out) {
    const auto shape = ops::generated_shape_of(v, d);
    if (!shape || !shape->has_row || !shape->creates_table)
        return;
    for (const auto& c : shape->bound) {
        if (c.member_type.empty()) {
            report(out, d, c, wire_mismatch, "the row binds '" + c.column +
                "' but the member is not declared in the struct",
                "Give the key column a type the row declares a member for.");
            continue;
        }
        const auto cpp_wire = wire_of_cpp(c.member_type);
        const auto sql_wire = wire_of_sql(c.sql_type);
        if (!cpp_wire) {
            report(out, d, c, unclassified, "'" + c.column + "' holds " +
                c.member_type + ", which this check cannot place on the "
                "wire", "Teach the check the type's format.");
            continue;
        }
        if (!sql_wire) {
            if (*cpp_wire == wire{"text", 0})
                continue;
            report(out, d, c, unclassified, "'" + c.column + "' holds " +
                c.member_type + " but " + shape->table + " declares it " +
                c.sql_type + ", a type this check cannot place on the wire",
                "Teach the check the type's format.");
            continue;
        }
        if (*cpp_wire != *sql_wire)
            report(out, d, c, wire_mismatch, "'" + c.column + "' holds " +
                c.member_type + " (" + describe(*cpp_wire) + ") but " +
                shape->table + " creates it " + c.sql_type + " (" +
                describe(*sql_wire) + ")", "Make the model's C++ and SQL "
                "types agree, then regenerate.");
    }
}

}

const ct::descriptor entity_binary_types{
    .name = "entity_binary_types",
    .version = 3,
    .summary = "Reports each column a model's generated row binds in a "
        "binary format its create script's type does not define.",
    .reads = ct::inputs::every_document(),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::document_function<ct::diagnostic_table>{run}};

}
