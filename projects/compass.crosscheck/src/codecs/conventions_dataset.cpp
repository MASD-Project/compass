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
#include "compass.crosscheck/codecs/conventions_dataset.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.crosscheck/ops/csv.hpp"
#include "compass.crosscheck/ops/python_literals.hpp"
#include "compass.crosscheck/ops/python_numbers.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;
using kind = ops::python_value::kind;

/**
 * @brief Where the script stops without writing: a GenError, or a
 * ValueError its decoders raise.
 */
struct stop {};

/**
 * @brief The text render() writes around the blocks, with the names it
 * joins in as @NAME@.
 */
constexpr std::string_view render_template = R"tpl(@HEADER@
-- =============================================================================
-- Methodology and Dataset Registration
-- =============================================================================

DO $$
BEGIN
    PERFORM ores_dq_methodologies_upsert_fn(ores_utility_system_tenant_id_fn(),
        '@METHODOLOGY@',
        '@DESCRIPTION@',
        '@URL@',
        @TEXT@
    );

    PERFORM ores_dq_datasets_upsert_fn(ores_utility_system_tenant_id_fn(),
        '@CODE@',
        'ORE Analytics',
        'Trading',
        'Reference Data',
        'NONE',
        'Primary',
        'Actual',
        'Raw',
        '@METHODOLOGY@',
        'Conventions',
        'The curated index and instrument convention set every tenant reads: the conventions of the ORE examples, the index conventions ORE defines in code, and the conventions OreStudio adds, one row per (kind, id), each with its source in conventions.tsv.',
        'ORESTUDIO',
        'The conventions trade builders, curve bootstraps and ORE runs resolve an index or an instrument by',
        current_date,
        'Internal Use Only',
        'conventions'
    );
END $$;

-- =============================================================================
-- Artefact Seed Data
-- =============================================================================

do $$
declare
    v_dataset_id uuid;
    v_tenant_id uuid := ores_utility_system_tenant_id_fn();
    -- The live convention tables take their party from the publish, not from
    -- the artefact. The flat artefact tables carry a placeholder party_id for
    -- their not-null constraint. The atomised convention stages no party at
    -- all: its central row and its parts are written for the party the
    -- publish names.
    v_party_id uuid := ores_utility_nil_uuid_fn();
begin
    select id into v_dataset_id
    from ores_dq_datasets_tbl
    where tenant_id = v_tenant_id
      and code = '@CODE@'
      and valid_to = ores_utility_infinity_timestamp_fn();

    if v_dataset_id is null then
        raise exception 'Dataset not found: @CODE@';
    end if;
)tpl";

constexpr std::array<ct::option, 2> options{{
    {"script", "projects/ores.seeder/datasets/conventions/generate_sql.py",
        "The generator; conventions.tsv, oresmd_index_map.tsv and "
        "methodology.txt are read beside it."},
    {"output",
        "projects/ores.sql/populate/refdata/"
        "refdata_conventions_artefact_populate.sql",
        "The populate script the generator writes."}}};

using strings = std::vector<std::string>;

/**
 * @brief One (xml field, column, parser type) of a mapping; the field is
 * empty for a decoded column, which the script writes None for.
 */
struct mapping {
    std::string field;
    std::string column;
    std::string type;
};

struct part {
    std::string name;
    std::string table;
    std::vector<mapping> columns;
};

/**
 * @brief A COMPOSITE_KINDS entry. Its parts_of is either a lambda whose
 * body lists the parts, held in fixed_parts, or a function of the script,
 * named in function.
 */
struct composite {
    std::string kind_code;
    std::string central_table;
    std::optional<strings> fixed_parts;
    std::string function;
    std::vector<part> parts;
};

/**
 * @brief The script's tables, as its source states them.
 */
struct tables {
    std::map<std::string, std::map<std::string, std::string, std::less<>>,
        std::less<>> normalisers;
    std::map<std::string, std::pair<std::string, std::vector<mapping>>,
        std::less<>> kinds;
    std::set<std::string, std::less<>> cut_kinds;
    std::map<std::string, composite, std::less<>> composites;
    std::set<std::string, std::less<>> world_kinds;
    strings index_fields;
    strings index_kinds;
    std::map<std::string, strings, std::less<>> primary_index;
    std::map<std::string, strings, std::less<>> nested_columns;
    std::string header;
    std::string dataset_code;
    std::string methodology;
    std::string methodology_description;
    std::string methodology_url;
};

ops::python_value module_value(std::string_view source,
    std::string_view name) {
    auto v = ops::python_assignment(source, name);
    if (!v)
        throw stop{};
    return std::move(*v);
}

std::string text_of(const ops::python_value& v) {
    if (v.what != kind::string)
        throw stop{};
    return v.text;
}

strings texts_of(const ops::python_value& v) {
    const auto s = ops::strings_of(v);
    if (!s)
        throw stop{};
    return *s;
}

std::vector<mapping> mappings_of(const ops::python_value& v) {
    if (v.what != kind::sequence)
        throw stop{};
    std::vector<mapping> out;
    for (const auto& t : v.items) {
        if (t.what != kind::sequence || t.items.size() != 3)
            throw stop{};
        const auto& f = t.items[0];
        if (f.what == kind::name && f.text != "None")
            throw stop{};
        out.push_back({f.what == kind::name ? std::string() : text_of(f),
            text_of(t.items[1]), text_of(t.items[2])});
    }
    return out;
}

const ops::python_value& entry_of(const ops::python_value& dict,
    std::string_view key) {
    for (const auto& e : dict.entries) {
        if (e.key == key)
            return e.value;
    }
    throw stop{};
}

tables tables_of(std::string_view source) {
    tables t;
    for (const auto& [type, name] : {std::pair{"daycounter", "DAY_COUNTER"},
             std::pair{"bdc", "BDC"}, std::pair{"frequency", "FREQUENCY"},
             std::pair{"compounding", "COMPOUNDING"},
             std::pair{"daterule", "DATE_RULE"}}) {
        auto& table = t.normalisers[type];
        const auto normaliser = module_value(source, name);
        for (const auto& e : normaliser.entries)
            table.insert_or_assign(e.key, text_of(e.value));
    }
    const auto kinds = module_value(source, "KINDS");
    for (const auto& e : kinds.entries) {
        const auto& spec = e.value;
        if (spec.what != kind::sequence || spec.items.size() != 2)
            throw stop{};
        t.kinds.insert_or_assign(e.key, std::pair{text_of(spec.items[0]),
            mappings_of(spec.items[1])});
    }
    const auto cut = module_value(source, "CUT_KINDS");
    for (const auto& e : cut.entries)
        t.cut_kinds.insert(e.key);
    const auto composites = module_value(source, "COMPOSITE_KINDS");
    for (const auto& e : composites.entries) {
        composite c;
        c.kind_code = text_of(entry_of(e.value, "kind_code"));
        c.central_table = text_of(entry_of(e.value, "central_table"));
        const auto& parts_of = entry_of(e.value, "parts_of");
        if (parts_of.what != kind::name)
            throw stop{};
        if (parts_of.text == "lambda") {
            if (parts_of.items.size() != 1)
                throw stop{};
            c.fixed_parts = texts_of(parts_of.items.front());
        } else {
            c.function = parts_of.text;
        }
        for (const auto& p : entry_of(e.value, "parts").entries)
            c.parts.push_back({p.key, text_of(entry_of(p.value, "table")),
                mappings_of(entry_of(p.value, "mapping"))});
        t.composites.insert_or_assign(e.key, std::move(c));
    }
    for (const auto& w : texts_of(module_value(source, "WORLD_KINDS")))
        t.world_kinds.insert(w);
    t.index_fields = texts_of(module_value(source, "INDEX_FIELDS"));
    t.index_kinds = texts_of(module_value(source, "INDEX_KINDS"));
    const auto primary = module_value(source, "PRIMARY_INDEX");
    for (const auto& e : primary.entries)
        t.primary_index.insert_or_assign(e.key, texts_of(e.value));
    const auto nested = module_value(source, "NESTED_COLUMNS");
    for (const auto& e : nested.entries)
        t.nested_columns.insert_or_assign(e.key, texts_of(e.value));
    t.header = text_of(module_value(source, "HEADER"));
    t.dataset_code = text_of(module_value(source, "DATASET_CODE"));
    t.methodology = text_of(module_value(source, "METHODOLOGY"));
    t.methodology_description = text_of(module_value(source,
        "METHODOLOGY_DESCRIPTION"));
    t.methodology_url = text_of(module_value(source, "METHODOLOGY_URL"));
    return t;
}

/**
 * @brief A signature's fields: split at " ; ", each at its first '='; a
 * later field of one name replaces an earlier one, in the earlier place.
 */
class fields {
public:
    explicit fields(std::string_view signature) {
        std::size_t start = 0;
        while (start <= signature.size()) {
            auto end = signature.find(" ; ", start);
            if (end == std::string_view::npos)
                end = signature.size();
            const auto token = signature.substr(start, end - start);
            if (const auto eq = token.find('='); eq != std::string_view::npos)
                set(std::string(token.substr(0, eq)),
                    std::string(token.substr(eq + 1)));
            start = end + 3;
        }
    }

    [[nodiscard]] const std::string* get(std::string_view name) const {
        const auto it = std::ranges::find(items_, name,
            &std::pair<std::string, std::string>::first);
        return it == items_.end() ? nullptr : &it->second;
    }

    [[nodiscard]] bool stated(std::string_view name) const {
        const auto* v = get(name);
        return v != nullptr && !co::strip(*v).empty();
    }

private:
    void set(std::string name, std::string value) {
        for (auto& [n, v] : items_) {
            if (n == name) {
                v = std::move(value);
                return;
            }
        }
        items_.emplace_back(std::move(name), std::move(value));
    }

    std::vector<std::pair<std::string, std::string>> items_;
};

/**
 * @brief Python's dict for a few keys: insertion order, a later write
 * keeping the earlier place.
 */
class ordered {
public:
    void set(std::string_view key, std::string value) {
        for (auto& [k, v] : items_) {
            if (k == key) {
                v = std::move(value);
                return;
            }
        }
        items_.emplace_back(std::string(key), std::move(value));
    }

    [[nodiscard]] const std::string* get(std::string_view key) const {
        const auto it = std::ranges::find(items_, key,
            &std::pair<std::string, std::string>::first);
        return it == items_.end() ? nullptr : &it->second;
    }

    [[nodiscard]] bool contains(std::string_view key) const {
        return get(key) != nullptr;
    }

    [[nodiscard]] const auto& items() const noexcept { return items_; }

private:
    std::vector<std::pair<std::string, std::string>> items_;
};

std::vector<std::pair<std::string, std::string>> split_nested(
    std::string_view value) {
    std::vector<std::pair<std::string, std::string>> out;
    std::size_t start = 0;
    while (start <= value.size()) {
        auto end = value.find('/', start);
        if (end == std::string_view::npos)
            end = value.size();
        const auto token = value.substr(start, end - start);
        if (const auto eq = token.find('='); eq != std::string_view::npos)
            out.emplace_back(token.substr(0, eq), token.substr(eq + 1));
        start = end + 1;
    }
    return out;
}

std::string int_of(std::string_view v) {
    const auto i = ops::python_int(v);
    if (!i)
        throw stop{};
    return *i;
}

std::string float_of(std::string_view v) {
    const auto f = ops::python_float(v);
    if (!f)
        throw stop{};
    return *f;
}

std::string bool_of(std::string_view v) {
    std::string s(co::strip(v));
    for (auto& c : s) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return s == "true" || s == "y" || s == "yes" || s == "1" ? "true"
                                                              : "false";
}

std::string joined(const strings& items, std::string_view by) {
    std::string out;
    for (std::size_t i = 0; i < items.size(); ++i)
        out += (i == 0 ? "" : std::string(by)) + items[i];
    return out;
}

ordered decode_commodity_future(const fields& f) {
    ordered out;
    if (const auto* val = f.get("AnchorDay"); val && !val->empty()) {
        for (const auto& [k, v] : split_nested(*val)) {
            if (k == "NthWeekday") {
                for (const auto& [k2, v2] : split_nested(v)) {
                    if (k2 == "Nth")
                        out.set("anchor_nth_nth", int_of(v2));
                    else if (k2 == "Weekday")
                        out.set("anchor_nth_weekday", v2);
                }
            } else if (k == "DayOfMonth") {
                out.set("anchor_day_of_month", int_of(v));
            } else if (k == "CalendarDaysBefore") {
                out.set("anchor_calendar_days_before", int_of(v));
            } else if (k == "BusinessDaysAfter") {
                out.set("anchor_business_days_after", int_of(v));
            } else if (k == "LastWeekday") {
                out.set("anchor_last_weekday", v);
            } else if (k == "WeeklyDayOfTheWeek") {
                out.set("anchor_weekly_day_of_the_week", v);
            }
        }
    }
    if (const auto* val = f.get("ValidContractMonths"); val && !val->empty()) {
        strings months;
        for (const auto& [k, v] : split_nested(*val)) {
            if (k == "Month")
                months.push_back(v);
        }
        if (!months.empty())
            out.set("valid_contract_months", joined(months, ","));
    }
    if (const auto* val = f.get("OffPeakPowerIndexData");
        val && !val->empty()) {
        for (const auto& [k, v] : split_nested(*val)) {
            if (k == "OffPeakHours")
                out.set("off_peak_hours", float_of(v));
            else if (k == "OffPeakIndex")
                out.set("off_peak_index", v);
            else if (k == "PeakCalendar")
                out.set("peak_calendar", v);
            else if (k == "PeakIndex")
                out.set("peak_index", v);
        }
    }
    if (const auto* val = f.get("AveragingData"); val && !val->empty()) {
        for (const auto& [k, v] : split_nested(*val)) {
            if (k == "CommodityName")
                out.set("averaging_commodity_name", v);
            else if (k == "Conventions")
                out.set("averaging_conventions", v);
            else if (k == "Period")
                out.set("averaging_period", v);
            else if (k == "PricingCalendar")
                out.set("averaging_pricing_calendar", v);
            else if (k == "UseBusinessDays")
                out.set("averaging_use_business_days", bool_of(v));
            else if (k == "DeliveryRollDays")
                out.set("averaging_delivery_roll_days", int_of(v));
            else if (k == "FutureMonthOffset")
                out.set("averaging_future_month_offset", int_of(v));
            else if (k == "DailyExpiryOffset")
                out.set("averaging_daily_expiry_offset", int_of(v));
        }
    }
    if (const auto* val = f.get("ProhibitedExpiries"); val && !val->empty()) {
        std::string_view rest = *val;
        if (rest.starts_with("Dates="))
            rest.remove_prefix(6);
        strings dates;
        for (const auto& [k, v] : split_nested(rest)) {
            if (k == "Date")
                dates.push_back(v);
        }
        if (!dates.empty())
            out.set("prohibited_expiries", joined(dates, ","));
    }
    for (const auto& [field, column] : {
             std::pair{"FutureContinuationMappings",
                 "future_continuation_mappings"},
             std::pair{"OptionContinuationMappings",
                 "option_continuation_mappings"}}) {
        const auto* val = f.get(field);
        if (val == nullptr || val->empty())
            continue;
        strings pairs;
        std::optional<std::string> from;
        for (const auto& [k, v] : split_nested(*val)) {
            if (k == "ContinuationMapping") {
                from.reset();
            } else if (k == "From") {
                from = v;
            } else if (k == "To" && from) {
                pairs.push_back(int_of(*from) + ":" + int_of(v));
            }
        }
        if (!pairs.empty())
            out.set(column, joined(pairs, ","));
    }
    return out;
}

ordered decode_intraday_power_load(const fields& f) {
    static const pt::regex datum(R"re(LoadProfileDatum=Date=(\d{4}-\d{2}-\d{2}))re"
        R"re(((?:/LoadFactors=(?:LoadFactor=\d+/?)+)+))re");
    static const pt::regex factor(R"re(LoadFactor=(\d+))re");
    static const pt::regex rule(R"re(LoadProfileBusinessDayRule=)re"
        R"re(BusinessDayLoadFactors=(?:LoadFactor=\d+/?)+)re"
        R"re(/Calendar=([^/]+)/Date=(\d{4}-\d{2}-\d{2}))re");
    ordered out;
    const auto* val = f.get("PowerLoadProfileData");
    if (val == nullptr || val->empty())
        return out;
    strings explicit_profile;
    for (const auto& m : datum.find_all(*val)) {
        strings factors;
        for (const auto& g : factor.find_all(m.group(2)))
            factors.emplace_back(g.group(1));
        explicit_profile.push_back(std::string(m.group(1)) + "|" +
            joined(factors, ","));
    }
    if (!explicit_profile.empty())
        out.set("explicit_load_profile", joined(explicit_profile, ";"));
    strings rules;
    for (const auto& m : rule.find_all(*val))
        rules.push_back(std::string(m.group(2)) + "|" +
            std::string(m.group(1)) + "||");
    if (!rules.empty())
        out.set("business_day_load_rules", joined(rules, ";"));
    return out;
}

std::optional<ordered> decoded_of(std::string_view kind, const fields& f) {
    if (kind == "CommodityFuture")
        return decode_commodity_future(f);
    if (kind == "IntradayPowerLoad")
        return decode_intraday_power_load(f);
    return std::nullopt;
}

std::string sql_literal(std::string_view value) {
    std::string out = "'";
    for (const auto c : value) {
        if (c == '\'')
            out += "''";
        else
            out.push_back(c);
    }
    return out + "'";
}

/**
 * @brief A cell of a column whose parser type is @p type: the raw text for
 * text, a normalised or parsed value otherwise, as a SQL literal.
 */
std::string parsed(const tables& t, std::string_view type,
    const std::string& raw) {
    if (type == "text")
        return sql_literal(raw);
    if (type == "int")
        return sql_literal(int_of(raw));
    if (type == "float")
        return sql_literal(float_of(raw));
    if (type == "bool")
        return sql_literal(bool_of(raw));
    const auto table = t.normalisers.find(type);
    if (table == t.normalisers.end())
        throw stop{};
    const auto it = table->second.find(raw);
    if (it == table->second.end())
        throw stop{};
    return sql_literal(it->second);
}

using row = std::map<std::string, std::optional<std::string>, std::less<>>;

const std::string& column_of(const row& r, std::string_view name) {
    const auto it = r.find(name);
    if (it == r.end() || !it->second)
        throw stop{};
    return *it->second;
}

std::string optional_column(const row& r, std::string_view name) {
    const auto it = r.find(name);
    if (it == r.end())
        return {};
    if (!it->second)
        throw stop{};
    return *it->second;
}

bool holds(const strings& s, std::string_view v) {
    return std::ranges::find(s, v) != s.end();
}

std::set<std::string> referenced(const tables& t, std::string_view kind,
    const std::string& id, const fields& f) {
    std::set<std::string> names;
    for (const auto& field : t.index_fields) {
        if (f.stated(field))
            names.insert(*f.get(field));
    }
    if (holds(t.index_kinds, kind))
        names.insert(id);
    return names;
}

struct block {
    std::string table;
    strings columns;
    std::vector<strings> rows;
    std::string guard;
    std::string label;
};

strings parts_of(const composite& c, const fields& f) {
    if (c.fixed_parts)
        return *c.fixed_parts;
    if (c.function == "tenor_basis_swap_parts")
        return {f.stated("PayIndex") || f.stated("ReceiveIndex")
            ? "pay_receive" : "long_short"};
    if (c.function == "commodity_future_parts") {
        static constexpr std::array<std::string_view, 10> option_fields{
            "OptionExpiryMonthLag", "OptionContractFrequency",
            "OptionExpiryOffset", "OptionCalendarDaysBefore",
            "OptionMinBusinessDaysBefore", "OptionExpiryDay",
            "OptionExpiryLastWeekdayOfMonth",
            "OptionExpiryWeeklyDayOfTheWeek", "OptionBusinessDayConvention",
            "OptionUnderlyingFutureConvention"};
        const auto decoded = decode_commodity_future(f);
        strings out{"contract_terms"};
        if (std::ranges::any_of(decoded.items(), [](const auto& kv) {
                return kv.first.starts_with("anchor_");
            }))
            out.emplace_back("expiry_anchor");
        if (std::ranges::any_of(option_fields, [&](auto o) {
                return f.stated(o);
            }) || decoded.contains("option_continuation_mappings"))
            out.emplace_back("option_expiry");
        if (decoded.contains("off_peak_index"))
            out.emplace_back("off_peak_index");
        if (decoded.contains("averaging_commodity_name"))
            out.emplace_back("averaging_terms");
        return out;
    }
    throw stop{};
}

std::vector<block> composite_blocks(const tables& t,
    const std::vector<row>& rows, const std::string& kind) {
    const auto& spec = t.composites.at(kind);
    const auto kind_code = sql_literal(spec.kind_code);
    std::vector<block> blocks;
    blocks.push_back({spec.central_table, {"dataset_id", "tenant_id",
        "kind_code", "name", "oresmd_uri"}, {}, " and kind_code = " +
        kind_code, kind + " central"});
    for (const auto& p : spec.parts) {
        auto spaced = p.name;
        std::ranges::replace(spaced, '_', ' ');
        block b{p.table, {"dataset_id", "tenant_id", "convention_name",
            "kind_code"}, {}, "", kind + " " + spaced};
        for (const auto& m : p.columns)
            b.columns.push_back(m.column);
        blocks.push_back(std::move(b));
    }
    for (const auto& r : rows) {
        if (column_of(r, "kind") != kind)
            continue;
        const fields f(column_of(r, "signature"));
        const auto name = sql_literal(column_of(r, "id"));
        blocks.front().rows.push_back({"v_dataset_id", "v_tenant_id",
            kind_code, name, "null"});
        const auto decoded = decoded_of(kind, f).value_or(ordered{});
        for (const auto& part_name : parts_of(spec, f)) {
            const auto p = std::ranges::find(spec.parts, part_name,
                &part::name);
            if (p == spec.parts.end())
                throw stop{};
            strings cells{"v_dataset_id", "v_tenant_id", name, kind_code};
            for (const auto& m : p->columns) {
                if (m.type == "decoded") {
                    const auto* v = decoded.get(m.column);
                    cells.push_back(v ? sql_literal(*v) : "null");
                    continue;
                }
                const auto* raw = f.get(m.field);
                cells.push_back(raw ? parsed(t, m.type, *raw) : "null");
            }
            blocks[static_cast<std::size_t>(p - spec.parts.begin()) + 1]
                .rows.push_back(std::move(cells));
        }
    }
    return blocks;
}

block flat_block(const tables& t, const std::vector<row>& rows,
    const std::string& kind,
    const std::map<std::string, std::string, std::less<>>& index_map) {
    const auto& [stem, columns_of_kind] = t.kinds.at(kind);
    const bool party = !t.world_kinds.contains(stem);
    strings columns{"dataset_id", "tenant_id", "id", "version"};
    if (party)
        columns.emplace_back("party_id");
    const auto base = columns.size();
    for (const auto& m : columns_of_kind)
        columns.push_back(m.column);
    if (const auto n = t.nested_columns.find(kind);
        n != t.nested_columns.end()) {
        for (const auto& c : n->second) {
            if (!holds(columns, c))
                columns.push_back(c);
        }
    }
    if (!holds(columns, "oresmd_uri"))
        columns.emplace_back("oresmd_uri");

    block out{"ores_dq_" + stem + "_conventions_artefact_tbl", columns, {},
        "", kind};
    for (const auto& r : rows) {
        if (column_of(r, "kind") != kind)
            continue;
        const auto& id = column_of(r, "id");
        const fields f(column_of(r, "signature"));
        ordered values;
        values.set("id", sql_literal(id));
        for (const auto& m : columns_of_kind) {
            const auto* raw = f.get(m.field);
            if (raw == nullptr)
                continue;
            values.set(m.column, parsed(t, m.type, *raw));
        }
        if (const auto decoded = decoded_of(kind, f)) {
            for (const auto& [column, raw] : decoded->items())
                values.set(column, sql_literal(raw));
        }
        std::optional<std::string> index;
        if (holds(t.index_kinds, kind)) {
            index = id;
        } else if (const auto p = t.primary_index.find(kind);
                   p != t.primary_index.end()) {
            for (const auto& field : p->second) {
                if (f.stated(field)) {
                    index = *f.get(field);
                    break;
                }
            }
        }
        if (index) {
            const auto uri = index_map.find(*index);
            if (uri == index_map.end())
                throw stop{};
            values.set("oresmd_uri", sql_literal(uri->second));
        }
        strings cells{"v_dataset_id", "v_tenant_id", *values.get("id"), "1"};
        if (party)
            cells.emplace_back("v_party_id");
        for (auto c = columns.begin() + static_cast<std::ptrdiff_t>(base);
             c != columns.end(); ++c) {
            const auto* v = values.get(*c);
            cells.push_back(v ? *v : "null");
        }
        out.rows.push_back(std::move(cells));
    }
    return out;
}

void emit(std::string& out, const block& b) {
    if (b.rows.empty())
        return;
    out += "\n    -- " + b.label + ": " + std::to_string(b.rows.size()) +
        " rows\n";
    out += "    if not exists (\n        select 1 from " + b.table +
        "\n        where dataset_id = v_dataset_id" + b.guard +
        "\n    ) then\n";
    out += "        insert into " + b.table + " (\n            " +
        joined(b.columns, ", ") + "\n        ) values\n";
    strings tuples;
    for (const auto& cells : b.rows)
        tuples.push_back("            (" + joined(cells, ", ") + ")");
    out += joined(tuples, ",\n");
    out += ";\n    end if;\n";
}

std::string replaced(std::string text, std::string_view from,
    std::string_view to) {
    for (auto at = text.find(from); at != std::string::npos;
         at = text.find(from, at + to.size()))
        text.replace(at, from.size(), to);
    return text;
}

std::string text_of_file(std::string_view bytes) {
    return pt::universal_newlines(pt::decode_replacing(bytes));
}

void run(const ct::corpus_view& v, ct::file_table& out) {
    const std::string script(v.option("script"));
    const auto slash = script.rfind('/');
    const auto here = slash == std::string::npos ? std::string()
        : script.substr(0, slash + 1);
    const auto source = v.read_file(script);
    const auto tsv = v.read_file(here + "conventions.tsv");
    const auto map = v.read_file(here + "oresmd_index_map.tsv");
    const auto methodology = v.read_file(here + "methodology.txt");
    if (!source || !tsv || !map || !methodology)
        return;
    try {
        const auto t = tables_of(text_of_file(*source));

        const auto records = ops::csv_rows(pt::decode_replacing(*tsv), '\t');
        if (records.empty())
            throw stop{};
        const auto& header = records.front();
        std::vector<row> rows;
        for (std::size_t i = 1; i < records.size(); ++i) {
            const auto& record = records[i];
            if (record.empty())
                continue;
            row r;
            for (std::size_t c = 0; c < header.size(); ++c)
                r.insert_or_assign(header[c], c < record.size()
                    ? std::optional(record[c]) : std::nullopt);
            const auto k = r.find("kind");
            if (k == r.end() || !k->second || k->second->empty())
                continue;
            rows.push_back(std::move(r));
        }

        std::set<std::string> kinds;
        for (const auto& r : rows)
            kinds.insert(column_of(r, "kind"));
        for (const auto& k : kinds) {
            if (!t.kinds.contains(k) && !t.composites.contains(k) &&
                !t.cut_kinds.contains(k))
                throw stop{};
        }

        std::map<std::string, std::string, std::less<>> index_map;
        for (const auto line : co::split_lines(text_of_file(*map))) {
            if (co::strip(line).empty())
                continue;
            const auto tab = line.find('\t');
            if (tab == std::string_view::npos || tab == 0 ||
                tab + 1 == line.size() ||
                line.find('\t', tab + 1) != std::string_view::npos)
                throw stop{};
            index_map.insert_or_assign(std::string(line.substr(0, tab)),
                std::string(line.substr(tab + 1)));
        }
        for (const auto& r : rows) {
            const auto stated = std::string(co::strip(optional_column(r,
                "oresmd_uri")));
            if (stated.empty())
                continue;
            const auto& id = column_of(r, "id");
            if (!holds(t.index_kinds, column_of(r, "kind")))
                throw stop{};
            if (const auto it = index_map.find(id);
                it != index_map.end() && it->second != stated)
                throw stop{};
            index_map.insert_or_assign(id, stated);
        }
        for (const auto& r : rows) {
            const auto& k = column_of(r, "kind");
            if (!t.kinds.contains(k) && !t.composites.contains(k))
                continue;
            const fields f(column_of(r, "signature"));
            for (const auto& name : referenced(t, k, column_of(r, "id"), f)) {
                if (!index_map.contains(name))
                    throw stop{};
            }
        }

        std::string sql = replaced(replaced(replaced(replaced(replaced(
            replaced(std::string(render_template), "@HEADER@", t.header),
            "@METHODOLOGY@", t.methodology), "@DESCRIPTION@",
            t.methodology_description), "@URL@", t.methodology_url),
            "@CODE@", t.dataset_code), "@TEXT@", sql_literal(co::strip(
            text_of_file(*methodology))));
        for (const auto& k : kinds) {
            if (t.composites.contains(k)) {
                for (const auto& b : composite_blocks(t, rows, k))
                    emit(sql, b);
            } else if (t.kinds.contains(k)) {
                emit(sql, flat_block(t, rows, k, index_map));
            }
        }
        sql += "end $$;\n";
        out.add(std::string(v.option("output")), sql);
    } catch (const stop&) {
    } catch (const ops::csv_error&) {
    }
}

}

const ct::descriptor conventions_dataset{
    .name = "conventions_dataset",
    .version = 1,
    .summary = "Writes the populate script of the refdata.conventions "
        "dataset from its curated table.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = {},
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::file_table>{run}};

}
