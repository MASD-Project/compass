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
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codec/types/fact.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.crosscheck/ops/python_literals.hpp"
#include "compass.crosscheck/ops/subjects.hpp"
#include "compass.crosscheck/ops/words.hpp"
#include "compass.model/ops/load.hpp"
#include "compass.model/ops/outline.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace mo = compass::model::ops;
namespace mt = compass::model::types;
namespace pt = compass::platform::text;

enum rule : std::uint32_t { money_not_exact, continuous_exact,
    unpublished_area, unreadable_areas, contradicted, unset_required,
    self_contradicting, opt_out, outside_protocol, unreadable_model,
    unregistered_unit, twice_registered, missing_unit };

constexpr std::array<std::string_view, 2> money_not_exact_sources{
    "E47BAF35-066F-4EF2-BA3D-522AED44FF33",
    "B9DAC38A-BEB1-4658-87BC-DFF901033E84"};

constexpr std::array<std::string_view, 1> continuous_exact_sources{
    "B9DAC38A-BEB1-4658-87BC-DFF901033E84"};

constexpr std::array<ct::rule, 13> rules{{
    {"money_not_exact", "A money column is numeric in SQL and a decimal in "
        "C++, never a binary float or text.", money_not_exact_sources,
        ct::blame_method::line},
    {"continuous_exact", "A continuous quantity is not typed numeric or "
        "decimal, an exactness it does not have.", continuous_exact_sources,
        ct::blame_method::line},
    {"unpublished_area", "Every #+http_area: a model declares is a "
        "published area.", {}, ct::blame_method::line},
    {"unreadable_areas", "The route classification module states AREAS "
        "and NO_AREA as literals."},
    {"contradicted", "A model does not redeclare a feature its profile "
        "fixes to another value.", {}, ct::blame_method::line},
    {"unset_required", "A model sets every feature its profile requires.",
        {}, ct::blame_method::added},
    {"self_contradicting", "A model states a feature once, or always with "
        "one value.", {}, ct::blame_method::line},
    {"opt_out", "No model switches the TypeScript protocol facet off.",
        {}, ct::blame_method::line},
    {"outside_protocol", "Every NATS subject a model declares is inside the "
        "entity protocol.", {}, ct::blame_method::line},
    {"unreadable_model", "Every model is UTF-8."},
    {"unregistered_unit", "A component's route aggregate registers the "
        "route unit of every model that opts into one."},
    {"twice_registered", "An aggregate registers each unit once.",
        {}, ct::blame_method::line},
    {"missing_unit", "Every unit an aggregate registers has its source in "
        "the component."}}};

constexpr std::array<ct::option, 3> declared_options{{
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
    {"classification", "build/scripts/route_classification.py",
        "The Python module whose AREAS and NO_AREA name the published "
        "areas."}}};

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

/**
 * @brief Whether @p key is a feature name as model-drift reads one:
 * lower-case letters and underscores.
 */
bool feature_name(std::string_view key) {
    return !key.empty() && std::ranges::all_of(key, [](char c) {
        return (c >= 'a' && c <= 'z') || c == '_';
    });
}

/**
 * @brief Each feature a model states in a drawer, with every heading path
 * and value it is stated with, in document order.
 */
using statements = std::map<std::string_view,
    std::vector<std::pair<std::string, std::string_view>>, std::less<>>;

statements stated_features(const mo::outline& o) {
    statements out;
    for (std::size_t i = 0; i < o.file_property_keys.size(); ++i) {
        const auto value = co::strip(o.file_property_values[i]);
        if (feature_name(o.file_property_keys[i]) && !value.empty())
            out[o.file_property_keys[i]].emplace_back("(top)", value);
    }
    std::vector<std::string> paths(o.size());
    for (std::size_t n = 1; n < o.size(); ++n) {
        std::size_t parent = n - 1;
        while (parent > 0 && o.levels[parent] >= o.levels[n])
            --parent;
        paths[n] = parent == 0 ? std::string(o.titles[n])
                               : paths[parent] + " / " +
                std::string(o.titles[n]);
    }
    for (std::size_t i = 0; i < o.property_keys.size(); ++i) {
        const auto value = co::strip(o.property_values[i]);
        if (feature_name(o.property_keys[i]) && !value.empty())
            out[o.property_keys[i]].emplace_back(
                paths[o.property_nodes[i].value], value);
    }
    return out;
}

void write_features(const mo::outline& o, ct::fact_writer& fact) {
    const auto all = stated_features(o);
    const auto profile = all.find("profile");
    if (profile == all.end()) {
        fact.number(0);
        return;
    }
    fact.number(static_cast<std::uint32_t>(all.size()));
    for (const auto& [key, entries] : all) {
        fact.text(key);
        const bool agree = std::ranges::all_of(entries, [&](const auto& e) {
            return e.second == entries.front().second; });
        if (agree) {
            fact.number(1).text(entries.front().second);
            continue;
        }
        fact.number(static_cast<std::uint32_t>(entries.size()));
        for (const auto& [where, value] : entries)
            fact.text(where).text(value);
    }
}

/**
 * @brief The features a profile fixes, from the first two cells of each
 * row of the tables of its Assignments section, a feature's id link read
 * as its label; a later row of one feature replaces its value.
 */
void write_assignments(const ct::document& d, const mo::outline& o,
    ct::fact_writer& fact) {
    auto slug = std::string(co::stem_of(d.path).substr(
        std::string_view("variability_").size()));
    std::ranges::replace(slug, '_', '-');
    std::vector<std::pair<std::string, std::string>> fixed;
    const auto section = mo::section(o, mo::node_id{0}, "Assignments");
    for (const auto table : section.is_none() ? std::vector<std::uint32_t>{}
                                              : mo::tables_of(o, section)) {
        const auto first = mo::first_row(o, table);
        for (auto row = first; row < first + mo::row_count(o, table); ++row) {
            if (mo::cell_count(o, row) != 2)
                continue;
            auto feature = co::strip(mo::cell(o, row, 0));
            if (feature.starts_with("[[id:")) {
                const auto open = feature.find("][");
                if (open == std::string_view::npos || !feature.ends_with("]]"))
                    continue;
                feature = feature.substr(open + 2, feature.size() - open - 4);
            }
            const std::string value(co::strip(mo::cell(o, row, 1)));
            if (!feature_name(feature) || feature == "feature" ||
                value.find_first_not_of("-+") == std::string::npos)
                continue;
            const auto it = std::ranges::find_if(fixed, [&](const auto& f) {
                return f.first == feature; });
            if (it != fixed.end())
                it->second = value;
            else
                fixed.emplace_back(feature, value);
        }
    }
    fact.text(slug).number(static_cast<std::uint32_t>(fixed.size()));
    for (const auto& [feature, value] : fixed)
        fact.text(feature).text(value);
}

/**
 * @brief The fact of a model: each #+http_area: with the offset of its
 * line, then, when it binds a profile, the profiles it binds and what it
 * states of each feature (fact_kind::model); of a profile, its slug and
 * the features its Assignments table fixes (fact_kind::profile).
 */
enum class fact_kind : std::uint32_t { other, model, profile };

void write_areas(const ct::document& d, const mo::outline& o,
    ct::fact_writer& fact) {
    std::vector<std::pair<std::uint32_t, std::string_view>> areas;
    for (std::size_t i = 0; i < o.frontmatter_keys.size(); ++i) {
        if (o.frontmatter_keys[i] == "http_area")
            areas.emplace_back(line_start(d.source, o.frontmatter_keys[i]),
                co::strip(o.frontmatter_values[i]));
    }
    fact.number(static_cast<std::uint32_t>(areas.size()));
    for (const auto& [offset, area] : areas)
        fact.number(offset).text(area);
}

/**
 * @brief The route unit a model opts into: 1 and the singular that names
 * it, the first #+entity_singular: or #+name_singular:, when its file
 * drawer switches ores.cpp.http-route on as code generation reads an
 * override; 0 otherwise.
 */
void write_route(const mo::outline& o, ct::fact_writer& fact) {
    const auto enabled = mo::file_property(o, "ores.cpp.http-route.enabled");
    const auto value = ops::lowered(co::strip(enabled.value));
    const bool on = enabled.found && (value == "true" || value == "t" ||
        value == "yes" || value == "1");
    std::string_view singular;
    for (std::size_t i = 0; i < o.frontmatter_keys.size(); ++i) {
        if (o.frontmatter_keys[i] == "entity_singular" ||
            o.frontmatter_keys[i] == "name_singular") {
            singular = co::strip(o.frontmatter_values[i]);
            break;
        }
    }
    if (!on || singular.empty()) {
        fact.number(0);
        return;
    }
    fact.number(1).text(singular);
}

/**
 * @brief The route units the models of each component opt into, by
 * component and singular, with the model that opts in; a later model of
 * one singular wins.
 */
using route_opt_ins = std::map<std::string,
    std::map<std::string, std::string_view>, std::less<>>;

/**
 * @brief The component of a model directly under projects/ores.<c>/modeling/,
 * or empty for a model elsewhere.
 */
std::string_view component_of(std::string_view model) {
    const auto parts = co::components_of(model);
    if (parts.size() != 4 || parts[2] != "modeling" ||
        !parts[1].starts_with("ores."))
        return {};
    return parts[1].substr(std::string_view("ores.").size());
}

/**
 * @brief route-units: each aggregate
 * projects/ores.http/<c>/src/routes/<c>/<c>_routes.cpp registers the unit
 * of every model of ores.<c> that opts into one, once, and each unit it
 * registers has its source beside it.
 */
void check_routes(const ct::corpus_view& v, const route_opt_ins& opted,
    ct::diagnostic_table& out) {
    static const pt::regex registers(R"(^\s*(\w+)_routes::register_routes\()");
    for (const auto aggregate : v.list_files("projects/ores.http/",
             "_routes.cpp")) {
        const auto parts = co::components_of(aggregate);
        if (parts.size() != 7 || parts[3] != "src" || parts[4] != "routes" ||
            parts[5] != parts[2] ||
            parts[6] != std::string(parts[2]) + "_routes.cpp")
            continue;
        const auto dir = co::parent_of(aggregate);
        const auto text = pt::universal_newlines(pt::decode_replacing(
            v.read_file(aggregate).value_or("")));
        std::vector<std::string> called;
        for (const auto line : co::split_lines(text)) {
            if (const auto m = registers.match(line))
                called.emplace_back(m->group(1));
        }
        if (const auto it = opted.find(parts[2]); it != opted.end()) {
            for (const auto& [unit, model] : it->second) {
                if (std::ranges::find(called, unit) == called.end())
                    co::report(out, aggregate, 0, unregistered_unit,
                        "never registers " + unit + "_routes, which " +
                        std::string(model) + " opts into; the gateway "
                        "answers nothing for it and nothing reports it",
                        "Register the unit, or remove the model's "
                        ":ores.cpp.http-route.enabled: opt-in.");
            }
        }
        std::vector<std::string> seen;
        for (const auto& unit : called) {
            if (std::ranges::find(seen, unit) != seen.end())
                continue;
            seen.push_back(unit);
            const auto n = std::ranges::count(called, unit);
            if (n > 1)
                co::report(out, aggregate, 0, twice_registered, "registers " +
                    unit + "_routes " + std::to_string(n) + " times; the "
                    "second registration claims routes the first stated",
                    "Register each unit once.");
        }
        for (const auto& unit : called) {
            if (!v.file_exists(std::string(dir) + "/" + unit + "_routes.cpp"))
                co::report(out, aggregate, 0, missing_unit, "registers " +
                    unit + "_routes, which has no unit in this component",
                    "Remove the registration, or restore the unit.");
        }
    }
}

/**
 * @brief What a model states for a feature: its one value, none, or every
 * statement when they disagree.
 */
struct statement {
    std::optional<std::string_view> value;
    std::vector<std::pair<std::string_view, std::string_view>> disagreement;
};

using stated_map = std::map<std::string_view, statement, std::less<>>;

stated_map read_features(ct::fact_reader& fact) {
    stated_map out;
    for (auto n = fact.number(); n > 0; --n) {
        auto& s = out[fact.text()];
        const auto entries = fact.number();
        if (entries == 1) {
            s.value = fact.text();
            continue;
        }
        for (auto i = entries; i > 0; --i) {
            const auto where = fact.text();
            s.disagreement.emplace_back(where, fact.text());
        }
    }
    return out;
}

using profile_table = std::map<std::string_view,
    std::vector<std::pair<std::string_view, std::string_view>>, std::less<>>;

/**
 * @brief model-drift for one model that binds profiles: a feature a bound
 * profile fixes and the model states otherwise, or one it requires and the
 * model leaves unset, or states two ways.
 */
void check_drift(std::string_view path, const stated_map& values,
    const profile_table& profiles, ct::diagnostic_table& out) {
    const auto bound = values.find("profile");
    if (bound == values.end() || !bound->second.value)
        return;
    constexpr std::string_view fix = "A model that names a profile must not "
        "redeclare a feature the profile fixes to another value.";
    std::map<std::string_view, std::pair<std::string_view, std::string_view>>
        fixed_by;
    for (const auto raw : ops::split(*bound->second.value, ',')) {
        const auto name = co::strip(raw);
        // codegen's model loader refuses an unknown profile and two
        // profiles that disagree, and that refusal stops the check.
        const auto profile = profiles.find(name);
        if (name.empty() || profile == profiles.end())
            continue;
        for (const auto& [feature, expected] : profile->second) {
            if (const auto it = fixed_by.find(feature);
                it != fixed_by.end() && it->second.second != expected)
                continue;
            fixed_by[feature] = {name, expected};
            const auto s = values.find(feature);
            if (s != values.end() && !s->second.disagreement.empty()) {
                std::string where;
                for (const auto& [w, value] : s->second.disagreement)
                    where += (where.empty() ? "" : "; ") + std::string(w) +
                        "=" + std::string(value);
                co::report(out, path, 0, self_contradicting, "states " +
                    std::string(feature) + " twice with different values (" +
                    where + ")", fix);
                continue;
            }
            const auto actual = s == values.end()
                ? std::optional<std::string_view>{} : s->second.value;
            if (expected.starts_with("required")) {
                if (!actual || actual->empty())
                    co::report(out, path, 0, unset_required, "profile '" +
                        std::string(name) + "' requires " +
                        std::string(feature) + " to be set, model states "
                        "none", fix);
                continue;
            }
            if (actual && *actual != expected)
                co::report(out, path, 0, contradicted, "profile '" +
                    std::string(name) + "' fixes " + std::string(feature) +
                    "=" + std::string(expected) + ", model declares " +
                    std::string(*actual), fix);
        }
    }
}

/**
 * @brief The line of @p source that holds @p at, without the spaces at
 * either end.
 */
std::string_view line_at(std::string_view source, std::string_view at) {
    const auto begin = line_start(source, at);
    const auto end = source.find('\n', begin);
    return co::strip(source.substr(begin, end == std::string_view::npos
        ? std::string_view::npos : end - begin));
}

/**
 * @brief Each drawer property of the model, the file's drawer first, as
 * its key and value.
 */
std::vector<std::pair<std::string_view, std::string_view>> drawer_properties(
    const mo::outline& o) {
    std::vector<std::pair<std::string_view, std::string_view>> out;
    for (std::size_t i = 0; i < o.file_property_keys.size(); ++i)
        out.emplace_back(o.file_property_keys[i], o.file_property_values[i]);
    for (std::size_t i = 0; i < o.property_keys.size(); ++i)
        out.emplace_back(o.property_keys[i], o.property_values[i]);
    return out;
}

/**
 * @brief protocol-twins' opt-out and subject-conformance over a model's
 * drawers: a drawer that states the TypeScript protocol facet's switch,
 * and a :subject: outside the entity protocol.
 */
void check_drawers(const ct::document& d, const mo::outline& o,
    ct::diagnostic_table& out) {
    for (const auto& [key, value] : drawer_properties(o)) {
        if (key == "ores.ts.protocol.enabled") {
            co::report(out, d.path, line_start(d.source, key), opt_out,
                "the model opts out of the TypeScript protocol facet: " +
                std::string(line_at(d.source, key)), "Remove the key, so no "
                "model silences the facet.");
            continue;
        }
        const auto subject = co::strip(value);
        if (key != "subject" || subject.empty() ||
            subject.find_first_of(" \t") != std::string_view::npos)
            continue;
        const auto [ok, reason] = ops::classify_subject(subject);
        if (!ok)
            co::report(out, d.path, line_start(d.source, key),
                outside_protocol, std::string(subject) + ": " + reason,
                std::string(ops::subject_fix));
    }
}

void map(const ct::document& d, const ct::option_values& options,
    ct::file_scan& out) {
    const mo::document doc{d.path, d.source, &d.tables};
    const auto declared = mo::declared_type(doc);
    ct::fact_writer fact(out.fact);
    if (declared == "profile" &&
        co::stem_of(d.path).starts_with("variability_")) {
        mo::outline o;
        mo::build_outline(d.source, d.tables, o);
        fact.number(static_cast<std::uint32_t>(fact_kind::profile));
        write_assignments(d, o, fact);
        return;
    }
    const auto type = mo::model_type_of(declared);
    if (!type) {
        fact.number(static_cast<std::uint32_t>(fact_kind::other));
        return;
    }
    mo::outline o;
    mo::build_outline(d.source, d.tables, o);
    fact.number(static_cast<std::uint32_t>(fact_kind::model));
    if (!pt::is_utf8(d.source))
        co::report(out.findings, d.path, 0, unreadable_model, "the model is "
            "not UTF-8", "Save the model as UTF-8.");
    write_areas(d, o, fact);
    write_route(o, fact);
    write_features(o, fact);
    check_money(d, o, *type, options, out.findings);
    check_drawers(d, o, out.findings);
}


/**
 * @brief The published areas: AREAS less NO_AREA of the route
 * classification module, or nothing, reported, when they are not
 * literals.
 */
std::optional<std::set<std::string>> published_areas(const ct::corpus_view& v,
    ct::diagnostic_table& out) {
    const auto module_path = v.option("classification");
    const auto module = pt::universal_newlines(pt::decode_replacing(
        v.read_file(module_path).value_or("")));
    const auto areas = ops::python_assignment(module, "AREAS");
    const auto none = ops::python_assignment(module, "NO_AREA");
    const auto names = areas ? ops::strings_of(*areas) : std::nullopt;
    if (!names || !none || none->what != ops::python_value::kind::string) {
        co::report(out, module_path, 0, unreadable_areas,
            ops::python_source_problem("model_rules", module_path,
                "AREAS and NO_AREA", v.file_exists(module_path)),
            "Restore the file, or keep AREAS a tuple of strings and NO_AREA "
            "a string.");
        return std::nullopt;
    }
    std::set<std::string> published(names->begin(), names->end());
    published.erase(none->text);
    return published;
}

void reduce(std::span<const ct::scanned_file> files, const ct::corpus_view& v,
    ct::diagnostic_table& out) {
    profile_table profiles;
    bool areas_read = false;
    std::optional<std::set<std::string>> published;
    std::string listed;
    route_opt_ins opted;
    for (const auto& f : files) {
        ct::fact_reader fact(f.fact);
        if (static_cast<fact_kind>(fact.number()) != fact_kind::profile)
            continue;
        auto& fixed = profiles[fact.text()];
        for (auto n = fact.number(); n > 0; --n) {
            const auto feature = fact.text();
            fixed.emplace_back(feature, fact.text());
        }
    }
    for (const auto& f : files) {
        ct::fact_reader fact(f.fact);
        if (static_cast<fact_kind>(fact.number()) != fact_kind::model)
            continue;
        for (auto n = fact.number(); n > 0; --n) {
            const auto offset = fact.number();
            const std::string area(fact.text());
            if (!areas_read) {
                areas_read = true;
                published = published_areas(v, out);
                if (published)
                    for (const auto& a : *published)
                        listed += (listed.empty() ? "" : ", ") + a;
            }
            if (!published || published->contains(area))
                continue;
            co::report(out, f.path, offset, unpublished_area, "declares "
                "http_area '" + area + "', which is not a published area; "
                "published areas: " + listed, "Add the area to "
                "route_classification.AREAS, or use a published one.");
        }
        if (fact.number() == 1) {
            const auto singular = fact.text();
            if (const auto c = component_of(f.path); !c.empty())
                opted[std::string(c)][std::string(singular)] = f.path;
        }
        check_drift(f.path, read_features(fact), profiles, out);
    }
    check_routes(v, opted, out);
}

}

const ct::descriptor model_rules{
    .name = "model_rules",
    .version = 4,
    .summary = "Reports each codegen model that breaks a rule ORE Studio's "
        "gates state about one model.",
    .reads = ct::inputs::of_types({}),
    .options = declared_options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::scan_function{.files = models, .reduce = reduce,
        .map_document = map}};

}
