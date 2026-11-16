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
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "compass.model/ops/entity_claim.hpp"
#include "loaders.hpp"
#include "text.hpp"

namespace compass::model::ops {

namespace {

using types::value_id;

constexpr std::string_view product_families[] = {"option", "swap", "forward",
    "position", "swaption", "future", "repo", "security"};
constexpr std::string_view product_constructions[] = {"native", "scripted",
    "script", "container"};

template <std::size_t N>
bool one_of(const std::string_view (&set)[N], std::string_view v) {
    return std::ranges::find(set, v) != std::end(set);
}

/**
 * @brief str.split(@p separator): every piece, empty ones included.
 */
std::vector<std::string_view> split(std::string_view s, char separator) {
    std::vector<std::string_view> out;
    for (;;) {
        const auto at = s.find(separator);
        out.push_back(s.substr(0, at));
        if (at == std::string_view::npos)
            return out;
        s.remove_prefix(at + 1);
    }
}

std::string sql_text(std::string_view s) {
    std::string out;
    for (const char ch : collapse_spaces(s)) {
        out += ch;
        if (ch == '\'')
            out += '\'';
    }
    return out;
}

void set_commas(builder& b, const std::vector<value_id>& entries) {
    for (std::size_t i = 0; i < entries.size(); ++i)
        b.set(entries[i], "comma", i + 1 == entries.size() ? "" : ",");
}

/**
 * @brief One entity model of _entity_org_by_table: the table it claims,
 * its document and its names.
 */
struct entity_org {
    std::string table;
    std::string path;
    std::string singular;
    std::string plural;
};

/**
 * @brief _entity_org_by_table over projects/<p>/modeling/<f>.org, in
 * (p, f) order: by table in first-claimed order, a later claim of the
 * same table replacing the earlier one.
 */
std::vector<entity_org> entity_orgs(load_context& c) {
    constexpr std::string_view projects = "projects/";
    constexpr std::string_view modeling = "/modeling/";
    std::vector<std::pair<std::pair<std::string_view, std::string_view>,
        std::string_view>> paths;
    for (const auto path : c.list(projects)) {
        const auto rest = path.substr(projects.size());
        const auto at = rest.find(modeling);
        if (at == std::string_view::npos || at == 0)
            continue;
        const auto project = rest.substr(0, at);
        const auto file = rest.substr(at + modeling.size());
        if (project.contains('/') || file.contains('/') ||
            project.starts_with('.') || file.starts_with('.') ||
            !file.ends_with(".org"))
            continue;
        paths.push_back({{project, file}, path});
    }
    std::ranges::sort(paths);
    std::vector<entity_org> out;
    for (const auto& [key, path] : paths) {
        const auto d = c.find(path);
        if (!d)
            continue;
        auto claim = entity_claim_of(d->source);
        if (!claim)
            continue;
        entity_org e{std::move(claim->table), std::string(path),
            std::move(claim->entity_singular), std::move(claim->entity_plural)};
        const auto it = std::ranges::find(out, e.table, &entity_org::table);
        if (it == out.end())
            out.push_back(std::move(e));
        else
            *it = std::move(e);
    }
    return out;
}

/**
 * @brief The entity of @p orgs declaring @p singular, the last one when
 * several do, as a dict comprehension over them keeps it; or null.
 */
const entity_org* by_singular(const std::vector<entity_org>& orgs,
    std::string_view singular) {
    const entity_org* found = nullptr;
    for (const auto& e : orgs) {
        if (e.singular == singular)
            found = &e;
    }
    return found;
}

const entity_org* by_table(const std::vector<entity_org>& orgs,
    std::string_view table) {
    const auto it = std::ranges::find(orgs, table, &entity_org::table);
    return it == orgs.end() ? nullptr : &*it;
}

/**
 * @brief A document a loader reads beside the model, outlined.
 */
struct read_document {
    document d;
    outline o;
};

read_document read(load_context& c, std::string_view path) {
    const auto d = c.find(path);
    if (!d)
        throw model_error("no document at " + std::string(path));
    read_document out{*d, {}};
    build_outline(out.d.source, *out.d.tables, out.o);
    return out;
}

/**
 * @brief One child of an entity's * Columns: its title and its properties
 * by lowered key.
 */
struct declared_column {
    std::string_view name;
    std::map<std::string, std::string_view, std::less<>> properties;
};

/**
 * @brief _entity_columns: a title written twice keeps its first place and
 * its last properties.
 */
std::vector<declared_column> entity_columns(const outline& o) {
    std::vector<declared_column> out;
    for (const auto n : children(o, section(o, node_id{0}, "Columns"))) {
        declared_column column{o.titles[n.value], {}};
        for (const auto& p : properties(o, n))
            column.properties[to_lower(p.key)] = p.value;
        const auto it = std::ranges::find(out, column.name,
            &declared_column::name);
        if (it == out.end())
            out.push_back(std::move(column));
        else
            it->properties = std::move(column.properties);
    }
    return out;
}

std::string_view column_property(const declared_column& c,
    std::string_view key) {
    const auto it = c.properties.find(key);
    return it == c.properties.end() ? std::string_view() : it->second;
}

bool keyed_by_trade_id_alone(const std::vector<declared_column>& columns) {
    std::vector<std::string_view> keys;
    for (const auto& c : columns) {
        if (to_lower(column_property(c, "primary_key")) == "true")
            keys.push_back(c.name);
    }
    return keys.size() == 1 && keys.front() == "trade_id";
}

/**
 * @brief ^:economic:\s+(true|false)\s*$, multi-line: the leading
 * whitespace may run over line ends.
 */
bool declares_economic(std::string_view text) {
    constexpr std::string_view head = ":economic:";
    for (auto at = text.find(head); at != std::string_view::npos;
         at = text.find(head, at + 1)) {
        if (at != 0 && text[at - 1] != '\n')
            continue;
        auto p = at + head.size();
        const auto spaced = p;
        while (p < text.size() && is_space(text[p]))
            ++p;
        if (p == spaced)
            continue;
        const auto rest = text.substr(p);
        if (rest.starts_with("true"))
            p += 4;
        else if (rest.starts_with("false"))
            p += 5;
        else
            continue;
        while (p < text.size() && is_space(text[p]) && text[p] != '\n')
            ++p;
        if (p == text.size() || text[p] == '\n')
            return true;
    }
    return false;
}

/**
 * @brief ^\*+ Domain groups\s*$, multi-line.
 */
bool declares_domain_groups(std::string_view text) {
    for (const auto line : split(text, '\n')) {
        const auto stars = line.find_first_not_of('*');
        if (stars == 0 || stars == std::string_view::npos)
            continue;
        const auto rest = line.substr(stars);
        if (rest.starts_with(" Domain groups") &&
            trim(rest.substr(14)).empty())
            return true;
    }
    return false;
}

/**
 * @brief _require_economic_component: a digest component must be a known
 * entity whose text declares :economic: on a column or * Domain groups.
 */
void require_economic_component(load_context& c,
    const std::vector<entity_org>& orgs, std::string_view where,
    std::string_view component) {
    const auto* e = by_singular(orgs, component);
    if (e == nullptr)
        throw model_error("* Digest/" + std::string(where) + " names " +
            std::string(component) + ", which no entity model declares");
    const auto d = c.find(e->path);
    const auto text = d ? d->source : std::string_view();
    if (!declares_economic(text) && !declares_domain_groups(text))
        throw model_error("* Digest/" + std::string(where) + " names " +
            std::string(component) + ", whose model declares no ':economic:'");
}

struct market {
    std::string_view code;
    bool is_home = false;
};

/**
 * @brief One row of * Trade types, with its * Descriptions and * Classes.
 */
struct trade_type {
    std::string_view code;
    std::string name_sql;
    std::string family;
    std::string construction;
    std::vector<market> markets;
    std::string has_options;
    std::string has_extension;
    std::string instrument;
    std::string description_sql;
    std::string source_sql;
    std::string source_ref_sql;
    std::string deal_class;
};

std::vector<trade_type> trade_types(const outline& o,
    const std::set<std::string, std::less<>>& known) {
    const auto n = section(o, node_id{0}, "Trade types");
    if (n.is_none() || tables_of(o, n).empty())
        throw model_error("no * Trade types table");
    std::vector<trade_type> out;
    for (const auto& r : first_table_rows(o, n)) {
        trade_type t;
        t.code = trim(r.value_or("code"));
        if (t.code.empty())
            continue;
        t.family = trim(r.value_or("family"));
        if (!t.family.empty() && !one_of(product_families, t.family))
            throw model_error("product type " + std::string(t.code) +
                " has an unknown family");
        t.construction = trim(r.value_or("construction"));
        if (!one_of(product_constructions, t.construction))
            throw model_error("product type " + std::string(t.code) +
                " has an unknown construction");
        std::size_t homes = 0;
        for (const auto entry : split(r.value_or("markets"), ',')) {
            const auto m = trim(entry);
            if (m.empty())
                continue;
            const bool home = m.ends_with('!');
            homes += home ? 1 : 0;
            t.markets.push_back({home ? m.substr(0, m.size() - 1) : m, home});
        }
        if (!t.markets.empty() && homes != 1)
            throw model_error("product type " + std::string(t.code) +
                " must declare exactly one home market");
        for (auto [key, flag] : {std::pair{"has_options", &t.has_options},
                 std::pair{"has_extension", &t.has_extension}}) {
            *flag = trim(r.value_or(key));
            if (*flag != "true" && *flag != "false")
                throw model_error("trade type " + std::string(t.code) + " has " +
                    key + " that is neither true nor false");
        }
        t.instrument = trim(r.value_or("instrument"));
        if (!t.instrument.empty() && !known.contains(t.instrument))
            throw model_error("trade type " + std::string(t.code) +
                " is routed to " + t.instrument +
                ", which no entity model declares");
        t.name_sql = sql_text(r.value_or("name"));
        out.push_back(std::move(t));
    }
    std::set<std::string_view> codes;
    for (const auto& t : out) {
        if (!codes.insert(t.code).second)
            throw model_error("duplicate trade type code " +
                std::string(t.code));
    }
    return out;
}

/**
 * @brief _trade_type_descriptions and _trade_type_classes, onto @p rows.
 */
void describe(const outline& o, std::vector<trade_type>& rows) {
    const auto find_row = [&](std::string_view code) {
        return std::ranges::find(rows, code, &trade_type::code);
    };
    const auto descriptions = section(o, node_id{0}, "Descriptions");
    if (descriptions.is_none() || tables_of(o, descriptions).empty())
        throw model_error("no * Descriptions table");
    std::set<std::string_view> described;
    for (const auto& r : first_table_rows(o, descriptions)) {
        const auto code = trim(r.value_or("code"));
        if (code.empty())
            continue;
        const auto row = find_row(code);
        if (row == rows.end() || !described.insert(code).second)
            throw model_error("product type " + std::string(code) +
                " is described twice or not stated");
        row->description_sql = sql_text(r.value_or("description"));
        row->source_sql = sql_text(r.value_or("source"));
        row->source_ref_sql = sql_text(r.value_or("source_ref"));
        if (row->description_sql.empty() || row->source_sql.empty() ||
            row->source_ref_sql.empty())
            throw model_error("product type " + std::string(code) +
                " has a blank * Descriptions cell");
    }
    if (described.size() != rows.size())
        throw model_error("a product type has no description");

    const auto classes = section(o, node_id{0}, "Classes");
    if (classes.is_none() || tables_of(o, classes).empty())
        throw model_error("no * Classes table");
    for (auto& row : rows)
        row.deal_class = "non_flow";
    std::set<std::string_view> classified;
    for (const auto& r : first_table_rows(o, classes)) {
        const auto code = trim(r.value_or("code"));
        if (code.empty())
            continue;
        const auto deal_class = trim(r.value_or("deal_class"));
        if (deal_class != "flow" && deal_class != "non_flow")
            throw model_error("product type " + std::string(code) +
                " states a class that is neither flow nor non_flow");
        const auto row = find_row(code);
        if (row == rows.end() || !classified.insert(code).second)
            throw model_error("product type " + std::string(code) +
                " is classified twice or not stated");
        row->deal_class = deal_class;
    }
}

/**
 * @brief An instrument and the codes routed to it, in row order.
 */
struct instrument {
    std::string name;
    std::vector<std::string_view> codes;
};

std::vector<instrument> instruments_of(const std::vector<trade_type>& rows) {
    std::set<std::string> names;
    for (const auto& r : rows) {
        if (!r.instrument.empty())
            names.insert(r.instrument);
    }
    std::vector<instrument> out;
    for (const auto& name : names) {
        auto& i = out.emplace_back(instrument{name, {}});
        for (const auto& r : rows) {
            if (r.instrument == name)
                i.codes.push_back(r.code);
        }
    }
    return out;
}

void write_rows(builder& b, value_id catalogue,
    const std::vector<trade_type>& rows) {
    const auto list = b.list(catalogue, "rows");
    std::vector<value_id> entries;
    for (const auto& r : rows) {
        const auto o = b.append_object(list);
        entries.push_back(o);
        b.set(o, "code", r.code);
        b.set(o, "name_sql", r.name_sql);
        b.set(o, "description_sql", r.description_sql);
        b.set(o, "family", r.family);
        b.set(o, "family_literal",
            r.family.empty() ? "null" : "'" + r.family + "'");
        b.set(o, "construction", r.construction);
        b.set(o, "instrument_literal",
            r.instrument.empty() ? "null" : "'" + r.instrument + "'");
        b.set_integer(o, "display_order",
            static_cast<std::int64_t>(entries.size()));
        const auto markets = b.list(o, "markets");
        for (const auto& m : r.markets) {
            const auto e = b.append_object(markets);
            b.set(e, "code", m.code);
            b.set(e, "is_home", m.is_home);
        }
        b.set(o, "has_options", r.has_options);
        b.set(o, "has_extension", r.has_extension);
        b.set(o, "instrument", r.instrument);
        b.set(o, "source_sql", r.source_sql);
        b.set(o, "source_ref_sql", r.source_ref_sql);
        b.set(o, "deal_class", r.deal_class);
        b.set(o, "deal_class_literal", "'" + r.deal_class + "'");
    }
    set_commas(b, entries);
}

void write_instruments(builder& b, value_id catalogue,
    const std::vector<instrument>& instruments) {
    const auto list = b.list(catalogue, "instruments");
    std::vector<value_id> entries;
    for (const auto& i : instruments) {
        const auto o = b.append_object(list);
        entries.push_back(o);
        b.set(o, "name", i.name);
        const auto codes = b.list(o, "codes");
        for (const auto code : i.codes)
            b.append(codes, code);
    }
    set_commas(b, entries);
}

/**
 * @brief The (instrument, cpp_type) rows of * Batch/Instrument types and
 * the entities of * Batch/Children.
 */
struct batch_section {
    std::vector<std::pair<std::string_view, std::string_view>> overrides;
    std::vector<std::string_view> children;
};

batch_section batch_section_of(const outline& o, node_id batch) {
    batch_section out;
    for (const auto& r : first_table_rows(o, section(o, batch,
             "Instrument types"))) {
        const auto instrument = trim(r.value_or("instrument"));
        const auto cpp_type = trim(r.value_or("cpp_type"));
        if (instrument.empty() && cpp_type.empty())
            continue;
        if (instrument.empty() || cpp_type.empty())
            throw model_error("a * Batch/Instrument types row needs both an "
                "instrument and a cpp_type");
        const auto it = std::ranges::find(out.overrides, instrument,
            &decltype(out.overrides)::value_type::first);
        if (it == out.overrides.end())
            out.overrides.emplace_back(instrument, cpp_type);
        else
            it->second = cpp_type;
    }
    for (const auto& r : first_table_rows(o, section(o, batch, "Children"))) {
        const auto child = trim(r.value_or("child"));
        if (!child.empty())
            out.children.push_back(child);
    }
    return out;
}

/**
 * @brief _trade_type_batch: one member per routed instrument, then per
 * child.
 */
void write_batch(builder& b, value_id catalogue, const outline& o,
    const std::vector<instrument>& instruments,
    const std::vector<entity_org>& orgs) {
    const auto list = b.list(catalogue, "batch");
    const auto n = section(o, node_id{0}, "Batch");
    if (n.is_none())
        return;
    const auto declared = batch_section_of(o, n);
    for (const auto& [name, type] : declared.overrides) {
        if (std::ranges::find(instruments, name, &instrument::name) ==
            instruments.end())
            throw model_error("* Batch/Instrument types overrides " +
                std::string(name) + ", which no routed trade type names");
    }
    struct member {
        std::string_view entity;
        std::string_view cpp_type;
        bool routed;
    };
    std::vector<member> members;
    for (const auto& i : instruments) {
        const auto it = std::ranges::find(declared.overrides, i.name,
            &decltype(declared.overrides)::value_type::first);
        members.push_back({i.name, it == declared.overrides.end()
            ? std::string_view(i.name) : it->second, true});
    }
    for (const auto child : declared.children) {
        if (by_singular(orgs, child) == nullptr)
            throw model_error("* Batch/Children names " + std::string(child) +
                ", which no entity model declares");
        members.push_back({child, child, false});
    }
    std::set<std::string_view> names;
    std::vector<value_id> entries;
    for (const auto& m : members) {
        const auto& plural = by_singular(orgs, m.entity)->plural;
        if (!names.insert(plural).second)
            throw model_error("duplicate * Batch member " + plural);
        const auto e = b.append_object(list);
        entries.push_back(e);
        b.set(e, "entity", m.entity);
        b.set(e, "name", plural);
        b.set(e, "cpp_type", m.cpp_type);
        b.set(e, "routed", m.routed);
        b.set(e, "ts_pascal", pascal_case(m.cpp_type));
        b.set(e, "ts_module", "./domain/" + std::string(m.cpp_type) + ".js");
    }
    set_commas(b, entries);
}

/**
 * @brief _trade_type_digest: the components each routed instrument's
 * digest folds and the shared components every digest folds after them.
 */
void write_digest(load_context& c, value_id catalogue, const outline& o,
    const std::vector<instrument>& instruments,
    const std::vector<entity_org>& orgs) {
    auto& b = c.b;
    const auto digest = b.list(catalogue, "digest");
    const auto shared = b.list(catalogue, "digest_shared");
    const auto n = section(o, node_id{0}, "Digest");
    if (n.is_none())
        return;
    const auto components = section(o, n, "Instrument components");
    if (components.is_none() || tables_of(o, components).empty())
        throw model_error("* Digest has no Instrument components table");
    std::vector<std::pair<std::string_view, std::vector<std::string_view>>>
        by_instrument;
    for (const auto& r : first_table_rows(o, components)) {
        const auto name = trim(r.value_or("instrument"));
        const auto component = trim(r.value_or("component"));
        if (name.empty() && component.empty())
            continue;
        if (name.empty() || component.empty())
            throw model_error("a * Digest/Instrument components row needs "
                "both an instrument and a component");
        require_economic_component(c, orgs, "Instrument components",
            component);
        auto it = std::ranges::find(by_instrument, name,
            &decltype(by_instrument)::value_type::first);
        if (it == by_instrument.end())
            it = by_instrument.insert(by_instrument.end(), {name, {}});
        if (std::ranges::find(it->second, component) != it->second.end())
            throw model_error(std::string(component) + " is listed twice "
                "under " + std::string(name));
        it->second.push_back(component);
    }
    for (const auto& [name, listed] : by_instrument) {
        if (std::ranges::find(instruments, name, &instrument::name) ==
            instruments.end())
            throw model_error("* Digest/Instrument components lists " +
                std::string(name) + ", which no routed trade type names");
    }
    for (const auto& i : instruments) {
        const auto it = std::ranges::find(by_instrument, i.name,
            &decltype(by_instrument)::value_type::first);
        if (it == by_instrument.end())
            throw model_error("* Digest/Instrument components omits " +
                i.name);
        const auto e = b.append_object(digest);
        b.set(e, "instrument", i.name);
        const auto list = b.list(e, "components");
        for (const auto component : it->second)
            b.set(b.append_object(list), "entity", component);
    }

    std::vector<std::string_view> seen;
    for (const auto& r : first_table_rows(o, section(o, n,
             "Shared components"))) {
        const auto component = trim(r.value_or("component"));
        const auto reader = trim(r.value_or("reader"));
        if (component.empty() && reader.empty())
            continue;
        const bool identifier = !reader.empty() &&
            !(reader[0] >= '0' && reader[0] <= '9') &&
            std::ranges::all_of(reader, [](char ch) {
                return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                    (ch >= '0' && ch <= '9') || ch == '_';
            });
        if (component.empty() || !identifier)
            throw model_error("a * Digest/Shared components row needs a "
                "component and a reader function name");
        require_economic_component(c, orgs, "Shared components", component);
        if (std::ranges::find(seen, component) != seen.end())
            throw model_error(std::string(component) + " is listed twice "
                "under * Digest/Shared components");
        seen.push_back(component);
        const auto e = b.append_object(shared);
        b.set(e, "entity", component);
        b.set(e, "reader", reader);
    }
}

/**
 * @brief Where a common date is read from: a table and a column, and for
 * a mapped one the entity column that states it.
 */
struct date_source {
    std::string table;
    std::string column;
    std::string where;
};

using date_sources = std::vector<std::pair<std::string, date_source>>;

const date_source* source_of(const date_sources& sources,
    std::string_view name) {
    const auto it = std::ranges::find(sources, name,
        &date_sources::value_type::first);
    return it == sources.end() ? nullptr : &it->second;
}

/**
 * @brief _trade_type_common_dates: the vocabulary and one view per routed
 * header that maps a date.
 */
void write_common_dates(load_context& c, value_id catalogue,
    const outline& o, std::string_view component,
    const std::vector<instrument>& instruments,
    const std::vector<entity_org>& orgs) {
    auto& b = c.b;
    const auto out = b.object(catalogue, "common_dates");
    const auto names_list = b.list(out, "names");
    const auto views_list = b.list(out, "views");
    const auto n = section(o, node_id{0}, "Common dates");
    if (n.is_none())
        return;

    std::vector<std::pair<std::string_view, std::string>> names;
    date_sources shared;
    for (const auto& r : first_table_rows(o, n)) {
        const auto name = trim(r.value_or("name"));
        if (name.empty())
            continue;
        if (std::ranges::find(names, name,
                &decltype(names)::value_type::first) != names.end())
            throw model_error("duplicate common date " + std::string(name));
        names.emplace_back(name, collapse_spaces(r.value_or("description")));
        const auto entity = trim(r.value_or("shared_entity"));
        const auto column = trim(r.value_or("shared_column"));
        if (entity.empty() && column.empty())
            continue;
        const auto* e = by_singular(orgs, entity);
        if (e == nullptr)
            throw model_error("common date " + std::string(name) +
                " is shared from an entity no model declares");
        const auto columns = entity_columns(read(c, e->path).o);
        if (std::ranges::find(columns, column, &declared_column::name) ==
            columns.end())
            throw model_error("common date " + std::string(name) +
                " is shared from a column the entity does not declare");
        if (!keyed_by_trade_id_alone(columns))
            throw model_error("common date " + std::string(name) +
                " is shared from an entity not keyed by the trade alone");
        const auto it = std::ranges::find(shared, name,
            &date_sources::value_type::first);
        date_source s{e->table, std::string(column), {}};
        if (it == shared.end())
            shared.emplace_back(std::string(name), std::move(s));
        else
            it->second = std::move(s);
    }

    for (const auto& i : instruments) {
        const auto* header = by_singular(orgs, i.name);
        const auto header_doc = read(c, header->path);
        std::vector<const entity_org*> members{header};
        for (const auto child : children(header_doc.o,
                 section(header_doc.o, node_id{0}, "Delete cascade"))) {
            const auto* e = by_table(orgs,
                trim(property(header_doc.o, child, "table").value));
            if (e != nullptr)
                members.push_back(e);
        }
        date_sources mapped;
        for (const auto* member : members) {
            const auto columns = entity_columns(read(c, member->path).o);
            for (const auto& column : columns) {
                const auto declared = trim(column_property(column,
                    "common_date"));
                if (declared.empty())
                    continue;
                auto where = member->singular + "." + std::string(column.name);
                if (std::ranges::find(names, declared,
                        &decltype(names)::value_type::first) == names.end())
                    throw model_error(where + " declares :common_date: " +
                        std::string(declared) + ", which is not listed");
                if (source_of(shared, declared) != nullptr)
                    throw model_error(where + " declares a shared date");
                if (!keyed_by_trade_id_alone(columns))
                    throw model_error(where + " is not keyed by the trade "
                        "alone");
                if (source_of(mapped, declared) != nullptr)
                    throw model_error(i.name + ": " + std::string(declared) +
                        " is stated twice");
                mapped.emplace_back(std::string(declared), date_source{
                    member->table, std::string(column.name), std::move(where)});
            }
        }
        if (mapped.empty())
            continue;

        std::vector<std::pair<std::string, std::string>> aliases{
            {header->table, "h"}};
        const auto view = b.append_object(views_list);
        b.set(view, "view", "ores_" + std::string(component) + "_" +
            header->plural + "_common_dates_vw");
        b.set(view, "header_table", header->table);
        b.set(view, "entity", i.name);
        const auto joins = b.list(view, "joins");
        const auto alias_of = [&](const std::string& table) {
            return std::ranges::find(aliases, table,
                &decltype(aliases)::value_type::first);
        };
        std::size_t joined = 0;
        for (const auto* sources : {&mapped, &shared}) {
            for (const auto& [name, s] : *sources) {
                if (alias_of(s.table) != aliases.end())
                    continue;
                const auto alias = "j" + std::to_string(++joined);
                aliases.emplace_back(s.table, alias);
                const auto j = b.append_object(joins);
                b.set(j, "table", s.table);
                b.set(j, "alias", alias);
            }
        }
        const auto columns = b.list(view, "columns");
        std::vector<value_id> entries;
        for (const auto& [name, description] : names) {
            const auto* s = source_of(mapped, name);
            if (s == nullptr)
                s = source_of(shared, name);
            const auto e = b.append_object(columns);
            entries.push_back(e);
            b.set(e, "name", name);
            b.set(e, "expr", s == nullptr ? std::string("null::date")
                : alias_of(s->table)->second + "." + s->column);
        }
        set_commas(b, entries);
        const auto list = b.list(view, "mapped");
        for (const auto& [name, s] : mapped) {
            const auto e = b.append_object(list);
            b.set(e, "name", name);
            b.set(e, "entity_column", s.where);
        }
    }

    for (const auto& [name, description] : names) {
        const auto e = b.append_object(names_list);
        b.set(e, "name", name);
        b.set(e, "description", description);
        b.set(e, "shared", source_of(shared, name) != nullptr);
    }
}

/**
 * @brief _trade_type_unsupported: the types ORE cannot carry, each with
 * the reason a refusal gives.
 */
void write_unsupported(builder& b, value_id catalogue, const outline& o,
    const std::vector<trade_type>& rows) {
    const auto list = b.list(catalogue, "unsupported");
    const auto n = section(o, node_id{0}, "Unsupported trade types");
    if (n.is_none() || tables_of(o, n).empty())
        return;
    std::vector<std::string_view> seen;
    for (const auto& r : first_table_rows(o, n)) {
        const auto code = trim(r.value_or("code"));
        if (code.empty())
            continue;
        const auto reason = trim(r.value_or("reason"));
        const auto row = std::ranges::find(rows, code, &trade_type::code);
        if (row == rows.end() || !row->instrument.empty())
            throw model_error("unsupported trade type " + std::string(code) +
                " must be a stated type that routes to no table");
        if (reason.empty() || reason.contains('"') || reason.contains('\\'))
            throw model_error("unsupported trade type " + std::string(code) +
                " needs a reason with no quote or backslash");
        if (std::ranges::find(seen, code) != seen.end())
            throw model_error("unsupported trade type " + std::string(code) +
                " is stated twice");
        seen.push_back(code);
        const auto e = b.append_object(list);
        b.set(e, "code", code);
        b.set(e, "reason", reason);
    }
}

}

void load_trade_type_catalogue(load_context& c, value_id root) {
    auto& b = c.b;
    const auto& o = c.o;
    const auto orgs = entity_orgs(c);
    std::set<std::string, std::less<>> known;
    for (const auto& e : orgs)
        known.insert(e.singular);
    auto rows = trade_types(o, known);
    describe(o, rows);
    const auto instruments = instruments_of(rows);

    const auto catalogue = b.object(root, "trade_type_catalogue");
    for (const auto key : {"component", "brief"}) {
        if (const auto r = frontmatter(o, key); r.found)
            b.set(catalogue, key, r.value);
    }
    write_rows(b, catalogue, rows);
    write_instruments(b, catalogue, instruments);
    write_batch(b, catalogue, o, instruments, orgs);
    write_digest(c, catalogue, o, instruments, orgs);
    write_common_dates(c, catalogue, o, frontmatter_or(o, "component"),
        instruments, orgs);
    const auto routes = b.list(catalogue, "routes");
    for (const auto& r : rows) {
        if (r.instrument.empty())
            continue;
        const auto e = b.append_object(routes);
        b.set(e, "code", r.code);
        b.set(e, "instrument", r.instrument);
    }
    write_unsupported(b, catalogue, o, rows);
    const auto market_rows = b.list(catalogue, "market_rows");
    std::vector<value_id> entries;
    for (const auto& r : rows) {
        for (const auto& m : r.markets) {
            const auto e = b.append_object(market_rows);
            entries.push_back(e);
            b.set(e, "product_type_code", r.code);
            b.set(e, "asset_class_code", m.code);
            b.set(e, "is_home", m.is_home ? "true" : "false");
        }
    }
    set_commas(b, entries);
}

}
