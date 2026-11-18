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
#include "compass.crosscheck/codecs/model_rules.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codec/types/fact.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.crosscheck/ops/words.hpp"
#include "compass.model/ops/load.hpp"
#include "compass.model/ops/outline.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace mo = compass::model::ops;
namespace mt = compass::model::types;
namespace pt = compass::platform::text;

enum rule : std::uint32_t { money_not_exact, continuous_exact };

constexpr std::array<std::string_view, 2> money_not_exact_sources{
    "E47BAF35-066F-4EF2-BA3D-522AED44FF33",
    "B9DAC38A-BEB1-4658-87BC-DFF901033E84"};

constexpr std::array<std::string_view, 1> continuous_exact_sources{
    "B9DAC38A-BEB1-4658-87BC-DFF901033E84"};

constexpr std::array<ct::rule, 2> rules{{
    {"money_not_exact", "A money column is numeric in SQL and a decimal in "
        "C++, never a binary float or text.", money_not_exact_sources},
    {"continuous_exact", "A continuous quantity is not typed numeric or "
        "decimal, an exactness it does not have.", continuous_exact_sources}}};

constexpr std::array<ct::option, 2> options{{
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
        "money."}}};

/**
 * @brief The models of the checkout: every org file directly under a
 * modeling/ directory of projects/, at any depth. map_document keeps
 * those whose type ores.codegen loads.
 */
std::vector<std::string_view> models(const ct::corpus_view& v) {
    auto out = v.list_files("projects/", ".org");
    std::erase_if(out, [](auto p) {
        return !co::parent_of(p).ends_with("/modeling");
    });
    return out;
}

/**
 * @brief The offset of the start of the line that holds @p at, a view
 * into @p source.
 */
std::uint32_t line_start(std::string_view source, std::string_view at) {
    const auto offset = static_cast<std::size_t>(at.data() - source.data());
    const auto newline = offset == 0 ? std::string_view::npos
                                     : source.rfind('\n', offset - 1);
    return newline == std::string_view::npos
        ? 0 : static_cast<std::uint32_t>(newline + 1);
}

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

std::string base_type(std::string_view sql_type) {
    return ops::lowered(co::strip(sql_type.substr(0, sql_type.find('('))));
}

bool binary_float(std::string_view cpp_type) {
    static const pt::regex word(R"(\b(?:double|float)\b)");
    return word.search(cpp_type).has_value();
}

std::set<std::string, std::less<>> keys(std::string_view list) {
    std::set<std::string, std::less<>> out;
    for (const auto k : ops::split(list, ','))
        out.emplace(co::strip(k));
    return out;
}

/**
 * @brief money-decimal-columns over the columns of an entity or junction
 * and the fields of a field group: what the loader reads as each one's
 * name, :type: and :cpp_type:.
 */
void check_money(const ct::document& d, const mo::outline& o,
    mt::model_type type, const ct::option_values& options,
    ct::diagnostic_table& out) {
    const bool field_group = type == mt::model_type::field_group;
    if (!field_group && type != mt::model_type::entity &&
        type != mt::model_type::junction)
        return;
    const auto section = mo::section(o, mo::node_id{0},
        field_group ? "Fields" : "Columns");
    if (section.is_none())
        return;
    const auto excluded = keys(options["excluded_columns"]);
    const auto continuous = keys(options["continuous_columns"]);
    for (const auto n : mo::children(o, section)) {
        const auto name = o.titles[n.value];
        const auto sql_type = mo::property(o, n, "type");
        const auto cpp = std::string(mo::property(o, n, "cpp_type").value);
        const auto key = std::string(d.path) + "::" + std::string(name);
        const auto k = continuous.contains(key) ? kind::continuous
                                                : classify(name);
        const auto sql = sql_type.found ? std::string(sql_type.value)
                                        : std::string("(embedded)");
        const auto typed = "sql=" + sql + " cpp=" + (cpp.empty() ? "-" : cpp);
        if (k == kind::money && !excluded.contains(key)) {
            bool offends = false;
            if (!sql_type.found)
                offends = field_group &&
                    (binary_float(cpp) || cpp.contains("string"));
            else {
                const auto base = base_type(sql_type.value);
                offends = in(float_types, base) || base == "text" ||
                    (base == "numeric" && binary_float(cpp));
            }
            if (offends)
                co::report(out, d.path, line_start(d.source, name),
                    money_not_exact, std::string(name) + " is money, typed " +
                    typed, "Type a money column numeric in SQL and "
                    "ores::utility::decimal::decimal in C++.");
        } else if (k == kind::continuous) {
            const bool offends = sql_type.found
                ? base_type(sql_type.value) == "numeric"
                : field_group && cpp.contains("decimal");
            if (offends)
                co::report(out, d.path, line_start(d.source, name),
                    continuous_exact, std::string(name) + " is a continuous "
                    "quantity, typed " + typed, "Type a continuous quantity "
                    "double precision and double.");
        }
    }
}

void map(const ct::document& d, const ct::option_values& options,
    ct::file_scan& out) {
    const mo::document doc{d.path, d.source, &d.tables};
    const auto type = mo::model_type_of(mo::declared_type(doc));
    if (!type)
        return;
    mo::outline o;
    mo::build_outline(d.source, d.tables, o);
    check_money(d, o, *type, options, out.findings);
}

}

const ct::descriptor model_rules{
    .name = "model_rules",
    .version = 1,
    .summary = "Reports each codegen model that breaks a rule ORE Studio's "
        "gates state about one model.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::scan_function{.files = models, .map_document = map}};

}
