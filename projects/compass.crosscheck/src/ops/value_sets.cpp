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
#include "compass.crosscheck/ops/value_sets.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <tuple>
#include "compass.crosscheck/ops/ore_schema.hpp"
#include "compass.crosscheck/ops/projection_columns.hpp"
#include "compass.crosscheck/ops/python_numbers.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::ops::value_sets {

namespace {

namespace ct = compass::codec::types;
namespace cops = compass::codec::ops;
namespace pt = compass::platform::text;
using kind = json_value::kind;

constexpr std::string_view curation_dir =
    "projects/ores.seeder/datasets/ore_pricing_engines/curation/";
constexpr std::string_view vocabulary_path =
    "projects/ores.seeder/datasets/ore_pricing_engines/pricing_vocabulary.json";
constexpr std::string_view utility = "projects/ores.sql/utility/";
constexpr std::string_view xsd_dir = "external/ore/xsd/";
constexpr std::string_view curve_schema = "curveconfig.xsd";
constexpr std::string_view convention_schema = "conventions.xsd";

constexpr std::array<std::string_view, 4> catalogue_columns{"code", "name",
    "relation", "description"};
constexpr std::array<std::string_view, 2> catalogue_kind_columns{"kind",
    "catalogue"};
constexpr std::array<std::string_view, 2> rendered_columns{"kind",
    "renderings"};
constexpr std::array<std::string_view, 2> without_list_columns{"kind", "set"};
constexpr std::array<std::string_view, 2> list_of_columns{"kind", "parser"};
constexpr std::array<std::string_view, 3> type_set_columns{"family", "type",
    "set"};
constexpr std::array<std::string_view, 2> schema_type_set_columns{"kind",
    "type"};
constexpr std::array<std::string_view, 6> own_set_columns{"code", "name",
    "description", "source_kind", "pattern", "is_ordered"};
constexpr std::array<std::string_view, 4> override_columns{"element", "term",
    "kind", "reason"};
constexpr std::array<std::string_view, 1> curation_key_columns{"key"};
constexpr std::array<std::string_view, 1> source_columns{"source"};
constexpr std::array<std::string_view, 5> list_item_columns{"leaf", "kind",
    "source", "source_ref", "reason"};
constexpr std::array<std::string_view, 1> pattern_columns{"pattern"};
constexpr std::array<std::string_view, 2> parser_table_columns{"parser",
    "table"};
constexpr std::array<std::string_view, 4> family_columns{"table", "name",
    "description", "label"};
constexpr std::array<std::string_view, 2> schema_kind_columns{"xsd_type",
    "kind"};
constexpr std::array<std::string_view, 1> count_columns{"count"};
constexpr std::array<std::string_view, 2> item_word_columns{"kind", "words"};
constexpr std::array<std::string_view, 3> schema_word_columns{"kind", "name",
    "words"};

/**
 * @brief The tables of the value sets' check document the value sets
 * themselves are resolved with; projection_columns names the rest.
 */
constexpr std::array<cops::table_need, 19> needed{{
    {"Catalogues", catalogue_columns},
    {"Catalogue kinds", catalogue_kind_columns},
    {"Rendered kinds", rendered_columns},
    {"Kinds without a list", without_list_columns},
    {"Kinds one parser lists", list_of_columns},
    {"Type sets", type_set_columns},
    {"Schema type sets", schema_type_set_columns},
    {"Convention own sets", own_set_columns},
    {"Type overrides", override_columns},
    {"Curation keys", curation_key_columns},
    {"Curation sources", source_columns},
    {"List items", list_item_columns},
    {"Period pattern", pattern_columns},
    {"Parser tables", parser_table_columns},
    {"Rendering families", family_columns},
    {"Schema kinds", schema_kind_columns},
    {"Unread leaves allowed", count_columns},
    {"Item words", item_word_columns},
    {"Schema words", schema_word_columns}}};

/**
 * @brief Where a generator raises: the curation cannot be resolved.
 */
struct stop {
    std::string why;
};

[[noreturn]] void fail(std::string why) { throw stop{std::move(why)}; }

/**
 * @brief d[@p key]: the member, where the script raises KeyError on a
 * missing one.
 */
const json_value& at(const json_value& j, std::string_view key) {
    const auto* found = j.get(key);
    if (found == nullptr)
        fail("a curation entry has no " + std::string(key));
    return *found;
}

std::string path_in(std::string_view dir, std::string_view name) {
    return std::string(dir) + std::string(name);
}

json_value read_json(const ct::corpus_view& v, const std::string& path) {
    const auto text = v.read_file(path);
    if (!text)
        fail(path + " is missing");
    auto j = read_json_value(*text);
    if (!j)
        fail(path + " is not JSON");
    return std::move(*j);
}

using string_map = std::map<std::string, std::string, std::less<>>;

json_value str_value(std::string s) { return json_value::string(std::move(s)); }

/**
 * @brief The table @p name of @p d, which read_tables() checked is there.
 */
const cops::data_table& table_of(const cops::data_tables& d,
    std::string_view name) {
    return *d.find(name);
}

const std::string& cell(const cops::data_table& t,
    const std::vector<std::string>& row, std::string_view column) {
    return row[t.column(column)];
}

/**
 * @brief The dict the table @p name states from column @p key to column
 * @p value, the last row winning, as a dict literal's last key does.
 */
string_map pairs_of(const cops::data_tables& d, std::string_view name,
    std::string_view key, std::string_view value) {
    const auto& t = table_of(d, name);
    string_map out;
    for (const auto& row : t.rows)
        out.insert_or_assign(cell(t, row, key), cell(t, row, value));
    return out;
}

json_value object_value() {
    json_value j;
    j.what = kind::object;
    return j;
}

void put(json_value& object, std::string key, json_value value) {
    object.keys.push_back(std::move(key));
    object.items.push_back(std::move(value));
}

/**
 * @brief The dict of dicts the table @p name states: per row, column
 * @p key to a dict of the string cells of @p fields; an empty cell of a
 * column of @p nullable is None.
 */
json_value records_of(const cops::data_tables& d, std::string_view name,
    std::string_view key, std::initializer_list<std::string_view> fields,
    std::initializer_list<std::string_view> nullable = {}) {
    const auto& t = table_of(d, name);
    auto out = object_value();
    for (const auto& row : t.rows) {
        auto record = object_value();
        for (const auto f : fields) {
            const auto& c = cell(t, row, f);
            put(record, std::string(f), c.empty() &&
                std::ranges::contains(nullable, f) ? json_value{}
                : str_value(c));
        }
        put(out, cell(t, row, key), std::move(record));
    }
    return out;
}

/**
 * @brief The tuple the one-column table @p name states, one item a row.
 */
json_value list_of(const cops::data_tables& d, std::string_view name,
    std::string_view column) {
    const auto& t = table_of(d, name);
    json_value out;
    out.what = kind::list;
    for (const auto& row : t.rows)
        out.items.push_back(str_value(cell(t, row, column)));
    return out;
}

/**
 * @brief The one cell of column @p column of the one-row table @p name.
 */
const std::string& single(const cops::data_tables& d, std::string_view name,
    std::string_view column) {
    const auto& t = table_of(d, name);
    return cell(t, t.rows.front(), column);
}

std::map<std::string, std::string, std::less<>> strings(const json_value& t) {
    std::map<std::string, std::string, std::less<>> out;
    for (std::size_t k = 0; k < t.keys.size(); ++k)
        out.insert_or_assign(t.keys[k], python_str(t.items[k]));
    return out;
}

std::string lower(std::string s) {
    for (auto& c : s)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    return s;
}

/**
 * @brief words(): a name as a person reads it, for example Dimension type.
 */
std::string words(std::string_view name) {
    static const pt::regex part_re(R"([A-Z]+(?![a-z])|[A-Z]?[a-z]+|[0-9]+)");
    std::vector<std::string> parts;
    for (const auto& m : part_re.find_all(name))
        parts.emplace_back(m.group());
    if (parts.empty())
        parts.emplace_back(name);
    std::string text;
    for (const auto& p : parts) {
        if (!text.empty())
            text += " ";
        const bool upper = p.size() > 1 &&
            std::ranges::any_of(p, [](char c) { return c >= 'A' && c <= 'Z'; }) &&
            std::ranges::none_of(p, [](char c) { return c >= 'a' && c <= 'z'; });
        text += upper ? p : lower(p);
    }
    if (!text.empty() && text[0] >= 'a' && text[0] <= 'z')
        text[0] = static_cast<char>(text[0] - 'a' + 'A');
    return text;
}

/**
 * @brief re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower().
 */
std::string snake(std::string_view name) {
    std::string out;
    for (std::size_t k = 0; k < name.size(); ++k) {
        if (k > 0 && name[k] >= 'A' && name[k] <= 'Z')
            out.push_back('_');
        out.push_back(name[k]);
    }
    return lower(out);
}

std::string parser_set_code(std::string_view prefix, std::string_view parser) {
    return std::string(prefix) + snake(parser.substr(5));
}

value_row to_row(const json_value& j) {
    value_row r;
    if (const auto* x = j.get("value")) r.value = *x;
    if (const auto* x = j.get("label")) r.label = *x;
    if (const auto* x = j.get("description")) r.description = *x;
    if (const auto* x = j.get("order")) r.order = *x;
    if (const auto* x = j.get("is_active")) r.is_active = *x;
    if (const auto* x = j.get("canonical_value")) r.canonical_value = *x;
    return r;
}

value_set to_set(const json_value& j) {
    value_set s;
    if (const auto* x = j.get("name")) s.name = *x;
    if (const auto* x = j.get("description")) s.description = *x;
    s.source_kind = std::string(j.str("source_kind"));
    if (const auto* x = j.get("cardinality")) s.cardinality = *x;
    if (const auto* x = j.get("catalogue_code")) s.catalogue_code = *x;
    if (const auto* x = j.get("minimum")) s.minimum = *x;
    if (const auto* x = j.get("maximum")) s.maximum = *x;
    if (const auto* x = j.get("pattern")) s.pattern = *x;
    if (const auto* x = j.get("is_ordered")) s.is_ordered = *x;
    if (const auto* x = j.get("values")) {
        s.values.emplace();
        for (const auto& item : x->items)
            s.values->push_back(to_row(item));
    }
    return s;
}

catalogue to_catalogue(const json_value& j) {
    catalogue c;
    if (const auto* x = j.get("name")) c.name = *x;
    if (const auto* x = j.get("relation")) c.relation = *x;
    if (const auto* x = j.get("description")) c.description = *x;
    if (const auto* x = j.get("is_active")) c.is_active = *x;
    return c;
}

/**
 * @brief A row the schema or a parser states, in the shape every caller
 * builds: a value, a label, a description and an order.
 */
value_row stated_row(std::string value, std::string label,
    std::string description, std::size_t order) {
    return {str_value(std::move(value)), str_value(std::move(label)),
        str_value(std::move(description)),
        json_value::integer(static_cast<long long>(order)), std::nullopt,
        std::nullopt};
}

using generated_set = std::optional<value_set>;
using set_choice = std::pair<std::string, generated_set>;

/**
 * @brief ore_renderings.json and the Parser tables and Rendering families
 * tables, which were ore_aliases.py's: which renderings of a
 * value ORE's reader converts to which form.
 */
class renderings final {
public:
    renderings(const json_value& file, const json_value& parser_table,
        const json_value& sets)
        : parser_table_(strings(parser_table)), sets_(sets) {
        const auto* tables = file.get("tables");
        if (tables != nullptr)
            for (std::size_t k = 0; k < tables->keys.size(); ++k) {
                auto& pairs = tables_[tables->keys[k]];
                const auto& groups = tables->items[k];
                for (std::size_t g = 0; g < groups.keys.size(); ++g)
                    for (const auto& r : groups.items[g].items)
                        pairs.emplace_back(python_str(r), groups.keys[g]);
            }
        if (tables_.size() < 5)
            fail("ore_renderings.json holds " + std::to_string(tables_.size()) +
                " tables, and ORE's reader holds at least five; the file "
                "likely lost entries");
    }

    [[nodiscard]] bool has_family(std::string_view table) const {
        return sets_.get(table) != nullptr;
    }

    [[nodiscard]] std::optional<std::string> table_of_parser(
        std::string_view parser) const {
        const auto t = parser_table_.find(parser);
        if (t == parser_table_.end())
            return std::nullopt;
        return t->second;
    }

    [[nodiscard]] std::optional<std::string> canonical_of(
        std::string_view table, std::string_view rendering) const {
        for (const auto& [spelling, canonical] : pairs(table))
            if (spelling == rendering)
                return canonical;
        return std::nullopt;
    }

    [[nodiscard]] std::vector<std::string> canonical_forms(
        std::string_view table) const {
        std::vector<std::string> forms;
        for (const auto& [spelling, canonical] : pairs(table))
            if (std::ranges::find(forms, canonical) == forms.end())
                forms.push_back(canonical);
        return forms;
    }

    /**
     * @brief compare(): what a set states against what the table states.
     */
    [[nodiscard]] std::vector<std::string> compare(
        const std::vector<value_row>& values, std::string_view table) const {
        std::vector<std::string> problems;
        std::vector<std::pair<std::string, std::optional<std::string>>> stated;
        for (const auto& v : values) {
            const auto key = python_str(v.value);
            std::optional<std::string> canonical;
            if (v.canonical_value)
                canonical = python_str(*v.canonical_value);
            const auto at = std::ranges::find(stated, key,
                &std::pair<std::string, std::optional<std::string>>::first);
            if (at == stated.end())
                stated.emplace_back(key, canonical);
            else
                at->second = canonical;
        }
        const auto find = [&](std::string_view key) {
            return std::ranges::find(stated, key,
                &std::pair<std::string, std::optional<std::string>>::first);
        };
        for (const auto& form : canonical_forms(table))
            if (find(form) == stated.end())
                problems.push_back("the set states no value for the canonical "
                    "form '" + form + "'");
        for (const auto& [spelling, form] : pairs(table)) {
            const auto s = find(spelling);
            if (s == stated.end())
                problems.push_back("the set states no value for the rendering '" +
                    spelling + "'");
            else if (spelling != form && s->second != form)
                problems.push_back("the set states '" + spelling + "' as " +
                    (s->second ? "'" + *s->second + "'" : "None") +
                    ", and ORE reads it as '" + form + "'");
        }
        for (const auto& [value, canonical] : stated)
            if (canonical && find(*canonical) == stated.end())
                problems.push_back("the set states '" + value + "' as '" +
                    *canonical + "', which it holds no value for");
        return problems;
    }

    /**
     * @brief rendered_set(): the set one family states, from ORE's table
     * and the parser's map.
     */
    [[nodiscard]] value_set rendered_set(std::string_view table,
        const json_value& parsers) const {
        std::optional<std::string> parser;
        for (const auto& [p, t] : parser_table_)
            if (t == table && !parser)
                parser = p;
        if (!parser)
            fail(std::string(table) + ": no ORE parser names the renderings "
                "of this family");
        const auto* family = sets_.get(table);
        const auto* entry = parsers.get(*parser);
        if (family == nullptr || entry == nullptr)
            fail(std::string(table) + ": no family or parser " + *parser);
        value_set s;
        s.name = at(*family, "name");
        s.description = at(*family, "description");
        s.source_kind = "enumerated";
        s.is_ordered = json_value::boolean(false);
        s.values = rows(table, at(*entry, "values"), family->str("label"));
        return s;
    }

private:
    using pair_list = std::vector<std::pair<std::string, std::string>>;

    [[nodiscard]] const pair_list& pairs(std::string_view table) const {
        static const pair_list none;
        const auto t = tables_.find(table);
        return t == tables_.end() ? none : t->second;
    }

    /**
     * @brief canonical_map() then rendering_rows().
     */
    [[nodiscard]] std::vector<value_row> rows(std::string_view table,
        const json_value& parser_values, std::string_view label_of) const {
        pair_list known;
        for (const auto& [spelling, canonical] : pairs(table)) {
            const auto at = std::ranges::find(known, spelling, &pair_list::value_type::first);
            if (at == known.end())
                known.emplace_back(spelling, canonical);
            else
                at->second = canonical;
        }
        std::set<std::string> read;
        for (const auto& item : parser_values.items)
            read.insert(python_str(at(item, "value")));
        const auto listed = [](const std::vector<std::string>& v) {
            std::string out;
            for (const auto& s : v)
                out += (out.empty() ? "'" : ", '") + s + "'";
            return out;
        };
        std::vector<std::string> unstated, unreachable, unwritable;
        std::set<std::string> known_names, forms;
        for (const auto& [s, c] : known) {
            known_names.insert(s);
            forms.insert(c);
        }
        for (const auto& r : read)
            if (!known_names.contains(r))
                unstated.push_back(r);
        for (const auto& k : known_names)
            if (!read.contains(k))
                unreachable.push_back(k);
        for (const auto& f : forms)
            if (!read.contains(f))
                unwritable.push_back(f);
        const std::string t(table);
        if (!unstated.empty())
            fail("ORE reads " + listed(unstated) + " and the " + t +
                " table states no canonical form for " +
                (unstated.size() > 1 ? "them" : "it") +
                "; the store would refuse a document the engine accepts");
        if (!unreachable.empty())
            fail("the " + t + " table maps " + listed(unreachable) +
                " and ORE's reader accepts no such rendering; the row states a "
                "rule nothing can apply");
        if (!unwritable.empty())
            fail("the " + t + " table makes " + listed(unwritable) +
                " the form a configuration holds, and no document may write it; "
                "the writer would need a second table, so choose a form the "
                "schema states");
        std::vector<std::string> order;
        std::map<std::string, std::vector<std::string>> groups;
        for (const auto& [rendering, canonical] : known) {
            if (!groups.contains(canonical)) {
                order.push_back(canonical);
                groups[canonical] = {canonical};
            }
            auto& members = groups[canonical];
            if (rendering != canonical &&
                std::ranges::find(members, rendering) == members.end())
                members.push_back(rendering);
        }
        std::vector<value_row> out;
        for (const auto& canonical : order) {
            const auto& members = groups[canonical];
            const auto label = label_of == "words" ? words(canonical) : canonical;
            out.push_back(stated_row(canonical, label,
                label + " is the form a configuration holds.", out.size()));
            for (std::size_t k = 1; k < members.size(); ++k) {
                auto row = stated_row(members[k], members[k], members[k] +
                    " is another rendering of " + label +
                    ", which is the form a configuration holds.", out.size());
                row.canonical_value = str_value(canonical);
                out.push_back(std::move(row));
            }
        }
        return out;
    }

    std::map<std::string, pair_list, std::less<>> tables_;
    std::map<std::string, std::string, std::less<>> parser_table_;
    json_value sets_;
};

/**
 * @brief stated_set(): the set the schema states for one simple type, and
 * its code.
 */
std::pair<std::string, value_set> stated_set(const ore_schema::schemas& schemas,
    std::string_view kind_name, std::string_view type_name,
    std::string_view prefix) {
    const auto found = schemas.restriction_of(type_name);
    if (!found)
        fail(std::string(type_name) + ": the schema states no restriction");
    const auto& t = found->type;
    const auto code = std::string(prefix) + snake(type_name);
    const auto where = "external/ore/xsd/" + t.file + ":" +
        std::to_string(t.line);
    const std::string type(type_name);
    value_set s;
    s.name = str_value(words(type_name));
    s.source_kind = std::string(kind_name);
    s.is_ordered = json_value::boolean(false);
    if (kind_name == "enumerated") {
        s.description = str_value("The values the ORE schema states for " +
            type + ", from " + where + ".");
        s.values.emplace();
        for (const auto& value : t.values) {
            if (value.empty())
                continue;
            s.values->push_back(stated_row(value, value, value +
                " is one of the values the ORE schema states for " + type + ".",
                s.values->size()));
        }
        return {code, s};
    }
    if (t.pattern) {
        s.description = str_value("The pattern a " + type + " must match, as "
            "the ORE schema states it at " + where + ".");
        s.pattern = str_value("^(" + *t.pattern + ")$");
    } else {
        s.description = str_value("The values the ORE schema allows for " +
            type + ": from " + t.minimum.value_or("minus infinity") + " to " +
            t.maximum.value_or("infinity") + ", at " + where + ".");
        const auto real = [](const std::string& text) {
            json_value v;
            v.what = kind::real;
            const auto f = python_float(text);
            if (!f)
                fail("could not convert string to float: '" + text + "'");
            v.text = *f;
            return v;
        };
        if (t.minimum)
            s.minimum = real(*t.minimum);
        if (t.maximum)
            s.maximum = real(*t.maximum);
    }
    return {code, s};
}

/**
 * @brief parser_set(): the set of one parser's map, every string it accepts
 * labelled with what it means.
 */
value_set parser_set(std::string_view parser, const json_value& entry) {
    const auto* listed = entry.get("values");
    const auto count = listed == nullptr ? 0 : listed->items.size();
    if (count < 2)
        fail(std::string(parser) + " lists " + std::to_string(count) +
            " value at " + python_str(at(entry, "source_ref")) +
            "; the extractor likely misread it");
    std::map<std::string, std::string> meaning_of;
    value_set s;
    s.values.emplace();
    for (const auto& item : listed->items) {
        const auto value = python_str(at(item, "value"));
        const auto means = python_str(at(item, "means"));
        if (const auto m = meaning_of.find(value); m != meaning_of.end()) {
            if (m->second != means)
                fail(std::string(parser) + " maps " + value + " to both " +
                    m->second + " and " + means);
            continue;
        }
        meaning_of.emplace(value, means);
        const auto meaning = words(means);
        s.values->push_back(stated_row(value,
            lower(meaning) != lower(value) ? meaning : value,
            value + " is one of the values the ORE code accepts, meaning " +
                meaning + ".", s.values->size()));
    }
    s.name = str_value(words(parser.substr(5)));
    s.description = str_value("The values the ORE parser " + std::string(parser) +
        " accepts, from its map at " + python_str(at(entry, "source_ref")) + ".");
    s.source_kind = "enumerated";
    s.is_ordered = json_value::boolean(false);
    return s;
}

/**
 * @brief The data a value set source adds: its catalogues, sets and links,
 * the codes it generates, and the links two drawn leaves dispute.
 */
struct family {
    std::map<std::string, catalogue, std::less<>> catalogues;
    std::map<std::string, value_set, std::less<>> sets;
    std::map<std::string, std::string, std::less<>> links;
    std::set<std::string, std::less<>> generated;
};

value_set catalogue_set(const json_value& catalogues, const std::string& code) {
    const auto* c = catalogues.get(code);
    if (c == nullptr)
        fail(code + ": not a catalogue");
    const auto name = python_str(at(*c, "name"));
    value_set s;
    s.name = str_value(name);
    s.description = str_value("One of the " + lower(name) +
        " that reference data holds.");
    s.source_kind = "catalogue";
    s.catalogue_code = str_value(code);
    s.is_ordered = json_value::boolean(false);
    return s;
}

/**
 * @brief Keep the sets some link names, and the catalogues they draw on.
 */
void keep_named(family& f, const std::set<std::string, std::less<>>& named,
    const json_value& catalogues) {
    std::erase_if(f.sets, [&](const auto& e) { return !named.contains(e.first); });
    for (std::size_t k = 0; k < catalogues.keys.size(); ++k) {
        const auto& code = catalogues.keys[k];
        if (std::ranges::any_of(f.sets, [&](const auto& e) {
                return e.second.catalogue_code &&
                    python_str(*e.second.catalogue_code) == code;
            }))
            f.catalogues.insert_or_assign(code, to_catalogue(catalogues.items[k]));
    }
}

void add_generated(family& f, const std::string& code, value_set generated) {
    f.generated.insert(code);
    if (const auto s = f.sets.find(code); s != f.sets.end() &&
        s->second != generated)
        fail(code + ": two sources state a different set under one code");
    f.sets.try_emplace(code, std::move(generated));
}

/**
 * @brief Everything the resolution reads, loaded once.
 */
class inputs final {
public:
    inputs(const ct::corpus_view& v, const cops::data_tables& data)
        : v_(v), data_(data) {
        const auto module = [](std::string_view name) {
            return path_in(utility, name);
        };
        conv_catalogues = records_of(data, "Catalogues", "code",
            {"name", "relation", "description"});
        curve_catalogues = conv_catalogues;
        conv_catalogue_of = pairs_of(data, "Catalogue kinds", "kind",
            "catalogue");
        conv_rendered_of = pairs_of(data, "Rendered kinds", "kind",
            "renderings");
        conv_without_list = pairs_of(data, "Kinds without a list", "kind",
            "set");
        conv_list_of = pairs_of(data, "Kinds one parser lists", "kind",
            "parser");
        conv_schema_type_sets = pairs_of(data, "Schema type sets", "kind",
            "type");
        curve_schema_type_sets = conv_schema_type_sets;
        const auto& types = table_of(data, "Type sets");
        for (const auto& row : types.rows) {
            auto& sets = cell(types, row, "family") == "curve"
                ? curve_type_sets : conv_type_sets;
            sets.insert_or_assign(cell(types, row, "type"),
                cell(types, row, "set"));
        }
        const auto& own = table_of(data, "Convention own sets");
        conv_own_sets = object_value();
        for (const auto& row : own.rows) {
            auto set = object_value();
            for (const auto field : {"name", "description", "source_kind"})
                put(set, field, str_value(cell(own, row, field)));
            if (const auto& p = cell(own, row, "pattern"); !p.empty())
                put(set, "pattern", str_value(p));
            put(set, "is_ordered", json_value::boolean(
                cell(own, row, "is_ordered") == "yes"));
            put(conv_own_sets, cell(own, row, "code"), std::move(set));
        }
        const auto& overrides = table_of(data, "Type overrides");
        conv_type_override = object_value();
        for (const auto& row : overrides.rows) {
            const auto& element = cell(overrides, row, "element");
            const auto& term = cell(overrides, row, "term");
            json_value stated;
            stated.what = kind::list;
            stated.items = {str_value(cell(overrides, row, "kind")),
                str_value(cell(overrides, row, "reason"))};
            put(conv_type_override, element.empty() ? term + "|"
                : element + "|" + term, std::move(stated));
        }
        curve_curation_keys = list_of(data, "Curation keys", "key");
        curve_sources = list_of(data, "Curation sources", "source");
        curve_list_item = records_of(data, "List items", "leaf",
            {"kind", "source", "source_ref", "reason"}, {"kind"});
        period_pattern = single(data, "Period pattern", "pattern");
        auto parser_table = object_value();
        for (const auto& [parser, table] : pairs_of(data, "Parser tables",
                "parser", "table"))
            put(parser_table, parser, str_value(table));
        rendering = std::make_unique<renderings>(
            read_json(v, module("ore_renderings.json")), parser_table,
            records_of(data, "Rendering families", "table",
                {"name", "description", "label"}));
        std::vector<std::pair<std::string, std::string>> xsd;
        for (const auto path : v.list_files(xsd_dir, ".xsd")) {
            if (path.find('/', xsd_dir.size()) != std::string_view::npos)
                continue;
            xsd.emplace_back(std::string(path.substr(xsd_dir.size())),
                v.read_file(path).value_or(""));
        }
        schemas = std::make_unique<ore_schema::schemas>(xsd,
            pairs_of(data, "Schema kinds", "xsd_type", "kind"));
        const auto& words = table_of(data, "Item words");
        item_words = object_value();
        for (const auto& row : words.rows) {
            const auto& w = cell(words, row, "words");
            put(item_words, cell(words, row, "kind"),
                w.empty() ? json_value{} : str_value(w));
        }
        schema_words = pairs_of(data, "Schema words", "kind", "words");
        schema_names = pairs_of(data, "Schema words", "kind", "name");
        unread_allowed = std::stoll(single(data, "Unread leaves allowed",
            "count"));
        convention_terms = read_json(v, module("convention_terms.json"));
        curve_terms = read_json(v, module("curve_terms.json"));
        vocabularies = read_json(v, module("ore_parameter_vocabularies.json"));
        const auto curation_path = module("curve_value_set_curation.json");
        if (v.file_exists(curation_path))
            curve_curation = read_json(v, curation_path);
        else
            curve_curation.what = kind::object;
    }

    /**
     * @brief The xpaths the convention kind terms carry, read on first use
     * as the script does.
     */
    /**
     * @brief The convention kind terms, resolved on first use and kept, so
     * one evaluation resolves them once.
     */
    const std::optional<convention_terms_result>& terms() {
        if (!terms_read_) {
            terms_ = ops::convention_terms(v_, data_);
            terms_read_ = true;
        }
        return terms_;
    }

    const std::set<std::string>& convention_drawn() {
        if (!convention_drawn_) {
            const auto& t = terms();
            if (!t)
                fail("the convention kind terms have problems; "
                    "generate_convention_kind_terms.py reports them");
            convention_drawn_.emplace();
            for (const auto& r : t->rows)
                convention_drawn_->insert(r.xpath);
        }
        return *convention_drawn_;
    }

    const std::vector<projection_column>& curve_rows() {
        if (!curve_rows_) {
            auto rows = ops::curve_columns(v_);
            if (!rows)
                fail("the curve configuration columns cannot be derived; "
                    "generate_curve_column_guards.py reports why");
            curve_rows_ = std::move(*rows);
        }
        return *curve_rows_;
    }

    json_value conv_catalogues, conv_own_sets, conv_type_override;
    std::map<std::string, std::string, std::less<>> conv_catalogue_of,
        conv_rendered_of, conv_without_list, conv_list_of,
        conv_schema_type_sets, conv_type_sets;
    json_value curve_catalogues, curve_curation_keys, curve_sources,
        curve_list_item;
    std::map<std::string, std::string, std::less<>> curve_type_sets,
        curve_schema_type_sets;
    std::string period_pattern;
    std::unique_ptr<renderings> rendering;
    std::unique_ptr<ore_schema::schemas> schemas;
    json_value item_words;
    std::map<std::string, std::string, std::less<>> schema_words, schema_names;
    long long unread_allowed = 0;
    json_value convention_terms, curve_terms, vocabularies, curve_curation;

private:
    const ct::corpus_view& v_;
    const cops::data_tables& data_;
    bool terms_read_ = false;
    std::optional<convention_terms_result> terms_;
    std::optional<std::set<std::string>> convention_drawn_;
    std::optional<std::vector<projection_column>> curve_rows_;
};

std::string join_path(const json_value& path) {
    std::string out;
    for (const auto& p : path.items)
        out += (out.empty() ? "" : "/") + python_str(p);
    return out;
}

/**
 * @brief The resolution of one set from the schema, shared by the
 * convention and curve families.
 */
std::optional<set_choice> schema_set(inputs& in, std::string_view schema,
    const std::string& address, std::string_view prefix,
    const std::map<std::string, std::string, std::less<>>& schema_type_sets,
    const std::map<std::string, std::string, std::less<>>& type_sets,
    const std::function<bool()>& drawn) {
    const auto found = in.schemas->rule(schema, address);
    if (!found)
        return std::nullopt;
    const auto& [kind_name, type_name] = *found;
    const auto stated = in.schemas->restriction_of(type_name);
    const bool bounded = stated && (stated->type.minimum || stated->type.maximum);
    if (const auto s = schema_type_sets.find(kind_name);
        s != schema_type_sets.end() && !bounded)
        return set_choice{type_sets.at(s->second), std::nullopt};
    if (!stated)
        return std::nullopt;
    if (!drawn())
        return set_choice{"text", std::nullopt};
    auto [code, set] = stated_set(*in.schemas, kind_name, type_name, prefix);
    return set_choice{code, std::move(set)};
}

/**
 * @brief convention_value_sets.py.
 */
class conventions final {
public:
    explicit conventions(inputs& in) : in_(in) {}

    [[nodiscard]] static std::string address_of(const json_value& term) {
        std::string leaf = python_str(at(term, "name"));
        if (const auto* item = term.get("list_item"); item && item->truthy())
            leaf += "/" + python_str(*item);
        const auto path = join_path(at(term, "path"));
        return "/Conventions/" + path + (path.empty() ? "" : "/") + leaf;
    }

    /**
     * @brief override_of(): the kind the tree states for a term and why.
     */
    [[nodiscard]] std::optional<std::pair<std::string, std::string>> override_of(
        std::string_view element, const json_value& term) const {
        const auto name = python_str(at(term, "name"));
        const auto* found = in_.conv_type_override.get(name + "|");
        if (found == nullptr)
            found = in_.conv_type_override.get(std::string(element) + "|" + name);
        if (found == nullptr || found->items.size() < 2)
            return std::nullopt;
        return std::pair{python_str(found->items[0]), python_str(found->items[1])};
    }

    set_choice set_of(const json_value& term, std::string_view element) {
        const auto& parsers = at(in_.convention_terms, "parsers");
        const auto override = override_of(element, term);
        const auto kind_name = override ? override->first
                                        : python_str(at(term, "type"));
        if (const auto r = in_.conv_rendered_of.find(kind_name);
            r != in_.conv_rendered_of.end())
            return {"convention." + kind_name,
                in_.rendering->rendered_set(r->second, parsers)};
        if (in_.conv_catalogue_of.contains(kind_name))
            return {"convention." + kind_name, std::nullopt};
        if (const auto t = in_.conv_type_sets.find(kind_name);
            t != in_.conv_type_sets.end())
            return {t->second, std::nullopt};
        const auto* term_parser = term.get("parser");
        std::string parser = term_parser ? python_str(*term_parser) : "None";
        bool has_parser = term_parser && term_parser->truthy();
        if (const auto l = in_.conv_list_of.find(kind_name);
            l != in_.conv_list_of.end()) {
            parser = l->second;
            has_parser = !parser.empty();
        }
        if (has_parser) {
            if (const auto* entry = parsers.get(parser)) {
                auto s = parser_set(parser, *entry);
                state_canonical(*s.values, parser);
                return {parser_set_code("convention.", parser), std::move(s)};
            }
        }
        if (const auto w = in_.conv_without_list.find(kind_name);
            w != in_.conv_without_list.end()) {
            if (auto stated = schema_set(in_, convention_schema,
                    address_of(term), "convention.", in_.conv_schema_type_sets,
                    in_.conv_type_sets,
                    [&] { return in_.convention_drawn().contains(address_of(term)); }))
                return std::move(*stated);
            return {w->second, std::nullopt};
        }
        fail(python_str(at(term, "source_ref")) + ": " +
            python_str(at(term, "name")) + " is read by " + parser +
            ", which states no list, and its kind " + kind_name +
            " names no set");
    }

    family data() {
        family f;
        for (std::size_t k = 0; k < in_.conv_own_sets.keys.size(); ++k)
            f.sets.insert_or_assign(in_.conv_own_sets.keys[k],
                to_set(in_.conv_own_sets.items[k]));
        for (const auto& [kind_name, cat] : in_.conv_catalogue_of)
            f.sets.insert_or_assign("convention." + kind_name,
                catalogue_set(in_.conv_catalogues, cat));
        const auto& terms_of = at(in_.convention_terms, "conventions");
        for (const auto& [element, convention] : sorted(terms_of))
            for (const auto& term : at(*convention, "terms").items) {
                auto [code, generated] = set_of(term, element);
                if (generated)
                    add_generated(f, code, std::move(*generated));
                const auto key = element + "|" + python_str(at(term, "name"));
                if (const auto l = f.links.find(key);
                    l != f.links.end() && l->second != code)
                    fail(key + " would take two sets, " + l->second + " and " +
                        code);
                f.links.insert_or_assign(key, code);
            }
        std::set<std::string, std::less<>> named;
        for (const auto& [key, code] : f.links)
            named.insert(code);
        keep_named(f, named, in_.conv_catalogues);
        return f;
    }

    [[nodiscard]] static std::vector<std::pair<std::string, const json_value*>>
    sorted(const json_value& object) {
        std::vector<std::pair<std::string, const json_value*>> out;
        for (std::size_t k = 0; k < object.keys.size(); ++k)
            out.emplace_back(object.keys[k], &object.items[k]);
        std::ranges::stable_sort(out, {}, &std::pair<std::string, const json_value*>::first);
        return out;
    }

private:
    /**
     * @brief stated(): the canonical form of a list's values, proven
     * against the table.
     */
    void state_canonical(std::vector<value_row>& values,
        const std::string& parser) const {
        const auto table = in_.rendering->table_of_parser(parser);
        if (!table)
            return;
        for (auto& value : values) {
            const auto v = python_str(value.value);
            const auto canonical = in_.rendering->canonical_of(*table, v);
            if (!canonical)
                fail(parser + " accepts '" + v + "', and the " + *table +
                    " table states no canonical form for it");
            if (*canonical != v)
                value.canonical_value = str_value(*canonical);
        }
        const auto problems = in_.rendering->compare(values, *table);
        if (!problems.empty()) {
            std::string joined;
            for (const auto& p : problems)
                joined += (joined.empty() ? "" : "; ") + p;
            fail(parser + ": " + joined);
        }
    }

    inputs& in_;
};

/**
 * @brief curve_value_sets.py.
 */
class curves final {
public:
    explicit curves(inputs& in) : in_(in) {
        const auto& declared = in_.vocabularies;
        std::vector<std::string> other;
        for (std::size_t k = 0; k < declared.keys.size(); ++k)
            if (declared.items[k].str("schema") != curve_schema)
                other.push_back(declared.keys[k]);
        if (!other.empty()) {
            std::ranges::sort(other);
            std::string names;
            for (const auto& o : other)
                names += (names.empty() ? "" : ", ") + o;
            fail("ore_parameter_vocabularies.json declares " + names +
                " for a schema no generator applies; add the rule that "
                "applies it or drop the row");
        }
        for (const auto& entry : declared.items)
            value_set_of_.insert_or_assign(python_str(at(entry, "element")),
                python_str(at(entry, "value_set")));
        check_curation();
    }

    [[nodiscard]] static std::string address_of(const json_value& term) {
        return "/CurveConfiguration/" + join_path(at(term, "path"));
    }

    /**
     * @brief terms(): every section's terms, sections in sorted order.
     */
    [[nodiscard]] std::vector<std::pair<std::string, const json_value*>> terms() const {
        std::vector<std::pair<std::string, const json_value*>> out;
        for (const auto& [section, body] :
            conventions::sorted(at(in_.curve_terms, "sections")))
            for (const auto& term : at(*body, "terms").items)
                out.emplace_back(section, &term);
        return out;
    }

    set_choice set_of(const json_value& term, std::string_view section) {
        const auto& parsers = at(in_.curve_terms, "parsers");
        const auto name = python_str(at(term, "name"));
        const auto* entry = in_.curve_curation.get(std::string(section) + "|" + name);
        if (entry == nullptr || !entry->truthy())
            entry = in_.curve_curation.get("*|" + name);
        if (entry != nullptr && entry->truthy()) {
            const auto link = python_str(at(*entry, "link"));
            if (link.starts_with("refdata."))
                return {"curve." + link.substr(link.find('.') + 1), std::nullopt};
            const auto* p = parsers.get(link);
            if (p == nullptr)
                fail(name + ": the curation links to " + link + ", which no ORE "
                    "source states a list for; correct the entry or drop it");
            return {parser_set_code("curve.", link), parser_set(link, *p)};
        }
        if (const auto vs = value_set_of_.find(name); vs != value_set_of_.end()) {
            if (const auto t = renderings_of(vs->second))
                return {vs->second, in_.rendering->rendered_set(*t, parsers)};
            return {catalogue_of(vs->second) ? vs->second : "text", std::nullopt};
        }
        const auto type = python_str(at(term, "type"));
        if (const auto t = in_.curve_type_sets.find(type);
            t != in_.curve_type_sets.end() && term.get("type")->is(kind::string)) {
            if (type == "list")
                if (const auto item = item_set_of(term))
                    return {*item, std::nullopt};
            return {t->second, std::nullopt};
        }
        if (auto stated = schema_set(in_, curve_schema, address_of(term), "curve.",
                in_.curve_schema_type_sets, in_.curve_type_sets,
                [&] { return is_drawn(term); }))
            return std::move(*stated);
        return {"text", std::nullopt};
    }

    [[nodiscard]] std::optional<std::string> item_set_of(const json_value& term) const {
        const auto* stated = in_.curve_list_item.get(python_str(at(term, "name")));
        if (stated == nullptr || !stated->truthy())
            return std::nullopt;
        const auto* k = stated->get("kind");
        if (k == nullptr || !k->truthy())
            return std::nullopt;
        return in_.curve_type_sets.at(python_str(*k));
    }

    [[nodiscard]] bool is_drawn(const json_value& term) {
        const auto address = address_of(term);
        return std::ranges::any_of(in_.curve_rows(),
            [&](const auto& r) { return r.xpath == address; });
    }

    family data() {
        const auto& parsers = at(in_.curve_terms, "parsers");
        family f;
        for (auto& [code, set] : own_sets())
            f.sets.insert_or_assign(code, std::move(set));
        std::set<std::string> value_sets;
        for (const auto& [element, vs] : value_set_of_)
            value_sets.insert(vs);
        for (const auto& vs : value_sets) {
            if (const auto t = renderings_of(vs)) {
                f.sets.insert_or_assign(vs, in_.rendering->rendered_set(*t, parsers));
                continue;
            }
            const auto cat = catalogue_of(vs);
            if (!cat)
                continue;
            f.sets.insert_or_assign(vs, catalogue_set(in_.curve_catalogues, *cat));
        }
        std::set<std::string> projected;
        for (const auto& r : in_.curve_rows())
            projected.insert(r.xpath);
        std::vector<std::pair<std::string,
            std::vector<std::pair<std::string, std::set<bool>>>>> codes;
        for (const auto& [section, term] : terms()) {
            auto [code, generated] = set_of(*term, section);
            if (generated)
                add_generated(f, code, std::move(*generated));
            const auto key = section + "|" + python_str(at(*term, "name"));
            auto at = std::ranges::find(codes, key, &decltype(codes)::value_type::first);
            if (at == codes.end()) {
                codes.emplace_back(key, decltype(codes)::value_type::second_type{});
                at = std::prev(codes.end());
            }
            auto c = std::ranges::find(at->second, code,
                &std::pair<std::string, std::set<bool>>::first);
            if (c == at->second.end()) {
                at->second.emplace_back(code, std::set<bool>{});
                c = std::prev(at->second.end());
            }
            c->second.insert(projected.contains(address_of(*term)));
        }
        for (const auto& [key, found] : codes) {
            std::vector<std::string> drawn;
            for (const auto& [code, shown] : found)
                if (shown.contains(true))
                    drawn.push_back(code);
            if (drawn.size() == 1)
                f.links.insert_or_assign(key, drawn.front());
            else if (drawn.empty() && found.size() == 1)
                f.links.insert_or_assign(key, found.front().first);
        }
        for (const auto& key : in_.curve_curation.keys) {
            const auto bar = key.find('|');
            const auto section = key.substr(0, bar);
            const auto name = bar == std::string::npos ? "" : key.substr(bar + 1);
            const bool matched = std::ranges::any_of(terms(), [&](const auto& t) {
                return (section.empty() || section == "*" || section == t.first) &&
                    python_str(at(*t.second, "name")) == name;
            });
            if (!matched)
                fail("curation entry " + key + " matches no curve leaf the ORE "
                    "source reads; correct the key or drop the entry");
        }
        std::set<std::string, std::less<>> named;
        for (const auto& [key, found] : codes)
            for (const auto& [code, shown] : found)
                named.insert(code);
        keep_named(f, named, in_.curve_catalogues);
        return f;
    }

private:
    /**
     * @brief OWN_SETS. The module states the date list's pattern by calling
     * the schema reader, which no literal can express, so the sets are
     * stated here, with the Period pattern table's pattern and the date
     * pattern from the schema's date type.
     */
    [[nodiscard]] std::vector<std::pair<std::string, value_set>> own_sets() const {
        const auto make = [](std::string name, std::string description,
            std::string source_kind, bool ordered) {
            value_set s;
            s.name = str_value(std::move(name));
            s.description = str_value(std::move(description));
            s.source_kind = std::move(source_kind);
            s.is_ordered = json_value::boolean(ordered);
            return s;
        };
        const auto many = [](value_set s) {
            s.cardinality = str_value("many");
            return s;
        };
        const auto patterned = [](value_set s, std::optional<std::string> p) {
            s.pattern = p ? str_value(*p) : json_value{};
            return s;
        };
        std::optional<std::string> date;
        if (const auto r = in_.schemas->restriction_of("date"); r && r->type.pattern)
            date = "^(" + *r->type.pattern + ")$";
        return {
            {"curve.flag", make("Flag", "True or false, as ORE writes a boolean.",
                "boolean", false)},
            {"curve.number", make("Number", "A number the ORE code parses as a real.",
                "number", false)},
            {"curve.integer", make("Whole number",
                "A whole number the ORE code parses as an integer.", "integer", false)},
            {"curve.period", patterned(make("Period", "A period such as 2D, 3M or "
                "1Y6M, which the ORE code parses as a period.", "regex", false),
                in_.period_pattern)},
            {"curve.number_list", many(make("Numbers", "A comma separated list of "
                "numbers, which the ORE code reads into one vector.", "number", true))},
            {"curve.period_list", many(patterned(make("Periods", "A comma separated "
                "list of periods such as 1Y,2Y,5Y, which the ORE code reads into one "
                "vector.", "regex", true), in_.period_pattern))},
            {"curve.date_list", many(patterned(make("Dates", "A comma separated list "
                "of dates, in a form the ORE schema and the ORE date parser accept.",
                "regex", true), date))},
            {"curve.list", many(make("List of text", "A comma separated list whose "
                "items no value set states yet. ORE reads it into one vector, so "
                "the leaf holds many values and the tree cannot yet say what one "
                "may be.", "text", true))}};
    }

    const json_value* declared(std::string_view value_set) const {
        for (const auto& entry : in_.vocabularies.items)
            if (entry.str("value_set") == value_set)
                return &entry;
        fail(std::string(value_set) + ": the declaration names a set no row backs");
    }

    [[nodiscard]] std::optional<std::string> renderings_of(std::string_view vs) const {
        const auto* entry = declared(vs);
        const auto* t = entry->get("renderings");
        if (t == nullptr || t->is(kind::null))
            return std::nullopt;
        const auto table = python_str(*t);
        if (!in_.rendering->has_family(table))
            fail(std::string(vs) + ": the declaration names " + table +
                ", which is not a family ORE's reader converts");
        return table;
    }

    [[nodiscard]] std::optional<std::string> catalogue_of(std::string_view vs) const {
        const auto* entry = declared(vs);
        const auto* c = entry->get("catalogue");
        if (c == nullptr || c->is(kind::null))
            return std::nullopt;
        const auto cat = python_str(*c);
        if (in_.curve_catalogues.get(cat) == nullptr)
            fail(std::string(vs) + ": the declaration names " + cat +
                ", which is not a catalogue this generator knows");
        return cat;
    }

    void check_curation() const {
        const auto& entries = in_.curve_curation;
        for (std::size_t k = 0; k < entries.keys.size(); ++k) {
            const auto& entry = entries.items[k];
            const auto& key = entries.keys[k];
            std::string missing;
            for (const auto& name : in_.curve_curation_keys.items) {
                const auto* f = entry.get(python_str(name));
                if (f == nullptr || !f->truthy())
                    missing += (missing.empty() ? "" : ", ") + python_str(name);
            }
            if (!missing.empty())
                fail("curation entry " + key + " has no " + missing);
            const auto source = python_str(at(entry, "source"));
            if (std::ranges::none_of(in_.curve_sources.items,
                    [&](const auto& s) { return python_str(s) == source; }))
                fail("curation entry " + key + " has an unknown source " + source);
            const auto link = python_str(at(entry, "link"));
            if (!link.starts_with("parse") && !link.starts_with("refdata."))
                fail("curation entry " + key + " links to " + link +
                    ", which is neither a parser nor a catalogue");
        }
    }

    inputs& in_;
    std::map<std::string, std::string, std::less<>> value_set_of_;
};

std::string scope_of(std::string_view scope) {
    if (scope == "model") return "ModelParameters";
    if (scope == "engine") return "EngineParameters";
    if (scope == "global") return "GlobalParameters";
    return std::string(scope);
}

/**
 * @brief contract_pairs(): every parameter the contract holds, as scope
 * and name.
 */
std::set<std::pair<std::string, std::string>> contract_pairs(
    const ct::corpus_view& v) {
    std::set<std::pair<std::string, std::string>> pairs;
    const auto vocabulary = read_json(v, std::string(vocabulary_path));
    for (const auto& entry : vocabulary.items)
        if (const auto* ps = entry.get("pairs"))
            for (const auto& pair : ps->items)
                if (const auto* settings = pair.get("settings"))
                    for (const auto& s : settings->items)
                        pairs.emplace(scope_of(python_str(at(s, "scope"))),
                            python_str(at(s, "name")));
    const auto code = read_json(v, path_in(curation_dir, "settings-code.json"));
    for (std::size_t k = 0; k < code.keys.size(); ++k)
        pairs.emplace(scope_of(python_str(at(code.items[k], "scope"))),
            code.keys[k]);
    const auto globals = read_json(v, path_in(curation_dir, "globals.json"));
    for (const auto& name : globals.keys)
        pairs.emplace("GlobalParameters", name);
    std::erase_if(pairs, [](const auto& p) { return p.second.empty(); });
    return pairs;
}

/**
 * @brief review(): a reviewed override, read only when it names its
 * provenance.
 */
json_value review(const ct::corpus_view& v, const std::string& path) {
    auto data = read_json(v, path);
    std::string missing;
    for (const auto* key : {"reason", "source", "source_ref", "curator", "date"}) {
        const auto* f = data.get(key);
        if (f == nullptr || !f->truthy() ||
            python_str(*f).find_first_not_of(" \t\n\r\f\v") == std::string::npos)
            missing += (missing.empty() ? "" : ", ") + std::string(key);
    }
    if (!missing.empty())
        fail(path + " has no " + missing);
    const auto source = data.str("source");
    if (source != "ore_guide" && source != "ore_code" && source != "authored" &&
        source != "seed")
        fail(path + " has an unknown source '" + std::string(source) + "'");
    return data;
}

void update_sets(model& m, const json_value* sets) {
    if (sets == nullptr)
        return;
    for (std::size_t k = 0; k < sets->keys.size(); ++k)
        m.sets.insert_or_assign(sets->keys[k], to_set(sets->items[k]));
}

void update_links(model& m, const json_value* links) {
    if (links == nullptr)
        return;
    for (std::size_t k = 0; k < links->keys.size(); ++k)
        m.links.insert_or_assign(links->keys[k], python_str(links->items[k]));
}

void drop_unnamed(model& m) {
    std::set<std::string, std::less<>> named;
    for (const auto& [key, code] : m.links)
        named.insert(code);
    std::erase_if(m.sets, [&](const auto& e) { return !named.contains(e.first); });
}

/**
 * @brief load(): the guide's sets with the code's over the top, then the
 * reviewed files.
 */
model load(const ct::corpus_view& v) {
    model m;
    const auto guide = read_json(v, path_in(curation_dir, "value-sets.json"));
    if (const auto* registry = guide.get("catalogues")) {
        if (registry->is(kind::list)) {
            for (const auto& entry : registry->items)
                m.catalogues.insert_or_assign(python_str(at(entry, "code")),
                    to_catalogue(entry));
        } else {
            for (std::size_t k = 0; k < registry->keys.size(); ++k)
                m.catalogues.insert_or_assign(registry->keys[k],
                    to_catalogue(registry->items[k]));
        }
    }
    update_sets(m, guide.get("sets"));
    update_links(m, guide.get("links"));
    const auto code = read_json(v, path_in(curation_dir, "value-sets-code.json"));
    update_sets(m, code.get("sets"));
    update_links(m, code.get("links"));
    if (const auto* guide_sets = guide.get("sets"))
        for (std::size_t k = 0; k < guide_sets->keys.size(); ++k) {
            const auto& entry = guide_sets->items[k];
            if (entry.str("source_kind") == "catalogue" &&
                m.sets.contains(guide_sets->keys[k]))
                m.sets.insert_or_assign(guide_sets->keys[k], to_set(entry));
        }
    for (const auto* name : {"value-sets-review.json", "value-sets-guide-enums.json"}) {
        const auto reviewed = review(v, path_in(curation_dir, name));
        update_links(m, reviewed.get("links"));
        update_sets(m, reviewed.get("sets"));
    }
    drop_unnamed(m);
    return m;
}

void add(model& m, family f, std::string_view what,
    std::vector<problem>& problems) {
    for (const auto& [code, set] : f.sets)
        if (m.sets.contains(code))
            problems.push_back({"clash", code + ": a " + std::string(what) +
                " set clashes with an existing set"});
    for (const auto& [key, code] : f.links)
        if (!f.sets.contains(code) && !m.sets.contains(code))
            problems.push_back({"clash", key + ": names " + code +
                ", which no set holds"});
    for (const auto& [key, code] : f.links)
        if (m.links.contains(key))
            problems.push_back({"clash", key + ": a " + std::string(what) +
                " link clashes with an existing link"});
    for (auto& [code, c] : f.catalogues)
        m.catalogues.insert_or_assign(code, std::move(c));
    for (auto& [code, set] : f.sets)
        m.sets.try_emplace(code, std::move(set));
    for (auto& [key, code] : f.links)
        m.links.insert_or_assign(key, std::move(code));
    m.generated.insert(f.generated.begin(), f.generated.end());
}

/**
 * @brief engine-settings-groups.json, as curation._groups() checks it and
 * setup_value_sets.data() reads it. Only this file is read: a defect in
 * another curation file belongs to the gate that reads it.
 */
family setup(const ct::corpus_view& v,
    std::set<std::pair<std::string, std::string>>& drawn) {
    const auto name = std::string("engine-settings-groups.json");
    const auto data = read_json(v, path_in(curation_dir, name));
    std::vector<std::string> keys = data.keys;
    std::ranges::sort(keys);
    if (keys != std::vector<std::string>{"groups", "value_sets"})
        fail(name + " states sections other than groups and value_sets");
    const auto& sets = at(data, "value_sets");
    const auto blank = [](const json_value* f) {
        return f == nullptr || !f->is(kind::string) ||
            f->text.find_first_not_of(" \t\n\r\f\v") == std::string::npos;
    };
    for (std::size_t k = 0; k < sets.keys.size(); ++k)
        for (const auto* key : {"name", "description", "source_kind", "reason",
                 "source", "source_ref", "curator", "date"})
            if (blank(sets.items[k].get(key)))
                fail(name + " set " + sets.keys[k] + " has no " + key);
    family f;
    for (std::size_t k = 0; k < sets.keys.size(); ++k) {
        const auto& entry = sets.items[k];
        value_set s;
        s.name = at(entry, "name");
        s.description = at(entry, "description");
        s.source_kind = std::string(entry.str("source_kind"));
        const auto* ordered = entry.get("is_ordered");
        s.is_ordered = ordered ? *ordered : json_value::boolean(false);
        s.values.emplace();
        if (const auto* values = entry.get("values"))
            for (const auto& item : values->items)
                s.values->push_back(to_row(item));
        const auto copy = [&](const char* key, std::optional<json_value>& to) {
            if (const auto* x = entry.get(key); x && !x->is(kind::null))
                to = *x;
        };
        copy("catalogue_code", s.catalogue_code);
        copy("minimum", s.minimum);
        copy("maximum", s.maximum);
        copy("pattern", s.pattern);
        f.sets.insert_or_assign(sets.keys[k], std::move(s));
    }
    for (const auto& group : at(data, "groups").items) {
        const auto where = name + " group " + python_str(at(group, "code"));
        for (const auto* key : {"code", "name", "description"})
            if (const auto* x = group.get(key); x == nullptr || !x->truthy())
                fail(where + " has no " + key);
        const auto* settings = group.get("settings");
        if (settings == nullptr)
            continue;
        for (const auto& setting : settings->items) {
            const auto ore = python_str(setting.get("ore") ? at(setting, "ore")
                                                            : json_value{});
            for (const auto* key : {"column", "ore", "value_set"})
                if (blank(setting.get(key)))
                    fail(where + ": the setting " + ore + " has no " + key);
            for (const auto* key : {"kind", "options", "minimum", "maximum"})
                if (const auto* x = setting.get(key); x && !x->is(kind::null))
                    fail(where + ": the setting " + ore + " states the control " +
                        key + " beside its set");
            const auto code = python_str(at(setting, "value_set"));
            if (!f.sets.contains(code))
                fail(where + ": the setting " + ore + " names the set '" + code +
                    "', which the file does not state");
            f.links.insert_or_assign("Setup|" + ore, code);
            drawn.emplace("Setup", ore);
        }
    }
    return f;
}

/**
 * @brief What a set states that its own kind cannot hold: validate() and
 * check_parameter_value_sets.set_problems(), as one list of rules.
 */
void set_rules(const model& m, std::vector<problem>& problems) {
    static const std::set<std::string, std::less<>> kinds{"enumerated",
        "catalogue", "range", "regex", "boolean", "integer", "number", "text"};
    for (const auto& [code, s] : m.sets) {
        const auto report = [&](std::string message) {
            problems.push_back({"impossible_set", code + ": " + std::move(message)});
        };
        if (!kinds.contains(s.source_kind)) {
            report("unknown source kind '" + s.source_kind + "'");
            continue;
        }
        const auto cardinality = s.cardinality ? python_str(*s.cardinality) : "one";
        if (cardinality != "one" && cardinality != "many")
            report("unknown cardinality '" + cardinality + "'");
        const auto set_bound = [](const std::optional<json_value>& b) {
            return b && !b->is(kind::null);
        };
        if (set_bound(s.minimum) && !s.minimum->is_number())
            report("a floor that is not a number");
        if (set_bound(s.maximum) && !s.maximum->is_number())
            report("a ceiling that is not a number");
        if (set_bound(s.minimum) && set_bound(s.maximum) &&
            s.minimum->is_number() && s.maximum->is_number() &&
            std::stod(s.minimum->text) > std::stod(s.maximum->text))
            report("a floor above its ceiling");
        const auto& values = s.values ? *s.values : std::vector<value_row>{};
        if (s.source_kind == "enumerated") {
            std::vector<std::string> stated;
            for (const auto& v : values)
                stated.push_back(v.value.is(kind::null) ? "" : python_str(v.value));
            std::set<std::string> unique(stated.begin(), stated.end());
            if (stated.empty())
                report("enumerated with no values");
            if (unique.size() != stated.size())
                report("enumerated with a value stated twice");
            if (unique.contains(""))
                report("enumerated with an empty value");
        } else if (s.source_kind == "catalogue") {
            const auto cat = s.catalogue_code ? python_str(*s.catalogue_code) : "None";
            if (!s.catalogue_code || !m.catalogues.contains(cat))
                report("names a catalogue that is not registered");
            if (!values.empty())
                report("a catalogue set that also lists values");
        } else if (s.source_kind == "regex" && !(s.pattern && s.pattern->truthy())) {
            report("a regular expression with no pattern");
        } else if ((s.source_kind == "integer" || s.source_kind == "number" ||
                       s.source_kind == "range") && !values.empty()) {
            report("a numeric set that also lists values");
        }
        std::set<std::string> held;
        for (const auto& v : values)
            held.insert(python_str(v.value));
        for (const auto& v : values) {
            if (!v.canonical_value || v.canonical_value->is(kind::null))
                continue;
            const auto canonical = python_str(*v.canonical_value);
            const auto value = python_str(v.value);
            if (canonical == value)
                report(value + " names itself as its canonical form, which "
                    "states nothing");
            else if (!held.contains(canonical))
                report(value + " names " + canonical + " as its canonical form, "
                    "and the set holds no such value");
        }
    }
}

void link_rules(const model& m, std::vector<problem>& problems) {
    std::set<std::string, std::less<>> named;
    for (const auto& [key, code] : m.links) {
        named.insert(code);
        if (!m.sets.contains(code))
            problems.push_back({"unknown_set", key + ": names an unknown set " + code});
    }
    for (const auto& [code, s] : m.sets)
        if (!named.contains(code) && !m.generated.contains(code))
            problems.push_back({"unnamed_set", code + ": no setting names it"});
}

/**
 * @brief describe(): why one leaf takes one set, in words a reviewer can
 * check.
 */
map_row describe(inputs& in, const std::string& code, const value_set& entry,
    const json_value& term, std::string_view schema, const std::string& xpath,
    const std::optional<std::string>& overridden) {
    const auto& kind_name = entry.source_kind;
    const auto* parser_v = term.get("parser");
    const auto parser = parser_v ? python_str(*parser_v) : "None";
    const bool has_parser = parser_v && parser_v->truthy();
    const auto element = python_str(at(term, "name"));
    const auto* spelling_v = term.get("type");
    const auto spelling = spelling_v ? python_str(*spelling_v) : "None";
    const bool has_spelling = spelling_v && spelling_v->truthy();
    const auto cardinality = entry.cardinality ? python_str(*entry.cardinality) : "one";
    const auto rule = in.schemas->rule(schema, xpath);
    std::string source = "ore_code";
    const auto* ref = term.get("source_ref");
    std::string source_ref = ref ? python_str(*ref) : "";
    std::string reason;
    const auto* list_item = in.curve_list_item.get(element);
    if (overridden) {
        reason = *overridden;
    } else if (spelling == "list" && list_item && list_item->truthy()) {
        reason = python_str(at(*list_item, "reason"));
        source = python_str(at(*list_item, "source"));
        source_ref = python_str(at(*list_item, "source_ref"));
    } else if (spelling == "text" && rule && rule->first == "enumerated") {
        const auto r = in.schemas->restriction_of(rule->second);
        reason = "The ORE parser reads " + element + " as a string and the ORE "
            "schema enumerates " + std::to_string(r->type.values.size()) +
            " values for " + rule->second + ", so the set lists them.";
        source = "ore_xsd";
        source_ref = "external/ore/xsd/" + r->type.file + ":" +
            std::to_string(r->type.line);
    } else if (spelling == "text" && rule) {
        const auto stated = in.schemas->restriction_of(rule->second);
        if (stated && stated->type.pattern) {
            reason = "The ORE parser reads " + element + " as a string and the "
                "ORE schema states the pattern a " + rule->second + " must match.";
        } else if (stated && (stated->type.minimum || stated->type.maximum)) {
            reason = "The ORE parser reads " + element + " as a string and the "
                "ORE schema allows a " + rule->second + " from " +
                stated->type.minimum.value_or("minus infinity") + " to " +
                stated->type.maximum.value_or("infinity") + ".";
        } else {
            const auto w = in.schema_words.find(rule->first);
            if (w == in.schema_words.end())
                fail("SCHEMA_WORDS has no " + rule->first);
            reason = "The ORE parser reads " + element + " as a string and the "
                "ORE schema declares it " + rule->second + ", so the value is " +
                w->second + ".";
        }
        source = "ore_xsd";
        source_ref = stated ? "external/ore/xsd/" + stated->type.file + ":" +
                std::to_string(stated->type.line)
                            : "external/ore/xsd/" + std::string(schema);
    } else if (cardinality == "many") {
        const auto* item = in.item_words.get(kind_name);
        reason = "ORE reads " + element + " into one vector, so the leaf holds a "
            "comma separated list and the set states what one item may be" +
            (item && item->truthy() ? ": " + python_str(*item) + "."
                : std::string(", which no rule states yet: the set says the leaf "
                    "is a list and admits any item."));
    } else if (kind_name == "catalogue") {
        reason = "ORE reads " + element + " with " + parser + ", which accepts "
            "the spellings reference data holds, so the set names that catalogue "
            "instead of copying it.";
    } else if (kind_name == "enumerated") {
        reason = "ORE reads " + element + " with " + parser + ", and the set lists "
            "every value that parser's map accepts.";
    } else if (kind_name == "text" && spelling == "calendar") {
        reason = "ORE parses " + element + " with " + parser + ", and a calendar "
            "may be a joint calendar such as US,MWK that no catalogue row holds, "
            "so the set states text.";
    } else if (kind_name == "text" && has_parser) {
        reason = "ORE parses " + element + " with " + parser + ", and this tree "
            "holds no catalogue of the names it accepts, so the set states text.";
    } else if (kind_name == "text") {
        reason = "ORE reads " + element + " and never parses it, so no vocabulary "
            "governs the value and the set states text.";
    } else {
        reason = "ORE reads " + element + " as " +
            (has_spelling ? spelling : kind_name) +
            ", and the set states the values that kind admits.";
    }
    map_row r;
    r.set = code;
    r.name = python_str(entry.name);
    r.source = source;
    r.source_ref = source_ref;
    r.reason = reason;
    return r;
}

std::vector<map_column> column_rows(inputs& in, const ct::corpus_view& v) {
    static const pt::regex element_re(R"(/([^/\[]+)(?:\[[^\]]*\])?$)");
    const auto& terms = in.terms();
    if (!terms)
        fail("the convention kind terms have problems; "
            "generate_convention_kind_terms.py reports them");
    std::map<std::string, map_column> found;
    const auto put = [&](std::string schema, const std::string& entity,
        const std::string& table, const std::string& column,
        const std::string& xpath, const std::string& row_code) {
        const auto m = element_re.search(xpath);
        if (!m)
            fail(row_code + ": no element in " + xpath);
        map_column c;
        c.map_code = schema + "|" + xpath;
        c.code = c.map_code + "|" + entity + "|" + column;
        c.schema = std::move(schema);
        c.element = std::string(m->group(1));
        c.entity = entity;
        c.table = table;
        c.column = column;
        c.xpath = xpath;
        found.insert_or_assign(c.code, std::move(c));
    };
    for (const auto& r : in.curve_rows())
        put(std::string(curve_schema), r.entity, r.table, r.column, r.xpath, r.code);
    for (const auto& r : terms->rows) {
        const auto table = ops::convention_table(v, r.entity);
        if (!table)
            fail(r.entity + ": its model states no :tablename:");
        put(std::string(convention_schema), r.entity, *table, r.column, r.xpath,
            r.code);
    }
    std::vector<map_column> out;
    std::map<std::string, int> seen;
    for (auto& [code, c] : found) {
        c.position = seen[c.map_code]++;
        out.push_back(std::move(c));
    }
    return out;
}

std::vector<map_row> map_rows(inputs& in, const model& m,
    const std::vector<map_column>& columns, conventions& conv, curves& curve) {
    std::map<std::pair<std::string, std::string>, const json_value*> declared;
    for (const auto& entry : in.vocabularies.items)
        declared.insert_or_assign({python_str(at(entry, "schema")),
            python_str(at(entry, "element"))}, &entry);
    std::map<std::string, const map_column*> found;
    std::set<std::pair<std::string, std::string>> addresses;
    for (const auto& c : columns)
        if (found.try_emplace(c.map_code, &c).second)
            addresses.emplace(c.schema, c.xpath);
    std::map<std::pair<std::string, std::string>, map_row> rules;
    const auto set_named = [&](const std::string& code) -> const value_set& {
        const auto s = m.sets.find(code);
        if (s == m.sets.end())
            fail(code + ": the element map names a set the curation does not hold");
        return s->second;
    };
    for (const auto& [section, term] : curve.terms()) {
        const auto address = std::pair{std::string(curve_schema),
            curves::address_of(*term)};
        if (!addresses.contains(address))
            continue;
        const auto code = curve.set_of(*term, section).first;
        auto row = describe(in, code, set_named(code), *term, address.first,
            address.second, std::nullopt);
        rules.try_emplace(address, std::move(row));
    }
    for (const auto& [element, convention] :
        conventions::sorted(at(in.convention_terms, "conventions")))
        for (const auto& term : at(*convention, "terms").items) {
            const auto address = std::pair{std::string(convention_schema),
                conventions::address_of(term)};
            if (!addresses.contains(address))
                continue;
            const auto code = conv.set_of(term, element).first;
            const auto overridden = conv.override_of(element, term);
            auto row = describe(in, code, set_named(code), term, address.first,
                address.second, overridden
                    ? std::optional<std::string>(overridden->second) : std::nullopt);
            rules.try_emplace(address, std::move(row));
        }
    std::vector<map_row> out;
    for (const auto& [code, c] : found) {
        map_row r;
        r.code = code;
        r.schema = c->schema;
        r.xpath = c->xpath;
        r.element = c->element;
        if (const auto d = declared.find({c->schema, c->element});
            d != declared.end()) {
            const auto& e = *d->second;
            r.set = python_str(at(e, "value_set"));
            r.name = python_str(at(e, "name"));
            r.source = python_str(at(e, "source"));
            r.source_ref = python_str(at(e, "source_ref"));
            r.reason = python_str(at(e, "reason"));
            out.push_back(std::move(r));
            continue;
        }
        std::optional<map_row> rule;
        if (const auto f = rules.find({c->schema, c->xpath}); f != rules.end())
            rule = f->second;
        else if (c->schema == curve_schema) {
            if (const auto found_rule = in.schemas->rule(c->schema, c->xpath)) {
                map_row s;
                if (found_rule->first == "enumerated") {
                    const auto [set_code, set] = stated_set(*in.schemas,
                        "enumerated", found_rule->second, "curve.");
                    const auto stated = in.schemas->restriction_of(found_rule->second);
                    s.set = set_code;
                    s.name = python_str(set.name);
                    s.source_ref = "external/ore/xsd/" + stated->type.file + ":" +
                        std::to_string(stated->type.line);
                    s.reason = "No ORE source read names " + c->element + " in a "
                        "form the extractor knows, and the ORE schema enumerates " +
                        std::to_string(stated->type.values.size()) + " values for " +
                        found_rule->second + ", so the set lists them.";
                } else {
                    const auto t = in.curve_schema_type_sets.find(found_rule->first);
                    const auto n = in.schema_names.find(found_rule->first);
                    const auto w = in.schema_words.find(found_rule->first);
                    if (t == in.curve_schema_type_sets.end() ||
                        n == in.schema_names.end() || w == in.schema_words.end())
                        fail(c->xpath + ": the schema kind " + found_rule->first +
                            " names no set");
                    s.set = in.curve_type_sets.at(t->second);
                    s.name = n->second;
                    s.source_ref = "external/ore/xsd/" + c->schema;
                    s.reason = "No ORE source read names " + c->element + " in a "
                        "form the extractor knows, and the ORE schema declares it " +
                        found_rule->second + ", so the value is " + w->second + ".";
                }
                s.source = "ore_xsd";
                rule = std::move(s);
            }
        }
        if (rule) {
            r.set = rule->set;
            r.name = rule->name;
            r.source = rule->source;
            r.source_ref = rule->source_ref;
            r.reason = rule->reason;
        } else {
            r.set = "text";
            r.name = c->element;
            r.source = "ore_schema";
            r.source_ref = "external/ore/xsd/" + c->schema;
            r.reason = "No ORE source read names " + c->element + " as a term: "
                "the extractor does not know the form this leaf is read in, so no "
                "parser states a rule for it and the set states text. Listed by "
                "the element map coverage report.";
        }
        out.push_back(std::move(r));
    }
    return out;
}

std::string q(const json_value& v) {
    switch (v.what) {
    case kind::null: return "null";
    case kind::boolean: return v.text;
    case kind::integer:
    case kind::real: return python_str(v);
    case kind::string: break;
    case kind::list:
    case kind::object: break;
    }
    if (v.text.empty())
        return "null";
    std::string out = "'";
    for (const char c : v.text) {
        if (c == '\'')
            out.push_back('\'');
        out.push_back(c);
    }
    return out + "'";
}

std::string q(const std::optional<json_value>& v, json_value fallback = {}) {
    return q(v ? *v : fallback);
}

std::string q(std::string_view s) { return q(json_value::string(std::string(s))); }

std::string sql_quoted(std::string_view s) {
    std::string out = "'";
    for (const char c : s) {
        if (c == '\'')
            out.push_back('\'');
        out.push_back(c);
    }
    return out + "'";
}

constexpr std::string_view seed_licence = R"(/* -*- sql-product: postgres; tab-width: 4; indent-tabs-mode: nil -*-
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
)";

constexpr std::string_view map_licence = R"(/* -*- sql-product: postgres; tab-width: 4; indent-tabs-mode: nil -*-
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
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
 * Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
)";

std::string write(std::string_view label, std::string_view table,
    std::string_view columns, const std::vector<std::vector<std::string>>& rows) {
    std::string values;
    for (const auto& r : rows) {
        if (!values.empty())
            values += ",\n";
        values += "    (";
        for (std::size_t k = 0; k < r.size(); ++k)
            values += (k == 0 ? "" : ", ") + r[k];
        values += ")";
    }
    const std::string l(label), t(table);
    return "\\echo '--- " + l + " ---'\n\ninsert into " + t + " (\n    " +
        std::string(columns) + "\n) values\n" + values +
        "\non conflict (tenant_id, code)\nwhere valid_to = "
        "ores_utility_infinity_timestamp_fn()\ndo nothing;\n\nselect '" + l +
        "' as entity, count(*) as count\nfrom " + t +
        "\nwhere valid_to = ores_utility_infinity_timestamp_fn();\n";
}

}

std::expected<cops::data_tables, unreadable> read_tables(
    const ct::corpus_view& v, std::string_view codec) {
    std::vector<cops::table_need> needs(needed.begin(), needed.end());
    std::ranges::copy(convention_terms_tables(), std::back_inserter(needs));
    const auto text = v.option(cops::data_option);
    auto data = cops::tables_for(codec, text, needs);
    if (!data) {
        const auto decoded = cops::decode_data_tables(text);
        return std::unexpected(unreadable{text.empty() || !decoded
            ? "compass/checks/" + std::string(codec) + ".org"
            : decoded->source, data.error()});
    }
    const auto wrong = [&](const cops::data_table& t, std::uint32_t line,
        std::string_view what) {
        return std::unexpected(unreadable{data->source, std::format(
            "{}:{}: {}, in the table \"{}\" {} reads", data->source, line,
            what, t.name, codec)});
    };
    for (const auto name : {"Period pattern", "Convention schema",
             "Unread leaves allowed"}) {
        const auto& t = table_of(*data, name);
        if (t.rows.size() != 1)
            return wrong(t, t.line, std::format("{} rows state what one "
                "row states", t.rows.size()));
    }
    const auto& count = table_of(*data, "Unread leaves allowed");
    if (const auto& c = cell(count, count.rows[0], "count");
        c.empty() || c.size() > 9 || !std::ranges::all_of(c, [](char x) {
            return x >= '0' && x <= '9';
        }))
        return wrong(count, count.row_lines[0], "the count '" + c +
            "' is not a whole number");
    const auto& own = table_of(*data, "Convention own sets");
    for (std::size_t r = 0; r < own.rows.size(); ++r)
        if (const auto& o = cell(own, own.rows[r], "is_ordered");
            o != "yes" && o != "no")
            return wrong(own, own.row_lines[r], "is_ordered is '" + o +
                "', not yes or no");
    const auto& types = table_of(*data, "Type sets");
    for (std::size_t r = 0; r < types.rows.size(); ++r)
        if (const auto& f = cell(types, types.rows[r], "family");
            f != "convention" && f != "curve")
            return wrong(types, types.row_lines[r], "the family '" + f +
                "' is neither convention nor curve");
    const auto& overrides = table_of(*data, "Type overrides");
    for (std::size_t r = 0; r < overrides.rows.size(); ++r)
        if (cell(overrides, overrides.rows[r], "term").empty())
            return wrong(overrides, overrides.row_lines[r], "the override "
                "names no term");
    return std::move(*data);
}

outcome evaluate(const ct::corpus_view& v, const cops::data_tables& data) {
    outcome out;
    try {
        inputs in(v, data);
        out.unread_allowed = in.unread_allowed;
        model m = load(v);
        const auto contract = contract_pairs(v);
        out.drawn = contract;
        std::set<std::pair<std::string, std::string>> linked;
        for (const auto& [key, code] : m.links) {
            const auto bar = key.find('|');
            linked.emplace(key.substr(0, bar),
                bar == std::string::npos ? "" : key.substr(bar + 1));
        }
        for (const auto& [scope, name] : contract)
            if (!linked.contains({scope, name}))
                out.problems.push_back({"unlinked_parameter", scope + "|" + name +
                    " has no value set, and a link is curated, not inferred"});
        std::erase_if(m.links, [&](const auto& e) {
            const auto bar = e.first.find('|');
            return !contract.contains({e.first.substr(0, bar),
                bar == std::string::npos ? "" : e.first.substr(bar + 1)});
        });
        drop_unnamed(m);
        conventions conv(in);
        curves curve(in);
        add(m, conv.data(), "convention", out.problems);
        add(m, curve.data(), "curve", out.problems);
        std::set<std::pair<std::string, std::string>> setup_drawn;
        add(m, setup(v, setup_drawn), "setup", out.problems);
        for (const auto& [scope, name] : setup_drawn) {
            out.drawn.emplace(scope, name);
            if (!m.links.contains(scope + "|" + name))
                out.problems.push_back({"unlinked_parameter", scope + "|" + name +
                    " has no value set, and a link is curated, not inferred"});
        }
        set_rules(m, out.problems);
        link_rules(m, out.problems);
        std::ranges::sort(out.problems, {}, [](const problem& p) {
            return std::tie(p.rule, p.message);
        });
        out.data = std::move(m);
        if (!out.problems.empty()) {
            out.map_stopped = "the value sets have problems";
            return out;
        }
        try {
            auto columns = column_rows(in, v);
            out.rows = map_rows(in, out.data, columns, conv, curve);
            out.columns = std::move(columns);
        } catch (const stop& s) {
            out.map_stopped = s.why;
        }
        out.terms = in.terms();
    } catch (const stop& s) {
        out = outcome{};
        out.stopped = s.why;
        out.map_stopped = s.why;
    }
    return out;
}

std::string render_seed(const model& m) {
    const std::string tenant = "ores_utility_system_tenant_id_fn()";
    const std::string actor = "'ores_reporting_service', 'system.initial_load'";
    const auto seed = q("Seed parameter value sets");
    std::vector<std::vector<std::string>> catalogues, sets, values, links;
    for (const auto& [code, c] : m.catalogues)
        catalogues.push_back({q(code), tenant, "0", q(c.name), q(c.relation),
            q(c.description), q(c.is_active, json_value::boolean(true)), actor, seed});
    for (const auto& [code, s] : m.sets) {
        sets.push_back({q(code), tenant, "0", q(s.name), q(s.description),
            q(s.source_kind), q(s.cardinality, json_value::string("one")),
            q(s.catalogue_code), q(s.minimum), q(s.maximum), q(s.pattern),
            q(s.is_ordered, json_value::boolean(false)), actor, seed});
        if (!s.values)
            continue;
        for (const auto& v : *s.values)
            values.push_back({q(code + ":" + python_str(v.value)), tenant, "0",
                q(code), q(v.value), q(v.label), q(v.description),
                python_str(v.order), q(v.is_active, json_value::boolean(true)),
                q(v.canonical_value), actor, seed});
    }
    for (const auto& [key, code] : m.links) {
        const auto bar = key.find('|');
        const auto scope = key.substr(0, bar);
        const auto name = bar == std::string::npos ? "" : key.substr(bar + 1);
        links.push_back({q(scope + ":" + name), tenant, "0", q(scope), q(name),
            q(code), actor, seed});
    }
    return std::string(seed_licence) + R"(
/**
 * Parameter Value Sets Population Script
 *
 * The allowed values of an ORE parameter: the enumerated sets, the catalogues a
 * set may draw on, and the setting each set belongs to. Generated by
 * projects/ores.sql/utility/generate_value_sets.py from the curation file.
 * Do not edit it by hand. This script is idempotent.
 */

)" + write("Value Set Catalogues",
        "ores_reporting_ore_parameter_value_set_catalogues_tbl",
        "code, tenant_id, version, name, relation, description, is_active, "
        "modified_by, change_reason_code, change_commentary", catalogues) +
        write("Value Sets", "ores_reporting_ore_parameter_value_sets_tbl",
        "code, tenant_id, version, name, description, source_kind, cardinality, "
        "catalogue_code, minimum, maximum, pattern, is_ordered, modified_by, "
        "change_reason_code, change_commentary", sets) +
        write("Value Set Values", "ores_reporting_ore_parameter_value_set_values_tbl",
        "code, tenant_id, version, value_set_code, value, label, description, "
        "display_order, is_active, canonical_value, modified_by, "
        "change_reason_code, change_commentary", values) +
        write("Setting Value Sets",
        "ores_reporting_ore_parameter_setting_value_sets_tbl",
        "code, tenant_id, version, scope, name, value_set_code, modified_by, "
        "change_reason_code, change_commentary", links);
}

std::string render_columns(const std::vector<map_column>& columns) {
    std::string values;
    for (const auto& c : columns) {
        if (!values.empty())
            values += ",\n";
        values += "    (" + sql_quoted(c.code) +
            ", ores_utility_system_tenant_id_fn(), 0, " + sql_quoted(c.map_code) +
            ", " + sql_quoted(c.entity) + ", " + sql_quoted(c.table) + ", " +
            sql_quoted(c.column) + ", " + sql_quoted(c.xpath) + ", " +
            std::to_string(c.position) +
            ", 'ores_reporting_service', 'system.initial_load', " +
            sql_quoted("Seed the ORE element map columns") + ")";
    }
    return std::string(map_licence) + R"(
/**
 * Ore Element Map Columns Population Script
 *
 * The projection columns that carry an ORE leaf, and the map row that states the
 * leaf's value set. Generated by
 * projects/ores.sql/utility/generate_ore_element_map.py from the curve
 * configuration projection, the convention mappers and the ORE schema. Do not
 * edit it by hand. This script is idempotent.
 */

insert into ores_reporting_ore_element_map_columns_tbl (
    code, tenant_id, version, element_map_code, entity_name, table_name,
    column_name, xpath, position, modified_by, change_reason_code,
    change_commentary
) values
)" + values + R"(
on conflict (tenant_id, code)
where valid_to = ores_utility_infinity_timestamp_fn()
do nothing;

select 'Ore Element Map Columns' as entity, count(*) as count
from ores_reporting_ore_element_map_columns_tbl
where valid_to = ores_utility_infinity_timestamp_fn();
)";
}

std::string render_map(const std::vector<map_row>& rows) {
    std::string values;
    for (const auto& r : rows) {
        if (!values.empty())
            values += ",\n";
        values += "    (" + sql_quoted(r.code) +
            ", ores_utility_system_tenant_id_fn(), 0, " + sql_quoted(r.schema) +
            ", " + sql_quoted(r.xpath) + ", " + sql_quoted(r.element) + ", " +
            sql_quoted(r.set) + ", " + sql_quoted(r.name) + ", " + sql_quoted(r.source) +
            ", " + sql_quoted(r.source_ref) + ", " + sql_quoted(r.reason) +
            ", 'ores_reporting_service', 'system.initial_load', " +
            sql_quoted("Seed the ORE element map") + ")";
    }
    return std::string(map_licence) + R"(
/**
 * ORE Element Map Population Script
 *
 * One row per ORE leaf: the value set that governs it, where the rule comes
 * from, and why. Generated by
 * projects/ores.sql/utility/generate_ore_element_map.py from the parameter
 * contract, its setting links and ore_parameter_vocabularies.json. Do not edit
 * it by hand. This script is idempotent.
 *
 * A leaf the contract does not describe takes =text=, so the map names every
 * leaf the guards check and a gap is a stated decision rather than a silent
 * pass.
 */

insert into ores_reporting_ore_parameter_vocabularies_tbl (
    code, tenant_id, version, schema_name, xpath, element_name, value_set_code,
    name, source, source_ref, reason, modified_by, change_reason_code,
    change_commentary
) values
)" + values + R"(
on conflict (tenant_id, code)
where valid_to = ores_utility_infinity_timestamp_fn()
do nothing;

select 'ORE Element Map' as entity, count(*) as count
from ores_reporting_ore_parameter_vocabularies_tbl
where valid_to = ores_utility_infinity_timestamp_fn();
)";
}

}
