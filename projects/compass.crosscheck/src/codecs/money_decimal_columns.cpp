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
#include "compass.crosscheck/codecs/money_decimal_columns.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/glob.hpp"
#include "compass.cpp/ops/paths.hpp"
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

constexpr std::uint32_t money_not_exact = 0;
constexpr std::uint32_t continuous_exact = 1;
constexpr std::uint32_t too_few_columns = 2;

constexpr std::array<std::string_view, 2> money_not_exact_sources{
    "E47BAF35-066F-4EF2-BA3D-522AED44FF33",
    "B9DAC38A-BEB1-4658-87BC-DFF901033E84"};

constexpr std::array<std::string_view, 1> continuous_exact_sources{
    "B9DAC38A-BEB1-4658-87BC-DFF901033E84"};

constexpr std::array<ct::rule, 3> rules{{
    {"money_not_exact", "A money column is numeric in SQL and a decimal in "
        "C++, never a binary float or text.", money_not_exact_sources},
    {"continuous_exact", "A continuous quantity is not typed numeric or "
        "decimal, an exactness it does not have.", continuous_exact_sources},
    {"too_few_columns", "The models yield at least min_columns columns, so "
        "the check is reading them."}}};

constexpr std::array<ct::option, 3> options{{
    {"excluded_columns",
        "projects/ores.refdata/modeling/"
        "ores.refdata.bond_future_volatility_config.org::strike_factor,"
        "projects/ores.refdata/modeling/"
        "ores.refdata.cds_volatility_config.org::strike_factor,"
        "projects/ores.synthetic/modeling/"
        "ores.synthetic.fx_spot_generation_config.org::gmm_initial_price,"
        "projects/ores.synthetic/modeling/"
        "ores.synthetic.ir_curve_generation_config.org::initial_rate",
        "Model::column keys whose name says money and whose economics do "
        "not; they are not checked as money."},
    {"continuous_columns",
        "projects/ores.trading/modeling/"
        "ores.trading.fx_variance_swap_instrument.org::strike",
        "Model::column keys of continuous quantities whose name reads as "
        "money."},
    {"min_columns", "1000", "The fewest columns a run may read."}}};

constexpr std::array<std::string_view, 12> money_tokens{"rate", "spread",
    "notional", "amount", "price", "strike", "fee", "premium", "margin",
    "barrier", "face", "yield"};
constexpr std::array<std::string_view, 5> head_qualifiers{"value", "level",
    "pay", "receive", "held"};
constexpr std::array<std::string_view, 17> continuous_tokens{"variance",
    "recovery", "participation", "ratio", "quantity", "weight", "probability",
    "correlation", "vol", "volatility", "cpi", "fraction", "count", "days",
    "day", "tenor", "period"};
constexpr std::array<std::string_view, 4> float_types{"double precision",
    "real", "float", "double"};

template <std::size_t N>
bool in(const std::array<std::string_view, N>& set, std::string_view s) {
    return std::ranges::find(set, s) != set.end();
}

enum class kind { money, continuous, other };

kind classify(std::string_view name) {
    const auto lowered = ops::lowered(name);
    auto tokens = ops::split(lowered, '_');
    if (std::ranges::any_of(tokens, [](auto t) {
            return in(continuous_tokens, t); }))
        return kind::continuous;
    while (!tokens.empty() && in(head_qualifiers, tokens.back()))
        tokens.pop_back();
    if (!tokens.empty() && in(money_tokens, tokens.back()))
        return kind::money;
    return kind::other;
}

/**
 * @brief A column the script reads: its model, the line of its heading or
 * key, its name and the SQL and C++ types it states, if any.
 */
struct column {
    std::string_view path;
    std::uint32_t line;
    std::string name;
    std::optional<std::string> sql_type;
    std::optional<std::string> cpp_type;
};

std::vector<column> columns(const ct::corpus_view& v) {
    static const pt::regex heading(R"(^\*\* (\S+)\s*$)");
    static const pt::regex section(R"(^\* [^*\s].*$)");
    static const pt::regex type(R"(^\s*:type:\s*(\S.*?)\s*$)");
    static const pt::regex cpp_type(R"(^\s*:cpp_type:\s*(\S.*?)\s*$)");
    static const pt::regex column_key(R"(^\s*:column:\s*(\S+)\s*$)");
    std::vector<column> out;
    for (const auto path : co::glob(v, "projects/**/modeling/*.org")) {
        const auto text = pt::universal_newlines(pt::decode_replacing(
            v.read_file(path).value_or("")));
        std::optional<column> current;
        const auto flush = [&] {
            if (current)
                out.push_back(std::move(*current));
            current.reset();
        };
        std::uint32_t number = 0;
        for (const auto raw : co::split_lines(text)) {
            ++number;
            if (const auto m = heading.match(raw)) {
                flush();
                current = column{path, number, std::string(m->group(1)), {},
                    {}};
                continue;
            }
            if (section.match(raw)) {
                flush();
                continue;
            }
            if (const auto k = column_key.match(raw); k && !current) {
                current = column{path, number, std::string(k->group(1)), {},
                    {}};
                continue;
            }
            if (!current)
                continue;
            if (const auto t = type.match(raw); t && !current->sql_type)
                current->sql_type = std::string(t->group(1));
            if (const auto c = cpp_type.match(raw); c && !current->cpp_type)
                current->cpp_type = std::string(c->group(1));
        }
        flush();
    }
    return out;
}

std::string base_type(std::string_view sql_type) {
    return ops::lowered(co::strip(sql_type.substr(0, sql_type.find('('))));
}

bool binary_float(std::string_view cpp_type) {
    static const pt::regex word(R"(\b(?:double|float)\b)");
    return word.search(cpp_type).has_value();
}

bool is_field_group(std::string_view path) {
    return co::stem_of(path).ends_with("_field_group");
}

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    const auto all = columns(v);
    const auto floor = std::stoul(std::string(v.option("min_columns")));
    if (all.size() < floor) {
        co::report(out, "projects", 0, too_few_columns, "read only " +
            std::to_string(all.size()) + " columns, below the floor; the "
            "model format may have changed", "Teach the check the model's "
            "format, or lower min_columns for a subtree.");
        return;
    }
    const auto keys = [](std::string_view list) {
        std::set<std::string> s;
        for (const auto k : ops::split(list, ','))
            s.emplace(co::strip(k));
        return s;
    };
    const auto excluded = keys(v.option("excluded_columns"));
    const auto continuous = keys(v.option("continuous_columns"));
    const auto bytes_of = [&](std::string_view path) {
        return v.read_file(path).value_or("");
    };
    for (const auto& c : all) {
        const auto key = std::string(c.path) + "::" + c.name;
        const auto k = continuous.contains(key) ? kind::continuous
                                                : classify(c.name);
        const auto cpp = c.cpp_type.value_or("");
        const auto sql = c.sql_type.value_or("(embedded)");
        bool offends = false;
        if (k == kind::money && !excluded.contains(key)) {
            if (!c.sql_type)
                offends = is_field_group(c.path) &&
                    (binary_float(cpp) || cpp.contains("string"));
            else {
                const auto base = base_type(*c.sql_type);
                offends = in(float_types, base) || base == "text" ||
                    (base == "numeric" && binary_float(cpp));
            }
            if (offends)
                co::report(out, c.path, co::offset_of_line(bytes_of(c.path),
                    c.line), money_not_exact, c.name + " is money, typed "
                    "sql=" + sql + " cpp=" + (cpp.empty() ? "-" : cpp),
                    "Type a money column numeric in SQL and "
                    "ores::utility::decimal::decimal in C++.");
        } else if (k == kind::continuous) {
            if (!c.sql_type)
                offends = is_field_group(c.path) && cpp.contains("decimal");
            else
                offends = base_type(*c.sql_type) == "numeric";
            if (offends)
                co::report(out, c.path, co::offset_of_line(bytes_of(c.path),
                    c.line), continuous_exact, c.name + " is a continuous "
                    "quantity, typed sql=" + sql + " cpp=" +
                    (cpp.empty() ? "-" : cpp), "Type a continuous quantity "
                    "double precision and double.");
        }
    }
}

}

const ct::descriptor money_decimal_columns{
    .name = "money_decimal_columns",
    .version = 1,
    .summary = "Reports each money column a model types inexactly, and each "
        "continuous one it types exact.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}
